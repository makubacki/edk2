/** @file
  Implement all four UEFI Runtime Variable services for the nonvolatile
  and volatile storage space and install variable architecture protocol.

Copyright (C) 2013, Red Hat, Inc.
Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
(C) Copyright 2015 Hewlett Packard Enterprise Development LP<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Variable.h"
#include "VariableHelpers.h"
#include "VariableStorage.h"
#include "VariableVolatile.h"
#include "VariableVolatileCommon.h"

extern VARIABLE_STORE_HEADER        *mNvVariableCache;
extern VARIABLE_INFO_ENTRY          *gVariableInfo;
extern BOOLEAN                      mCommandInProgress;
EFI_HANDLE                          mHandle                      = NULL;
EFI_EVENT                           mVirtualAddressChangeEvent   = NULL;
extern BOOLEAN                      mEndOfDxe;
VOID                                ***mVarCheckAddressPointer   = NULL;
UINTN                               mVarCheckAddressPointerCount = 0;
EDKII_VARIABLE_LOCK_PROTOCOL        mVariableLock                = { VariableLockRequestToLock };
EDKII_VAR_CHECK_PROTOCOL            mVarCheck                    = { VarCheckRegisterSetVariableCheckHandler,
                                                                     VarCheckVariablePropertySet,
                                                                     VarCheckVariablePropertyGet };

/**
  Some Secure Boot Policy Variable may update following other variable changes(SecureBoot follows PK change, etc).
  Record their initial State when variable write service is ready.

**/
VOID
EFIAPI
RecordSecureBootPolicyVarData(
  VOID
  );

/**
  Notifies the core variable driver that the Variable Storage Driver has completed the previously requested
  SMM phase asyncronous I/O. If the operation was a read, the data is now present in the NV storage cache.

  This function will only be used by the SMM implementation of UEFI Variables

  @param[in]  VariableStorageInstanceGuid   The instance GUID of the variable storage protocol invoking this function
  @param[in]  Status                        The result of the variable I/O operation. Possible values are:
                            EFI_SUCCESS            The variable I/O completed successfully.
                            EFI_NOT_FOUND          The variable was not found.
                            EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the variable and its data.
                            EFI_DEVICE_ERROR       The variable I/O could not complete due to a hardware error.
                            EFI_WRITE_PROTECTED    The variable in question is read-only.
                            EFI_WRITE_PROTECTED    The variable in question cannot be deleted.
                            EFI_SECURITY_VIOLATION The variable I/O could not complete due to an authentication failure.

**/
VOID
EFIAPI
VariableStorageSupportNotifySmmIoComplete (
  IN      EFI_GUID                    *VariableStorageInstanceGuid,
  IN      EFI_STATUS                  Status
  )
{
  DEBUG ((DEBUG_ERROR, "NotifySmmIoComplete() should never be called on DXE implementation of UEFI Variables.\n"));
  ASSERT (FALSE);
}

EDKII_VARIABLE_STORAGE_SUPPORT_PROTOCOL mVariableStorageSupport = {
  VariableStorageSupportNotifyWriteServiceReady,
  VariableStorageSupportNotifySmmIoComplete,
  VariableStorageSupportUpdateNvCache
};

/**

  This code finds variable in storage blocks (Volatile or Non-Volatile).

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode, and datasize is external input.
  This function will do basic validation, before parse the data.

  @param VariableName               Name of Variable to be found.
  @param VendorGuid                 Variable vendor GUID.
  @param Attributes                 Attribute value of the variable found.
  @param DataSize                   Size of Data found. If size is less than the
                                    data, this value contains the required size.
  @param Data                       Data pointer.

  @return EFI_INVALID_PARAMETER     Invalid parameter.
  @return EFI_SUCCESS               Find the specified variable.
  @return EFI_NOT_FOUND             Not found.
  @return EFI_BUFFER_TO_SMALL       DataSize is too small for the result.

**/
EFI_STATUS
EFIAPI
DxeVariableGetVariable (
  IN      CHAR16            *VariableName,
  IN      EFI_GUID          *VendorGuid,
  OUT     UINT32            *Attributes OPTIONAL,
  IN OUT  UINTN             *DataSize,
  OUT     VOID              *Data
  )
{
  EFI_STATUS    Status;
  BOOLEAN       CommandInProgress;
  EFI_GUID      InProgressNvStorageInstanceId;

  CommandInProgress = FALSE;
  Status = VariableServiceGetVariable (
             VariableName,
             VendorGuid,
             Attributes,
             DataSize,
             Data,
             &CommandInProgress,
             &InProgressNvStorageInstanceId
             );
  ASSERT (CommandInProgress == FALSE);
  return Status;
}

/**

  This code sets variable in storage blocks (Volatile or Non-Volatile).

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode, and datasize and data are external input.
  This function will do basic validation, before parse the data.
  This function will parse the authentication carefully to avoid security issues, like
  buffer overflow, integer overflow.
  This function will check attribute carefully to avoid authentication bypass.

  @param VariableName                     Name of Variable to be found.
  @param VendorGuid                       Variable vendor GUID.
  @param Attributes                       Attribute value of the variable found
  @param DataSize                         Size of Data found. If size is less than the
                                          data, this value contains the required size.
  @param Data                             Data pointer.

  @return EFI_INVALID_PARAMETER           Invalid parameter.
  @return EFI_SUCCESS                     Set successfully.
  @return EFI_OUT_OF_RESOURCES            Resource not enough to set variable.
  @return EFI_NOT_FOUND                   Not found.
  @return EFI_WRITE_PROTECTED             Variable is read-only.

**/
EFI_STATUS
EFIAPI
DxeVariableSetVariable (
  IN CHAR16                  *VariableName,
  IN EFI_GUID                *VendorGuid,
  IN UINT32                  Attributes,
  IN UINTN                   DataSize,
  IN VOID                    *Data
  )
{
  EFI_STATUS    Status;
  BOOLEAN       CommandInProgress;
  BOOLEAN       ReenterFunction;
  EFI_GUID      InProgressNvStorageInstanceId;

  CommandInProgress = FALSE;
  Status = VariableServiceSetVariable (
             VariableName,
             VendorGuid,
             Attributes,
             DataSize,
             Data,
             &CommandInProgress,
             &InProgressNvStorageInstanceId,
             &ReenterFunction
             );
  ASSERT (!CommandInProgress);
  ASSERT (!ReenterFunction);
  return Status;
}

/**
  Return TRUE if ExitBootServices () has been called.

  @retval TRUE If ExitBootServices () has been called.
**/
BOOLEAN
AtRuntime (
  VOID
  )
{
  return EfiAtRuntime ();
}


/**
  Initializes a basic mutual exclusion lock.

  This function initializes a basic mutual exclusion lock to the released state
  and returns the lock.  Each lock provides mutual exclusion access at its task
  priority level.  Since there is no preemption or multiprocessor support in EFI,
  acquiring the lock only consists of raising to the locks TPL.
  If Lock is NULL, then ASSERT().
  If Priority is not a valid TPL value, then ASSERT().

  @param  Lock       A pointer to the lock data structure to initialize.
  @param  Priority   EFI TPL is associated with the lock.

  @return The lock.

**/
EFI_LOCK *
InitializeLock (
  IN OUT EFI_LOCK                         *Lock,
  IN     EFI_TPL                          Priority
  )
{
  return EfiInitializeLock (Lock, Priority);
}

/**
  Acquires lock only at boot time. Simply returns at runtime.

  This is a temperary function that will be removed when
  EfiAcquireLock() in UefiLib can handle the call in UEFI
  Runtimer driver in RT phase.
  It calls EfiAcquireLock() at boot time, and simply returns
  at runtime.

  @param  Lock         A pointer to the lock to acquire.

**/
VOID
AcquireLockOnlyAtBootTime (
  IN EFI_LOCK                             *Lock
  )
{
  if (!AtRuntime ()) {
    EfiAcquireLock (Lock);
  }
}


/**
  Releases lock only at boot time. Simply returns at runtime.

  This is a temperary function which will be removed when
  EfiReleaseLock() in UefiLib can handle the call in UEFI
  Runtimer driver in RT phase.
  It calls EfiReleaseLock() at boot time and simply returns
  at runtime.

  @param  Lock         A pointer to the lock to release.

**/
VOID
ReleaseLockOnlyAtBootTime (
  IN EFI_LOCK                             *Lock
  )
{
  if (!AtRuntime ()) {
    EfiReleaseLock (Lock);
  }
}

/**
  Retrive the FVB protocol interface by HANDLE.
  Notification function of EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE.

  This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
  It convers pointer to new virtual address.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
VariableClassAddressChangeEvent (
  IN EFI_EVENT                            Event,
  IN VOID                                 *Context
  )
{
  UINTN                             Index;
  EDKII_VARIABLE_STORAGE_PROTOCOL   *VariableStorageProtocol;

  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->GetId);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->GetVariable);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->GetAuthenticatedVariable);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->GetNextVariableName);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->GetStorageUsage);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->GetAuthenticatedSupport);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->SetVariable);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->WriteServiceIsReady);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->GarbageCollect);
    EfiConvertPointer (0x0, (VOID **) &VariableStorageProtocol->AsyncIoRequired);
    EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->VariableGlobal.VariableStores[Index]);
  }
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->VariableGlobal.VariableStores);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->PlatformLangCodes);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->LangCodes);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->PlatformLang);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->VariableGlobal.VolatileVariableBase);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->VariableGlobal.HobVariableBase);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal);
  EfiConvertPointer (0x0, (VOID **) &mNvVariableCache);

  if (mAuthContextOut.AddressPointer != NULL) {
    for (Index = 0; Index < mAuthContextOut.AddressPointerCount; Index++) {
      EfiConvertPointer (0x0, (VOID **) mAuthContextOut.AddressPointer[Index]);
    }
  }

  if (mVarCheckAddressPointer != NULL) {
    for (Index = 0; Index < mVarCheckAddressPointerCount; Index++) {
      EfiConvertPointer (0x0, (VOID **) mVarCheckAddressPointer[Index]);
    }
  }
}


/**
  Notification function of EVT_GROUP_READY_TO_BOOT event group.

  This is a notification function registered on EVT_GROUP_READY_TO_BOOT event group.
  When the Boot Manager is about to load and execute a boot option, it reclaims variable
  storage if free size is below the threshold.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
OnReadyToBoot (
  EFI_EVENT                               Event,
  VOID                                    *Context
  )
{
  if (!mEndOfDxe) {
    MorLockInitAtEndOfDxe ();
    //
    // Set the End Of DXE bit in case the EFI_END_OF_DXE_EVENT_GROUP_GUID event is not signaled.
    //
    mEndOfDxe = TRUE;
    mVarCheckAddressPointer = VarCheckLibInitializeAtEndOfDxe (&mVarCheckAddressPointerCount);
    //
    // The initialization for variable quota.
    //
    InitializeVariableQuota ();
  }
  ReclaimForOS ();
  if (FeaturePcdGet (PcdVariableCollectStatistics)) {
    if (mVariableModuleGlobal->VariableGlobal.AuthFormat) {
      gBS->InstallConfigurationTable (&gEfiAuthenticatedVariableGuid, gVariableInfo);
    } else {
      gBS->InstallConfigurationTable (&gEfiVariableGuid, gVariableInfo);
    }
  }

  gBS->CloseEvent (Event);
}

/**
  Notification function of EFI_END_OF_DXE_EVENT_GROUP_GUID event group.

  This is a notification function registered on EFI_END_OF_DXE_EVENT_GROUP_GUID event group.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
OnEndOfDxe (
  EFI_EVENT                               Event,
  VOID                                    *Context
  )
{
  DEBUG ((EFI_D_INFO, "[Variable]END_OF_DXE is signaled\n"));
  MorLockInitAtEndOfDxe ();
  mEndOfDxe = TRUE;
  mVarCheckAddressPointer = VarCheckLibInitializeAtEndOfDxe (&mVarCheckAddressPointerCount);
  //
  // The initialization for variable quota.
  //
  InitializeVariableQuota ();
  if (PcdGetBool (PcdReclaimVariableSpaceAtEndOfDxe)) {
    ReclaimForOS ();
  }

  gBS->CloseEvent (Event);
}

/**
  Non-Volatile variable write service is ready event handler.

**/
VOID
EFIAPI
InstallVariableWriteReady (
  VOID
  )
{
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "Installing EfiVariableWriteArchProtocolGuid\n"));

  //
  // Some Secure Boot Policy Var (SecureBoot, etc) updates following other
  // Secure Boot Policy Variable change. Record their initial value.
  //
  RecordSecureBootPolicyVarData ();

  //
  // Install the Variable Write Architectural protocol.
  //
  Status = gBS->InstallProtocolInterface (
                  &mHandle,
                  &gEfiVariableWriteArchProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
}

/**
  Common variable initialization for DXE variable support.

  @retval EFI_SUCCESS    DXE variable initialization ran successfully

**/
EFI_STATUS
EFIAPI
VariableDxeCommonInitialize (
  VOID
  )
{
  EFI_STATUS   Status;
  EFI_EVENT    ReadyToBootEvent;

  //
  // Initialize variable services
  //
  Status = VariableCommonInitialize ();
  ASSERT_EFI_ERROR (Status);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mHandle,
                  &gEdkiiVariableLockProtocolGuid,
                  &mVariableLock,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mHandle,
                  &gEdkiiVarCheckProtocolGuid,
                  &mVarCheck,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mHandle,
                  &gEdkiiVariableStorageSupportProtocolGuid,
                  &mVariableStorageSupport,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  gRT->GetVariable         = DxeVariableGetVariable;
  gRT->GetNextVariableName = VariableServiceGetNextVariableName;
  gRT->SetVariable         = DxeVariableSetVariable;
  gRT->QueryVariableInfo   = VariableServiceQueryVariableInfo;

  //
  // Now install the Variable Runtime Architectural protocol on a new handle.
  //
  Status = gBS->InstallProtocolInterface (
                  &mHandle,
                  &gEfiVariableArchProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  VariableClassAddressChangeEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mVirtualAddressChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Register the event handling function to reclaim variable for OS usage.
  //
  Status = EfiCreateEventReadyToBootEx (
             TPL_NOTIFY,
             OnReadyToBoot,
             NULL,
             &ReadyToBootEvent
             );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  DXE EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL notification event handler.

  @param[in] EFI_EVENT     which will be returned when event is created.
  @param[in] Context       Context if any.

**/
VOID
EFIAPI
DxeVariableStorageSelectorNotify (
  IN EFI_EVENT    Event,
  IN VOID         *Context
  )
{
  EFI_STATUS   Status;
  UINTN        Index;
  EFI_HANDLE   *Handles;

  Handles  = NULL;

  Status = gBS->LocateProtocol (
                    &gEdkiiVariableStorageSelectorProtocolGuid,
                    NULL,
                    (VOID **) &mVariableModuleGlobal->VariableGlobal.VariableStorageSelectorProtocol
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "VariableStorageSelectorProtocol not found.\n"));
      return;
    }

    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEdkiiVariableStorageProtocolGuid,
                    NULL,
                    &mVariableModuleGlobal->VariableGlobal.VariableStoresCount,
                    &Handles
                    );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR(Status)) {
      return;
    }
    mVariableModuleGlobal->VariableGlobal.VariableStores =
      AllocateRuntimeZeroPool (
        sizeof (EDKII_VARIABLE_STORAGE_PROTOCOL *) *
        mVariableModuleGlobal->VariableGlobal.VariableStoresCount
        );
    ASSERT (mVariableModuleGlobal->VariableGlobal.VariableStores != NULL);
    if (mVariableModuleGlobal->VariableGlobal.VariableStores == NULL) {
      FreePool (Handles);
      return;
    }
    for ( Index = 0;
          Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
          Index++) {
      Status = gBS->OpenProtocol (
                Handles[Index],
                &gEdkiiVariableStorageProtocolGuid,
                (VOID **) &mVariableModuleGlobal->VariableGlobal.VariableStores[Index],
                gImageHandle,
                NULL,
                EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );
      ASSERT_EFI_ERROR (Status);
      if (EFI_ERROR (Status)) {
        FreePool (Handles);
        return;
      }
    }
    FreePool (Handles);

    Status = VariableDxeCommonInitialize ();
    ASSERT_EFI_ERROR (Status);
}

/**
  Flush the HOB variable to storage.

  @param[in] VariableName       Name of variable has been updated or deleted.
  @param[in] VendorGuid         Guid of variable has been updated or deleted.
  @param[in] Callback           An optional callback function with activities that should occur after HOB flush.

**/
VOID
FlushHobVariableToStorage (
  IN CHAR16                               *VariableName,
  IN EFI_GUID                             *VendorGuid,
  IN VARIABLE_HOB_FLUSH_COMPLETE_CALLBACK Callback      OPTIONAL
  )
{
  EFI_STATUS                    Status;
  VARIABLE_STORE_HEADER         *VariableStoreHeader;
  VARIABLE_HEADER               *Variable;
  VOID                          *VariableData;
  VARIABLE_POINTER_TRACK        VariablePtrTrack;
  BOOLEAN                       ErrorFlag;
  BOOLEAN                       CommandInProgress;
  BOOLEAN                       AsyncIoRequired;
  EFI_GUID                      InProgressInstanceGuid;

  ErrorFlag         = FALSE;
  CommandInProgress = FALSE;

  //
  // Flush the HOB variable to storage.
  //
  if (mVariableModuleGlobal->VariableGlobal.HobVariableBase != 0) {
    AsyncIoRequired = VariableStorageAnyAsyncIoRequired ();
    if (AsyncIoRequired && (VendorGuid == NULL) && (VariableName == NULL)) {
      return;
    }
    VariableStoreHeader = (VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.HobVariableBase;
    //
    // Set HobVariableBase to 0, it can avoid SetVariable to call back.
    //
    mVariableModuleGlobal->VariableGlobal.HobVariableBase = 0;
    for ( Variable = GetStartPointer (VariableStoreHeader)
        ; IsValidVariableHeader (Variable, GetEndPointer (VariableStoreHeader))
        ; Variable = GetNextVariablePtr (Variable)
        ) {
      if (Variable->State != VAR_ADDED) {
        //
        // The HOB variable has been set to DELETED state in local.
        //
        continue;
      }
      ASSERT ((Variable->Attributes & EFI_VARIABLE_NON_VOLATILE) != 0);
      if (VendorGuid == NULL || VariableName == NULL ||
          !CompareGuid (VendorGuid, GetVendorGuidPtr (Variable)) ||
          StrCmp (VariableName, GetVariableNamePtr (Variable)) != 0) {
        if (AsyncIoRequired) {
          ErrorFlag = TRUE;
          continue;
        }
        VariableData = GetVariableDataPtr (Variable);
        FindVariable (
          GetVariableNamePtr (Variable),
          GetVendorGuidPtr (Variable),
          &VariablePtrTrack,
          &mVariableModuleGlobal->VariableGlobal,
          FALSE,
          &CommandInProgress,
          &InProgressInstanceGuid
          );
        //
        // We should never need async I/O
        //
        ASSERT (!CommandInProgress);
        if (CommandInProgress) {
          return;
        }
        Status = UpdateVariable (
                   GetVariableNamePtr (Variable),
                   GetVendorGuidPtr (Variable),
                   VariableData,
                   DataSizeOfVariable (Variable),
                   Variable->Attributes,
                   0,
                   0,
                   &VariablePtrTrack,
                   NULL
                 );
        //
        // We should never need async I/O
        //
        ASSERT (!mCommandInProgress);
        if (mCommandInProgress) {
          return;
        }
        DEBUG ((EFI_D_INFO, "  Variable Driver: Flushed the HOB variable to storage: %g %s %r\n", GetVendorGuidPtr (Variable), GetVariableNamePtr (Variable), Status));
      } else {
        //
        // The updated or deleted variable is matched with this HOB variable.
        // Don't break here because we will try to set other HOB variables
        // since this variable could be set successfully.
        //
        Status = EFI_SUCCESS;
      }
      if (!EFI_ERROR (Status)) {
        //
        // If set variable successful, or the updated or deleted variable is matched with the HOB variable,
        // set the HOB variable to DELETED state in local.
        //
        DEBUG ((EFI_D_INFO, "  Variable Driver: Set the HOB variable to DELETED state in local: %g %s\n", GetVendorGuidPtr (Variable), GetVariableNamePtr (Variable)));
        Variable->State &= VAR_DELETED;
      } else {
        ErrorFlag = TRUE;
      }
    }
    if (ErrorFlag) {
      //
      // We still have HOB variable(s) not flushed in storage.
      //
      mVariableModuleGlobal->VariableGlobal.HobVariableBase = (EFI_PHYSICAL_ADDRESS) (UINTN) VariableStoreHeader;
    } else {
      //
      // All HOB variables have been flushed in storage.
      //
      DEBUG ((EFI_D_INFO, "  Variable Driver: All HOB variables have been flushed to storage.\n"));
      if (!AtRuntime ()) {
        FreePool ((VOID *) VariableStoreHeader);
      }
    }
    if (Callback != NULL) {
      Callback ();
    }
  }
}

/**
  Variable Driver main entry point. The Variable driver places the 4 EFI
  runtime services in the EFI System Table and installs arch protocols
  for variable read and write services being available. It also registers
  a notification function for an EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       Variable service successfully initialized.

**/
EFI_STATUS
EFIAPI
VariableServiceInitialize (
  IN EFI_HANDLE                ImageHandle,
  IN EFI_SYSTEM_TABLE          *SystemTable
  )
{
  EFI_STATUS                   Status;
  EFI_EVENT                    DxeVariableSelectorNotifyEvent;
  EFI_EVENT                    EndOfDxeEvent;
  VOID                         *DxeVariableStorageSelectorNotifyRegistration;

  //
  // Allocate runtime memory for variable driver global structure.
  //
  mVariableModuleGlobal = AllocateRuntimeZeroPool (sizeof (VARIABLE_MODULE_GLOBAL));
  if (mVariableModuleGlobal == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mVariableModuleGlobal->VariableGlobal.VariableStoresCount = 0;

  //
  // Determine if NV Emulation Mode is enabled
  //
  mNvVariableEmulationMode = PcdGetBool (PcdNvVariableEmulationMode);

  if (!mNvVariableEmulationMode) {
    Status = gBS->CreateEvent (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    DxeVariableStorageSelectorNotify,
                    NULL,
                    &DxeVariableSelectorNotifyEvent
                    );
    ASSERT_EFI_ERROR (Status);

    // Register storage selector protocol notify
    Status = gBS->RegisterProtocolNotify (
                    &gEdkiiVariableStorageSelectorProtocolGuid,
                    DxeVariableSelectorNotifyEvent,
                    &DxeVariableStorageSelectorNotifyRegistration
                    );
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }
    DxeVariableStorageSelectorNotify (NULL, NULL);
  } else {
    Status = VariableDxeCommonInitialize ();
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Register the event handling function to set the End Of DXE flag.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnEndOfDxe,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &EndOfDxeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
