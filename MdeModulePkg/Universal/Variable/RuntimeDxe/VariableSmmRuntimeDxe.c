/** @file
  Implement all four UEFI Runtime Variable services for the nonvolatile
  and volatile storage space and install variable architecture protocol
  based on SMM variable module.

  Caution: This module requires additional review when modified.
  This driver will have external input - variable data.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

  RuntimeServiceGetVariable() and RuntimeServiceSetVariable() are external API
  to receive data buffer. The size should be checked carefully.

  InitCommunicateBuffer() is really function to check the variable data size.

Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include <PiDxe.h>
#include <Protocol/VariableWrite.h>
#include <Protocol/Variable.h>
#include <Protocol/SmmCommunication.h>
#include <Protocol/SmmVariable.h>
#include <Protocol/VariableLock.h>
#include <Protocol/VarCheck.h>
#include <Protocol/VariableStorageIoCompletionProtocol.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>

#include <Guid/EventGroup.h>
#include <Guid/SmmVariableCommon.h>

#include "PrivilegePolymorphic.h"
#include "Variable.h" ///< Todo: Can remove before final merge
#include "VariableHelpers.h"
#include "VariableVolatileCommon.h"

EFI_HANDLE                                      mHandle                                   = NULL;
EFI_SMM_VARIABLE_PROTOCOL                       *mSmmVariable                             = NULL;
EFI_EVENT                                       mVirtualAddressChangeEvent                = NULL;
EFI_SMM_COMMUNICATION_PROTOCOL                  *mSmmCommunication                        = NULL;
EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   **mVariableStoreIoCompletionProtocols     = NULL;
UINTN                                           mVariableStoreIoCompletionProtocolsCount  = 0;
UINT8                                           *mVariableBuffer                          = NULL;
UINT8                                           *mVariableBufferPhysical                  = NULL;
VARIABLE_STORE_HEADER                           *mVariableRuntimeNvCacheBuffer            = NULL;
VARIABLE_STORE_HEADER                           *mVariableRuntimeVolatileCacheBuffer      = NULL;
UINTN                                           mVariableBufferSize;
UINTN                                           mVariableRuntimeNvCacheBufferSize;
UINTN                                           mVariableRuntimeVolatileCacheBufferSize;
UINTN                                           mVariableBufferPayloadSize;
BOOLEAN                                         mVariableRuntimeCachePendingUpdate;
BOOLEAN                                         mVariableRuntimeCacheReadLock;
EFI_LOCK                                        mVariableServicesLock;
EDKII_VARIABLE_LOCK_PROTOCOL                    mVariableLock;
EDKII_VAR_CHECK_PROTOCOL                        mVarCheck;
STATIC EFI_EVENT                                mVariableWriteReadyWaitEvent;

/**
  Some Secure Boot Policy Variable may update following other variable changes(SecureBoot follows PK change, etc).
  Record their initial State when variable write service is ready.

**/
VOID
EFIAPI
RecordSecureBootPolicyVarData (
  VOID
  );

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
  if (!EfiAtRuntime ()) {
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
  if (!EfiAtRuntime ()) {
    EfiReleaseLock (Lock);
  }
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
  Initialize the communicate buffer using DataSize and Function.

  The communicate size is: SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE +
  DataSize.

  Caution: This function may receive untrusted input.
  The data size external input, so this function will validate it carefully to avoid buffer overflow.

  @param[out]      DataPtr          Points to the data in the communicate buffer.
  @param[in]       DataSize         The data size to send to SMM.
  @param[in]       Function         The function number to initialize the communicate header.

  @retval EFI_INVALID_PARAMETER     The data size is too big.
  @retval EFI_SUCCESS               Find the specified variable.

**/
EFI_STATUS
InitCommunicateBuffer (
  OUT     VOID                              **DataPtr OPTIONAL,
  IN      UINTN                             DataSize,
  IN      UINTN                             Function
  )
{
  EFI_SMM_COMMUNICATE_HEADER                *SmmCommunicateHeader;
  SMM_VARIABLE_COMMUNICATE_HEADER2          *SmmVariableFunctionHeader;


  if (DataSize + SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE > mVariableBufferSize) {
    return EFI_INVALID_PARAMETER;
  }

  SmmCommunicateHeader = (EFI_SMM_COMMUNICATE_HEADER *) mVariableBuffer;
  CopyGuid (&SmmCommunicateHeader->HeaderGuid, &gEfiSmmVariableProtocolGuid);
  SmmCommunicateHeader->MessageLength = DataSize + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE;

  SmmVariableFunctionHeader = (SMM_VARIABLE_COMMUNICATE_HEADER2 *) SmmCommunicateHeader->Data;
  SmmVariableFunctionHeader->Function = Function;
  if (DataPtr != NULL) {
    *DataPtr = SmmVariableFunctionHeader->Data;
  }

  return EFI_SUCCESS;
}


/**
  Send the data in communicate buffer to SMM.

  @param[in]   DataSize               This size of the function header and the data.

  @retval      EFI_SUCCESS            Success is returned from the functin in SMM.
  @retval      Others                 Failure is returned from the function in SMM.

**/
EFI_STATUS
SendCommunicateBuffer (
  IN      UINTN                             DataSize,
  OUT     BOOLEAN                           *CommandInProgress,
  OUT     EFI_GUID                          *InProgressNvStorageInstanceId,
  OUT     BOOLEAN                           *ReenterFunction,
  OUT     BOOLEAN                           *VariableServicesInUse
  )
{
  EFI_STATUS                                Status;
  UINTN                                     CommSize;
  EFI_SMM_COMMUNICATE_HEADER                *SmmCommunicateHeader;
  SMM_VARIABLE_COMMUNICATE_HEADER2          *SmmVariableFunctionHeader;

  CommSize                  = DataSize + SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE;
  SmmCommunicateHeader      = (EFI_SMM_COMMUNICATE_HEADER *) mVariableBuffer;
  SmmVariableFunctionHeader = (SMM_VARIABLE_COMMUNICATE_HEADER2 *)SmmCommunicateHeader->Data;
  ZeroMem (&SmmVariableFunctionHeader->InProgressNvStorageInstanceId, sizeof (EFI_GUID));
  SmmVariableFunctionHeader->CommandInProgress      = FALSE;
  SmmVariableFunctionHeader->ReenterFunction        = FALSE;
  SmmVariableFunctionHeader->VariableServicesInUse  = FALSE;

  Status = mSmmCommunication->Communicate (mSmmCommunication, mVariableBufferPhysical, &CommSize);
  ASSERT_EFI_ERROR (Status);

  if (CommandInProgress != NULL) {
    *CommandInProgress = SmmVariableFunctionHeader->CommandInProgress;
  }
  if (InProgressNvStorageInstanceId != NULL) {
    CopyMem (InProgressNvStorageInstanceId, &SmmVariableFunctionHeader->InProgressNvStorageInstanceId, sizeof (EFI_GUID));
  }
  if (ReenterFunction != NULL) {
    *ReenterFunction = SmmVariableFunctionHeader->ReenterFunction;
  }
  if (VariableServicesInUse != NULL) {
    *VariableServicesInUse = SmmVariableFunctionHeader->VariableServicesInUse;
  }
  return  SmmVariableFunctionHeader->ReturnStatus;
}

/**
  Gets the EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL for the given Variable Storage Protocol Instance GUID

  @param[in]   InstanceId                           The Variable Storage Protocol Instance GUID
  @param[out]  VariableStorageIoCompletionProtocol  The found EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL

  @retval EFI_INVALID_PARAMETER       InstanceId is NULL.
  @retval EFI_SUCCESS                 EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL successfully found.
  @retval EFI_NOT_FOUND               EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL not found

**/
EFI_STATUS
GetVariableStorageIoCompletionProtocol (
  IN  EFI_GUID                                        *InstanceId,
  OUT EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   **VariableStorageIoCompletionProtocol
  )
{
  EFI_GUID                                        InstanceGuid;
  EFI_STATUS                                      Status;
  UINTN                                           Index;
  EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   *CurrentInstance;

  *VariableStorageIoCompletionProtocol = NULL;
  if (InstanceId == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (mVariableStoreIoCompletionProtocols == NULL) {
    return EFI_NOT_FOUND;
  }

  for ( Index = 0;
        Index < mVariableStoreIoCompletionProtocolsCount;
        Index++) {
    CurrentInstance = mVariableStoreIoCompletionProtocols[Index];
    if (CurrentInstance == NULL) {
      continue;
    }
    ZeroMem ((VOID *) &InstanceGuid, sizeof (EFI_GUID));
    Status = CurrentInstance->GetId (CurrentInstance, &InstanceGuid);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    if (CompareGuid (InstanceId, &InstanceGuid)) {
      *VariableStorageIoCompletionProtocol = CurrentInstance;
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_FOUND;
}

/**
  Mark a variable that will become read-only after leaving the DXE phase of execution.

  @param[in] This          The VARIABLE_LOCK_PROTOCOL instance.
  @param[in] VariableName  A pointer to the variable name that will be made read-only subsequently.
  @param[in] VendorGuid    A pointer to the vendor GUID that will be made read-only subsequently.

  @retval EFI_SUCCESS           The variable specified by the VariableName and the VendorGuid was marked
                                as pending to be read-only.
  @retval EFI_INVALID_PARAMETER VariableName or VendorGuid is NULL.
                                Or VariableName is an empty string.
  @retval EFI_ACCESS_DENIED     EFI_END_OF_DXE_EVENT_GROUP_GUID or EFI_EVENT_GROUP_READY_TO_BOOT has
                                already been signaled.
  @retval EFI_OUT_OF_RESOURCES  There is not enough resource to hold the lock request.
**/
EFI_STATUS
EFIAPI
VariableLockRequestToLock (
  IN CONST EDKII_VARIABLE_LOCK_PROTOCOL *This,
  IN       CHAR16                       *VariableName,
  IN       EFI_GUID                     *VendorGuid
  )
{
  EFI_STATUS                                Status;
  UINTN                                     VariableNameSize;
  UINTN                                     PayloadSize;
  SMM_VARIABLE_COMMUNICATE_LOCK_VARIABLE    *VariableToLock;

  if (VariableName == NULL || VariableName[0] == 0 || VendorGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  VariableNameSize = StrSize (VariableName);
  VariableToLock   = NULL;

  //
  // If VariableName exceeds SMM payload limit. Return failure
  //
  if (VariableNameSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_LOCK_VARIABLE, Name)) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime(&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + PayloadSize.
  //
  PayloadSize = OFFSET_OF (SMM_VARIABLE_COMMUNICATE_LOCK_VARIABLE, Name) + VariableNameSize;
  Status = InitCommunicateBuffer ((VOID **) &VariableToLock, PayloadSize, SMM_VARIABLE_FUNCTION_LOCK_VARIABLE);
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  if (VariableToLock == NULL) {
    ASSERT (VariableToLock != NULL);
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&VariableToLock->Guid, VendorGuid);
  VariableToLock->NameSize = VariableNameSize;
  CopyMem (VariableToLock->Name, VariableName, VariableToLock->NameSize);

  //
  // Send data to SMM.
  //
  Status = SendCommunicateBuffer (PayloadSize, NULL, NULL, NULL, NULL);

Done:
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  Register SetVariable check handler.

  @param[in] Handler            Pointer to check handler.

  @retval EFI_SUCCESS           The SetVariable check handler was registered successfully.
  @retval EFI_INVALID_PARAMETER Handler is NULL.
  @retval EFI_ACCESS_DENIED     EFI_END_OF_DXE_EVENT_GROUP_GUID or EFI_EVENT_GROUP_READY_TO_BOOT has
                                already been signaled.
  @retval EFI_OUT_OF_RESOURCES  There is not enough resource for the SetVariable check handler register request.
  @retval EFI_UNSUPPORTED       This interface is not implemented.
                                For example, it is unsupported in VarCheck protocol if both VarCheck and SmmVarCheck protocols are present.

**/
EFI_STATUS
EFIAPI
VarCheckRegisterSetVariableCheckHandler (
  IN VAR_CHECK_SET_VARIABLE_CHECK_HANDLER   Handler
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Variable property set.

  @param[in] Name               Pointer to the variable name.
  @param[in] Guid               Pointer to the vendor GUID.
  @param[in] VariableProperty   Pointer to the input variable property.

  @retval EFI_SUCCESS           The property of variable specified by the Name and Guid was set successfully.
  @retval EFI_INVALID_PARAMETER Name, Guid or VariableProperty is NULL, or Name is an empty string,
                                or the fields of VariableProperty are not valid.
  @retval EFI_ACCESS_DENIED     EFI_END_OF_DXE_EVENT_GROUP_GUID or EFI_EVENT_GROUP_READY_TO_BOOT has
                                already been signaled.
  @retval EFI_OUT_OF_RESOURCES  There is not enough resource for the variable property set request.

**/
EFI_STATUS
EFIAPI
VarCheckVariablePropertySet (
  IN CHAR16                         *Name,
  IN EFI_GUID                       *Guid,
  IN VAR_CHECK_VARIABLE_PROPERTY    *VariableProperty
  )
{
  EFI_STATUS                                Status;
  UINTN                                     VariableNameSize;
  UINTN                                     PayloadSize;
  SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY *CommVariableProperty;

  if (Name == NULL || Name[0] == 0 || Guid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (VariableProperty == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (VariableProperty->Revision != VAR_CHECK_VARIABLE_PROPERTY_REVISION) {
    return EFI_INVALID_PARAMETER;
  }

  VariableNameSize = StrSize (Name);
  CommVariableProperty = NULL;

  //
  // If VariableName exceeds SMM payload limit. Return failure
  //
  if (VariableNameSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name)) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + PayloadSize.
  //
  PayloadSize = OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name) + VariableNameSize;
  Status = InitCommunicateBuffer ((VOID **) &CommVariableProperty, PayloadSize, SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_SET);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (CommVariableProperty == NULL) {
    ASSERT (CommVariableProperty != NULL);
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&CommVariableProperty->Guid, Guid);
  CopyMem (&CommVariableProperty->VariableProperty, VariableProperty, sizeof (*VariableProperty));
  CommVariableProperty->NameSize = VariableNameSize;
  CopyMem (CommVariableProperty->Name, Name, CommVariableProperty->NameSize);

  //
  // Send data to SMM.
  //
  Status = SendCommunicateBuffer (PayloadSize, NULL, NULL, NULL, NULL);

Done:
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  Variable property get.

  @param[in]  Name              Pointer to the variable name.
  @param[in]  Guid              Pointer to the vendor GUID.
  @param[out] VariableProperty  Pointer to the output variable property.

  @retval EFI_SUCCESS           The property of variable specified by the Name and Guid was got successfully.
  @retval EFI_INVALID_PARAMETER Name, Guid or VariableProperty is NULL, or Name is an empty string.
  @retval EFI_NOT_FOUND         The property of variable specified by the Name and Guid was not found.

**/
EFI_STATUS
EFIAPI
VarCheckVariablePropertyGet (
  IN CHAR16                         *Name,
  IN EFI_GUID                       *Guid,
  OUT VAR_CHECK_VARIABLE_PROPERTY   *VariableProperty
  )
{
  EFI_STATUS                                Status;
  UINTN                                     VariableNameSize;
  UINTN                                     PayloadSize;
  SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY *CommVariableProperty;

  if (Name == NULL || Name[0] == 0 || Guid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (VariableProperty == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  VariableNameSize = StrSize (Name);
  CommVariableProperty = NULL;

  //
  // If VariableName exceeds SMM payload limit. Return failure
  //
  if (VariableNameSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name)) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + PayloadSize.
  //
  PayloadSize = OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name) + VariableNameSize;
  Status = InitCommunicateBuffer ((VOID **) &CommVariableProperty, PayloadSize, SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_GET);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (CommVariableProperty == NULL) {
    ASSERT (CommVariableProperty != NULL);
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&CommVariableProperty->Guid, Guid);
  CommVariableProperty->NameSize = VariableNameSize;
  CopyMem (CommVariableProperty->Name, Name, CommVariableProperty->NameSize);

  //
  // Send data to SMM.
  //
  Status = SendCommunicateBuffer (PayloadSize, NULL, NULL, NULL, NULL);
  if (Status == EFI_SUCCESS) {
    CopyMem (VariableProperty, &CommVariableProperty->VariableProperty, sizeof (*VariableProperty));
  }

Done:
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  Clears the global CommandInProgress indicator in SMM

**/
VOID
EFIAPI
ClearCommandInProgress (
  VOID
  )
{
  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE.
  //
  InitCommunicateBuffer (NULL, 0, SMM_VARIABLE_FUNCTION_CLEAR_COMMAND_IN_PROGRESS);

  //
  // Send data to SMM.
  //
  SendCommunicateBuffer (0, NULL, NULL, NULL, NULL);
}

/**
  Signals SMM to synchronize any dirty variable updates with the runtime cache(s).

**/
VOID
EFIAPI
SyncRuntimeCache (
  VOID
  )
{
  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE.
  //
  InitCommunicateBuffer (NULL, 0, SMM_VARIABLE_FUNCTION_SYNC_RUNTIME_CACHE);

  //
  // Send data to SMM.
  //
  SendCommunicateBuffer (0, NULL, NULL, NULL, NULL);
}

/**
  This code finds variable in a volatile memory store.

  Caution: This function may receive untrusted input.
  The data size is external input, so this function will validate it carefully to avoid buffer overflow.

  @param[in]      VariableName       Name of Variable to be found.
  @param[in]      VendorGuid         Variable vendor GUID.
  @param[out]     Attributes         Attribute value of the variable found.
  @param[in, out] DataSize           Size of Data found. If size is less than the
                                     data, this value contains the required size.
  @param[out]     Data               Data pointer.

  @retval EFI_SUCCESS                Found the specified variable.
  @retval EFI_INVALID_PARAMETER      Invalid parameter.
  @retval EFI_NOT_FOUND              The specified variable could not be found.

**/
EFI_STATUS
EFIAPI
FindVariableInRuntimeCache (
  IN      CHAR16                            *VariableName,
  IN      EFI_GUID                          *VendorGuid,
  OUT     UINT32                            *Attributes OPTIONAL,
  IN OUT  UINTN                             *DataSize,
  OUT     VOID                              *Data OPTIONAL
  )
{
  EFI_STATUS              Status;
  UINTN                   DelayIndex;
  UINTN                   TempDataSize;
  VARIABLE_POINTER_TRACK  RtPtrTrack;

  if (VariableName == NULL || VendorGuid == NULL || DataSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "Checking the runtime cache for:\n"));
  DEBUG ((DEBUG_INFO, "  Variable name: %s\n  Vendor GUID: %g\n", VariableName, VendorGuid));

  for (DelayIndex = 0; mVariableRuntimeCacheReadLock && DelayIndex < 200000; DelayIndex++) {
    MicroSecondDelay (10);
  }
  if (DelayIndex < 200000) {
    DEBUG ((DEBUG_INFO, "  RuntimeCacheReadLock is available.\n"));
    ASSERT (!mVariableRuntimeCacheReadLock);

    if (mVariableRuntimeCachePendingUpdate) {
      DEBUG ((DEBUG_INFO, "  Pending update... Triggering SMM to sync caches.\n"));
      SyncRuntimeCache ();
    }
    ASSERT (!mVariableRuntimeCachePendingUpdate);

    if (!mVariableRuntimeCachePendingUpdate) {
      mVariableRuntimeCacheReadLock = TRUE;

      RtPtrTrack.StartPtr = GetStartPointer (mVariableRuntimeVolatileCacheBuffer);
      RtPtrTrack.EndPtr   = GetEndPointer   (mVariableRuntimeVolatileCacheBuffer);
      RtPtrTrack.Volatile = TRUE;
      Status = FindVariableEx (VariableName, VendorGuid, FALSE, &RtPtrTrack);
      DEBUG ((DEBUG_INFO, "  Volatile runtime cache find status = %r\n", Status));

      if (EFI_ERROR (Status)) {
        RtPtrTrack.StartPtr = GetStartPointer (mVariableRuntimeNvCacheBuffer);
        RtPtrTrack.EndPtr   = GetEndPointer   (mVariableRuntimeNvCacheBuffer);
        RtPtrTrack.Volatile = FALSE;
        Status = FindVariableEx (VariableName, VendorGuid, FALSE, &RtPtrTrack);
        DEBUG ((DEBUG_INFO, "  Non-volatile runtime cache find status = %r\n", Status));
      }
      mVariableRuntimeCacheReadLock = FALSE;
      if (!EFI_ERROR (Status)) {
        //
        // Get data size
        //
        TempDataSize = DataSizeOfVariable (RtPtrTrack.CurrPtr);
        ASSERT (TempDataSize != 0);

        if (*DataSize >= TempDataSize) {
          if (Data != NULL) {
            CopyMem (Data, GetVariableDataPtr (RtPtrTrack.CurrPtr), TempDataSize);
          }
          if (Attributes != NULL) {
            *Attributes = RtPtrTrack.CurrPtr->Attributes;
          }

          *DataSize = TempDataSize;
          // Todo: Need to call UpdateVariableInfo () to track the read access

          DEBUG ((DEBUG_INFO, "  Used the variable from the runtime cache.\n"));
          Status = EFI_SUCCESS;
          goto Done;
        } else {
          *DataSize = TempDataSize;
          Status = EFI_BUFFER_TOO_SMALL;
          goto Done;
        }
      }
    }
  }

Done:
  return Status;
}

/**
  This code finds variable in storage blocks (Volatile or Non-Volatile).

  Caution: This function may receive untrusted input.
  The data size is external input, so this function will validate it carefully to avoid buffer overflow.

  @param[in]      VariableName       Name of Variable to be found.
  @param[in]      VendorGuid         Variable vendor GUID.
  @param[out]     Attributes         Attribute value of the variable found.
  @param[in, out] DataSize           Size of Data found. If size is less than the
                                     data, this value contains the required size.
  @param[out]     Data               Data pointer.

  @retval EFI_INVALID_PARAMETER      Invalid parameter.
  @retval EFI_SUCCESS                Find the specified variable.
  @retval EFI_NOT_FOUND              Not found.
  @retval EFI_BUFFER_TO_SMALL        DataSize is too small for the result.

**/
EFI_STATUS
EFIAPI
RuntimeServiceGetVariable (
  IN      CHAR16                            *VariableName,
  IN      EFI_GUID                          *VendorGuid,
  OUT     UINT32                            *Attributes OPTIONAL,
  IN OUT  UINTN                             *DataSize,
  OUT     VOID                              *Data
  )
{
  EFI_STATUS                                      Status;
  EFI_STATUS                                      Status2;
  UINTN                                           PayloadSize;
  SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE        *SmmVariableHeader;
  UINTN                                           TempDataSize;
  UINTN                                           VariableNameSize;
  UINTN                                           DelayTimer;
  EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   *IoCompletionProtocol;
  BOOLEAN                                         CommandInProgress;
  BOOLEAN                                         ReenterFunction;
  BOOLEAN                                         VariableServicesInUse;
  EFI_GUID                                        InProgressNvStorageInstanceId;

  if (VariableName == NULL || VendorGuid == NULL || DataSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TempDataSize          = *DataSize;
  VariableNameSize      = StrSize (VariableName);
  SmmVariableHeader     = NULL;

  //
  // If VariableName exceeds SMM payload limit. Return failure
  //
  if (VariableNameSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Check the runtime caches before triggering an SMI
  //
  Status =  FindVariableInRuntimeCache (VariableName, VendorGuid, Attributes, DataSize, Data);
  if (!EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + PayloadSize.
  //
  if (TempDataSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) - VariableNameSize) {
    //
    // If output data buffer exceed SMM payload limit. Trim output buffer to SMM payload size
    //
    TempDataSize = mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) - VariableNameSize;
  }
  PayloadSize = OFFSET_OF (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) + VariableNameSize + TempDataSize;

  Status = InitCommunicateBuffer ((VOID **) &SmmVariableHeader, PayloadSize, SMM_VARIABLE_FUNCTION_GET_VARIABLE);
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  if (SmmVariableHeader == NULL) {
    ASSERT (SmmVariableHeader != NULL);
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&SmmVariableHeader->Guid, VendorGuid);
  SmmVariableHeader->DataSize   = TempDataSize;
  SmmVariableHeader->NameSize   = VariableNameSize;
  if (Attributes == NULL) {
    SmmVariableHeader->Attributes = 0;
  } else {
    SmmVariableHeader->Attributes = *Attributes;
  }
  CopyMem (SmmVariableHeader->Name, VariableName, SmmVariableHeader->NameSize);

  //
  // Send data to SMM.
  //
  IoCompletionProtocol  = NULL;
  CommandInProgress     = FALSE;
  ReenterFunction       = FALSE;
  VariableServicesInUse = FALSE;
  ZeroMem (&InProgressNvStorageInstanceId, sizeof (EFI_GUID));
  Status  = SendCommunicateBuffer (
              PayloadSize,
              &CommandInProgress,
              &InProgressNvStorageInstanceId,
              &ReenterFunction,
              &VariableServicesInUse
              );
  if (VariableServicesInUse) {
    //
    // Wait for 5 seconds
    //
    for (DelayTimer = 0; DelayTimer < 5000; DelayTimer++) {
      MicroSecondDelay (1000);
    }
    //
    // Retry the command
    //
    CommandInProgress     = FALSE;
    ReenterFunction       = FALSE;
    VariableServicesInUse = FALSE;
    ZeroMem (&InProgressNvStorageInstanceId, sizeof (EFI_GUID));
    Status  = SendCommunicateBuffer (
                PayloadSize,
                &CommandInProgress,
                &InProgressNvStorageInstanceId,
                &ReenterFunction,
                &VariableServicesInUse
                );
    if (VariableServicesInUse) {
      //
      // 5 seconds is the maximum time we will wait
      //
      Status = EFI_DEVICE_ERROR;
      goto Done;
    }
  }
  if (CommandInProgress) {
    Status2 = GetVariableStorageIoCompletionProtocol (&InProgressNvStorageInstanceId, &IoCompletionProtocol);
    if (!EFI_ERROR (Status2) && (IoCompletionProtocol != NULL)) {
      Status2 = IoCompletionProtocol->Complete (IoCompletionProtocol, FALSE);
      if (!EFI_ERROR (Status2) && ReenterFunction) {
        //
        // Resend the command
        //
        CommandInProgress = FALSE;
        ZeroMem (&InProgressNvStorageInstanceId, sizeof (EFI_GUID));
        Status = SendCommunicateBuffer (PayloadSize, &CommandInProgress, NULL, NULL, &VariableServicesInUse);
        //
        // Command should complete immediately this time
        //
        ASSERT (!CommandInProgress);
        ASSERT (!VariableServicesInUse);
        if (CommandInProgress || VariableServicesInUse) {
          Status = EFI_DEVICE_ERROR;
        }
        if (CommandInProgress) {
          ClearCommandInProgress ();
        }
      } else if (EFI_ERROR (Status2)) {
        Status = Status2;
        ClearCommandInProgress ();
      }
    } else {
      Status = EFI_DEVICE_ERROR;
      ClearCommandInProgress ();
    }
  }

  //
  // Get data from SMM.
  //
  if (Status == EFI_SUCCESS || Status == EFI_BUFFER_TOO_SMALL) {
    //
    // SMM CommBuffer DataSize can be a trimed value
    // Only update DataSize when needed
    //
    *DataSize = SmmVariableHeader->DataSize;
  }
  if (Attributes != NULL) {
    *Attributes = SmmVariableHeader->Attributes;
  }

  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (Data != NULL) {
    CopyMem (Data, (UINT8 *)SmmVariableHeader->Name + SmmVariableHeader->NameSize, SmmVariableHeader->DataSize);
  } else {
    Status = EFI_INVALID_PARAMETER;
  }

Done:
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}


/**
  This code Finds the Next available variable.

  @param[in, out] VariableNameSize   Size of the variable name.
  @param[in, out] VariableName       Pointer to variable name.
  @param[in, out] VendorGuid         Variable Vendor Guid.

  @retval EFI_INVALID_PARAMETER      Invalid parameter.
  @retval EFI_SUCCESS                Find the specified variable.
  @retval EFI_NOT_FOUND              Not found.
  @retval EFI_BUFFER_TO_SMALL        DataSize is too small for the result.

**/
EFI_STATUS
EFIAPI
RuntimeServiceGetNextVariableName (
  IN OUT  UINTN                             *VariableNameSize,
  IN OUT  CHAR16                            *VariableName,
  IN OUT  EFI_GUID                          *VendorGuid
  )
{
  EFI_STATUS                                      Status;
  UINTN                                           PayloadSize;
  SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME *SmmGetNextVariableName;
  UINTN                                           OutVariableNameSize;
  UINTN                                           InVariableNameSize;

  if (VariableNameSize == NULL || VariableName == NULL || VendorGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OutVariableNameSize   = *VariableNameSize;
  InVariableNameSize    = StrSize (VariableName);
  SmmGetNextVariableName = NULL;

  //
  // If input string exceeds SMM payload limit. Return failure
  //
  if (InVariableNameSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name)) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + PayloadSize.
  //
  if (OutVariableNameSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name)) {
    //
    // If output buffer exceed SMM payload limit. Trim output buffer to SMM payload size
    //
    OutVariableNameSize = mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name);
  }
  //
  // Payload should be Guid + NameSize + MAX of Input & Output buffer
  //
  PayloadSize = OFFSET_OF (SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name) + MAX (OutVariableNameSize, InVariableNameSize);

  Status = InitCommunicateBuffer ((VOID **) &SmmGetNextVariableName, PayloadSize, SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME);
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  if (SmmGetNextVariableName == NULL) {
    ASSERT (SmmGetNextVariableName != NULL);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // SMM comm buffer->NameSize is buffer size for return string
  //
  SmmGetNextVariableName->NameSize = OutVariableNameSize;

  CopyGuid (&SmmGetNextVariableName->Guid, VendorGuid);
  //
  // Copy whole string
  //
  CopyMem (SmmGetNextVariableName->Name, VariableName, InVariableNameSize);
  if (OutVariableNameSize > InVariableNameSize) {
    ZeroMem ((UINT8 *) SmmGetNextVariableName->Name + InVariableNameSize, OutVariableNameSize - InVariableNameSize);
  }

  //
  // Send data to SMM
  //
  Status = SendCommunicateBuffer (PayloadSize, NULL, NULL, NULL, NULL);

  //
  // Get data from SMM.
  //
  if (Status == EFI_SUCCESS || Status == EFI_BUFFER_TOO_SMALL) {
    //
    // SMM CommBuffer NameSize can be a trimed value
    // Only update VariableNameSize when needed
    //
    *VariableNameSize = SmmGetNextVariableName->NameSize;
  }
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  CopyGuid (VendorGuid, &SmmGetNextVariableName->Guid);
  CopyMem (VariableName, SmmGetNextVariableName->Name, SmmGetNextVariableName->NameSize);

Done:
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  This code sets variable in storage blocks (Volatile or Non-Volatile).

  Caution: This function may receive untrusted input.
  The data size and data are external input, so this function will validate it carefully to avoid buffer overflow.

  @param[in] VariableName                 Name of Variable to be found.
  @param[in] VendorGuid                   Variable vendor GUID.
  @param[in] Attributes                   Attribute value of the variable found
  @param[in] DataSize                     Size of Data found. If size is less than the
                                          data, this value contains the required size.
  @param[in] Data                         Data pointer.

  @retval EFI_INVALID_PARAMETER           Invalid parameter.
  @retval EFI_SUCCESS                     Set successfully.
  @retval EFI_OUT_OF_RESOURCES            Resource not enough to set variable.
  @retval EFI_NOT_FOUND                   Not found.
  @retval EFI_WRITE_PROTECTED             Variable is read-only.

**/
EFI_STATUS
EFIAPI
RuntimeServiceSetVariable (
  IN CHAR16                                 *VariableName,
  IN EFI_GUID                               *VendorGuid,
  IN UINT32                                 Attributes,
  IN UINTN                                  DataSize,
  IN VOID                                   *Data
  )
{
  EFI_STATUS                                      Status;
  EFI_STATUS                                      Status2;
  UINTN                                           PayloadSize;
  SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE        *SmmVariableHeader;
  UINTN                                           VariableNameSize;
  UINTN                                           DelayTimer;
  EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   *IoCompletionProtocol;
  BOOLEAN                                         CommandInProgress;
  BOOLEAN                                         ReenterFunction;
  BOOLEAN                                         VariableServicesInUse;
  EFI_GUID                                        InProgressNvStorageInstanceId;

  //
  // Check input parameters.
  //
  if (VariableName == NULL || VariableName[0] == 0 || VendorGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (DataSize != 0 && Data == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  VariableNameSize      = StrSize (VariableName);
  SmmVariableHeader     = NULL;

  //
  // If VariableName or DataSize exceeds SMM payload limit. Return failure
  //
  if ((VariableNameSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) ||
      (DataSize > mVariableBufferPayloadSize - OFFSET_OF (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) - VariableNameSize)){
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + PayloadSize.
  //
  PayloadSize = OFFSET_OF (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) + VariableNameSize + DataSize;
  Status = InitCommunicateBuffer ((VOID **) &SmmVariableHeader, PayloadSize, SMM_VARIABLE_FUNCTION_SET_VARIABLE);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (SmmVariableHeader == NULL) {
    ASSERT (SmmVariableHeader != NULL);
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  CopyGuid ((EFI_GUID *) &SmmVariableHeader->Guid, VendorGuid);
  SmmVariableHeader->DataSize   = DataSize;
  SmmVariableHeader->NameSize   = VariableNameSize;
  SmmVariableHeader->Attributes = Attributes;
  CopyMem (SmmVariableHeader->Name, VariableName, SmmVariableHeader->NameSize);
  CopyMem ((UINT8 *) SmmVariableHeader->Name + SmmVariableHeader->NameSize, Data, DataSize);

  //
  // Send data to SMM. For SetVariable we may need to reenter SMM an arbitary number of times in order to complete
  // the write request. Continue to reenter SMM until ReenterFunction comes back FALSE
  //
  ReenterFunction = TRUE;
  while (ReenterFunction) {
    IoCompletionProtocol  = NULL;
    CommandInProgress     = FALSE;
    ReenterFunction       = FALSE;
    VariableServicesInUse = FALSE;
    ZeroMem (&InProgressNvStorageInstanceId, sizeof (EFI_GUID));
    Status  = SendCommunicateBuffer (
                PayloadSize,
                &CommandInProgress,
                &InProgressNvStorageInstanceId,
                &ReenterFunction,
                &VariableServicesInUse
                );
    if (VariableServicesInUse) {
      //
      // Wait for 5 seconds
      //
      for (DelayTimer = 0; DelayTimer < 5000; DelayTimer++) {
        MicroSecondDelay (1000);
      }
      //
      // Retry the command
      //
      CommandInProgress     = FALSE;
      ReenterFunction       = FALSE;
      VariableServicesInUse = FALSE;
      ZeroMem (&InProgressNvStorageInstanceId, sizeof (EFI_GUID));
      Status  = SendCommunicateBuffer (
                  PayloadSize,
                  &CommandInProgress,
                  &InProgressNvStorageInstanceId,
                  &ReenterFunction,
                  &VariableServicesInUse
                  );
      if (VariableServicesInUse) {
        //
        // 5 seconds is the maximum time we will wait
        //
        Status = EFI_DEVICE_ERROR;
        goto Done;
      }
    }
    if (CommandInProgress) {
      Status2 = GetVariableStorageIoCompletionProtocol (&InProgressNvStorageInstanceId, &IoCompletionProtocol);
      if (!EFI_ERROR (Status2) && (IoCompletionProtocol != NULL)) {
        Status2 = IoCompletionProtocol->Complete (IoCompletionProtocol, TRUE);
        Status = Status2;
        if (EFI_ERROR (Status2)) {
          break;
        }
      } else {
        Status = EFI_DEVICE_ERROR;
        break;
      }
    } else {
      ReenterFunction = FALSE;
      break;
    }
  }
  ClearCommandInProgress ();

Done:
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);

  if (!EfiAtRuntime ()) {
    if (!EFI_ERROR (Status)) {
      SecureBootHook (
        VariableName,
        VendorGuid
        );
    }
  }
  return Status;
}


/**
  This code returns information about the EFI variables.

  @param[in]  Attributes                   Attributes bitmask to specify the type of variables
                                           on which to return information.
  @param[out] MaximumVariableStorageSize   Pointer to the maximum size of the storage space available
                                           for the EFI variables associated with the attributes specified.
  @param[out] RemainingVariableStorageSize Pointer to the remaining size of the storage space available
                                           for EFI variables associated with the attributes specified.
  @param[out] MaximumVariableSize          Pointer to the maximum size of an individual EFI variables
                                           associated with the attributes specified.

  @retval EFI_INVALID_PARAMETER            An invalid combination of attribute bits was supplied.
  @retval EFI_SUCCESS                      Query successfully.
  @retval EFI_UNSUPPORTED                  The attribute is not supported on this platform.

**/
EFI_STATUS
EFIAPI
RuntimeServiceQueryVariableInfo (
  IN  UINT32                                Attributes,
  OUT UINT64                                *MaximumVariableStorageSize,
  OUT UINT64                                *RemainingVariableStorageSize,
  OUT UINT64                                *MaximumVariableSize
  )
{
  EFI_STATUS                                Status;
  UINTN                                     PayloadSize;
  SMM_VARIABLE_COMMUNICATE_QUERY_VARIABLE_INFO *SmmQueryVariableInfo;

  SmmQueryVariableInfo = NULL;

  if(MaximumVariableStorageSize == NULL || RemainingVariableStorageSize == NULL || MaximumVariableSize == NULL || Attributes == 0) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime(&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + PayloadSize;
  //
  PayloadSize = sizeof (SMM_VARIABLE_COMMUNICATE_QUERY_VARIABLE_INFO);
  Status = InitCommunicateBuffer ((VOID **)&SmmQueryVariableInfo, PayloadSize, SMM_VARIABLE_FUNCTION_QUERY_VARIABLE_INFO);
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  if (SmmQueryVariableInfo == NULL) {
    ASSERT (SmmQueryVariableInfo != NULL);
    return EFI_OUT_OF_RESOURCES;
  }

  SmmQueryVariableInfo->Attributes  = Attributes;

  //
  // Send data to SMM.
  //
  Status = SendCommunicateBuffer (PayloadSize, NULL, NULL, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Get data from SMM.
  //
  *MaximumVariableSize          = SmmQueryVariableInfo->MaximumVariableSize;
  *MaximumVariableStorageSize   = SmmQueryVariableInfo->MaximumVariableStorageSize;
  *RemainingVariableStorageSize = SmmQueryVariableInfo->RemainingVariableStorageSize;

Done:
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  Exit Boot Services Event notification handler.

  Notify SMM variable driver about the event.

  @param[in]  Event     Event whose notification function is being invoked.
  @param[in]  Context   Pointer to the notification function's context.

**/
VOID
EFIAPI
OnExitBootServices (
  IN      EFI_EVENT                         Event,
  IN      VOID                              *Context
  )
{
  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE.
  //
  InitCommunicateBuffer (NULL, 0, SMM_VARIABLE_FUNCTION_EXIT_BOOT_SERVICE);

  //
  // Send data to SMM.
  //
  SendCommunicateBuffer (0, NULL, NULL, NULL, NULL);
}


/**
  On Ready To Boot Services Event notification handler.

  Notify SMM variable driver about the event.

  @param[in]  Event     Event whose notification function is being invoked
  @param[in]  Context   Pointer to the notification function's context

**/
VOID
EFIAPI
OnReadyToBoot (
  IN      EFI_EVENT                         Event,
  IN      VOID                              *Context
  )
{
  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE.
  //
  InitCommunicateBuffer (NULL, 0, SMM_VARIABLE_FUNCTION_READY_TO_BOOT);

  //
  // Send data to SMM.
  //
  SendCommunicateBuffer (0, NULL, NULL, NULL, NULL);

  gBS->CloseEvent (Event);
}


/**
  Notification function of EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE.

  This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
  It convers pointer to new virtual address.

  @param[in]  Event        Event whose notification function is being invoked.
  @param[in]  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
VariableAddressChangeEvent (
  IN EFI_EVENT                              Event,
  IN VOID                                   *Context
  )
{
  UINTN                                           Index;
  EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   *VariableStoreIoCompletionProtocol;

  for ( Index = 0;
        Index < mVariableStoreIoCompletionProtocolsCount;
        Index++) {
    VariableStoreIoCompletionProtocol = mVariableStoreIoCompletionProtocols[Index];
    EfiConvertPointer (0x0, (VOID **) &VariableStoreIoCompletionProtocol->Complete);
    EfiConvertPointer (0x0, (VOID **) &VariableStoreIoCompletionProtocol->GetId);
    EfiConvertPointer (0x0, (VOID **) &mVariableStoreIoCompletionProtocols[Index]);
  }
  EfiConvertPointer (0x0, (VOID **) &mVariableStoreIoCompletionProtocols);
  EfiConvertPointer (0x0, (VOID **) &mVariableBuffer);
  EfiConvertPointer (0x0, (VOID **) &mVariableRuntimeNvCacheBuffer);
  EfiConvertPointer (0x0, (VOID **) &mVariableRuntimeVolatileCacheBuffer);
  EfiConvertPointer (0x0, (VOID **) &mSmmCommunication);
}

/**
  This code gets variable payload size.

  @param[out] VariablePayloadSize   Output pointer to variable payload size.

  @retval EFI_SUCCESS               Get successfully.
  @retval Others                    Get unsuccessfully.

**/
EFI_STATUS
EFIAPI
GetVariablePayloadSize (
  OUT UINTN                         *VariablePayloadSize
  )
{
  EFI_STATUS                                Status;
  SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *SmmGetPayloadSize;
  EFI_SMM_COMMUNICATE_HEADER                *SmmCommunicateHeader;
  SMM_VARIABLE_COMMUNICATE_HEADER2          *SmmVariableFunctionHeader;
  UINTN                                     CommSize;
  UINT8                                     *CommBuffer;

  SmmGetPayloadSize = NULL;
  CommBuffer = NULL;

  if(VariablePayloadSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE);
  //
  CommSize = SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE);
  CommBuffer = AllocateZeroPool (CommSize);
  if (CommBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  SmmCommunicateHeader = (EFI_SMM_COMMUNICATE_HEADER *) CommBuffer;
  CopyGuid (&SmmCommunicateHeader->HeaderGuid, &gEfiSmmVariableProtocolGuid);
  SmmCommunicateHeader->MessageLength = SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE);

  SmmVariableFunctionHeader = (SMM_VARIABLE_COMMUNICATE_HEADER2 *) SmmCommunicateHeader->Data;
  SmmVariableFunctionHeader->Function = SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE;
  SmmGetPayloadSize = (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *) SmmVariableFunctionHeader->Data;

  //
  // Send data to SMM.
  //
  Status = mSmmCommunication->Communicate (mSmmCommunication, CommBuffer, &CommSize);
  ASSERT_EFI_ERROR (Status);

  Status = SmmVariableFunctionHeader->ReturnStatus;
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Get data from SMM.
  //
  *VariablePayloadSize = SmmGetPayloadSize->VariablePayloadSize;

Done:
  if (CommBuffer != NULL) {
    FreePool (CommBuffer);
  }
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  This code gets the total size required for variable stores in runtime.

  @param[out] TotalNvStorageSize        Output pointer for the total non-volatile storage size in bytes.
  @param[out] TotalVolatileStorageSize  Output pointer for the total volatile storage size in bytes.

  @retval EFI_SUCCESS                   Retrieved the size successfully.
  @retval EFI_INVALID_PARAMETER         TotalNvStorageSize parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES          Could not allocate a CommBuffer.
  @retval Others                         Could not retrieve the size successfully.;

**/
EFI_STATUS
EFIAPI
GetTotalRuntimeStoreSize (
  OUT UINTN                         *TotalNvStorageSize,
  OUT UINTN                         *TotalVolatileStorageSize
  )
{
  EFI_STATUS                                          Status;
  SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE       *SmmGetTotalStoreSize;
  EFI_SMM_COMMUNICATE_HEADER                          *SmmCommunicateHeader;
  SMM_VARIABLE_COMMUNICATE_HEADER2                    *SmmVariableFunctionHeader;
  UINTN                                               CommSize;
  UINT8                                               *CommBuffer;

  SmmGetTotalStoreSize = NULL;
  CommBuffer = NULL;

  if (TotalNvStorageSize == NULL || TotalVolatileStorageSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE);
  //
  CommSize = SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE);
  CommBuffer = AllocateZeroPool (CommSize);
  if (CommBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  SmmCommunicateHeader = (EFI_SMM_COMMUNICATE_HEADER *) CommBuffer;
  CopyGuid (&SmmCommunicateHeader->HeaderGuid, &gEfiSmmVariableProtocolGuid);
  SmmCommunicateHeader->MessageLength = SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE);

  SmmVariableFunctionHeader = (SMM_VARIABLE_COMMUNICATE_HEADER2 *) SmmCommunicateHeader->Data;
  SmmVariableFunctionHeader->Function = SMM_VARIABLE_FUNCTION_GET_TOTAL_STORE_SIZE;
  SmmGetTotalStoreSize = (SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE *) SmmVariableFunctionHeader->Data;

  //
  // Send data to SMM.
  //
  Status = mSmmCommunication->Communicate (mSmmCommunication, CommBuffer, &CommSize);
  ASSERT_EFI_ERROR (Status);

  Status = SmmVariableFunctionHeader->ReturnStatus;
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Get data from SMM.
  //
  *TotalNvStorageSize = SmmGetTotalStoreSize->TotalNvStorageSize;
  *TotalVolatileStorageSize = SmmGetTotalStoreSize->TotalVolatileStorageSize;

Done:
  if (CommBuffer != NULL) {
    FreePool (CommBuffer);
  }
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  Sends the runtime variable cache context information to SMM.

  @retval EFI_SUCCESS               Retrieved the size successfully.
  @retval EFI_INVALID_PARAMETER     TotalNvStorageSize parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES      Could not allocate a CommBuffer.
  @retval Others                    Could not retrieve the size successfully.;

**/
EFI_STATUS
EFIAPI
SendRuntimeVariableCacheContextToSmm (
  VOID
  )
{
  EFI_STATUS                                                Status;
  SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT   *SmmRuntimeVarCacheContext;
  EFI_SMM_COMMUNICATE_HEADER                                *SmmCommunicateHeader;
  SMM_VARIABLE_COMMUNICATE_HEADER2                          *SmmVariableFunctionHeader;
  UINTN                                                     CommSize;
  UINT8                                                     *CommBuffer;

  SmmRuntimeVarCacheContext = NULL;
  CommBuffer = NULL;

  AcquireLockOnlyAtBootTime (&mVariableServicesLock);

  //
  // Init the communicate buffer. The buffer data size is:
  // SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT);
  //
  CommSize = SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT);
  CommBuffer = AllocateZeroPool (CommSize);
  if (CommBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  SmmCommunicateHeader = (EFI_SMM_COMMUNICATE_HEADER *) CommBuffer;
  CopyGuid (&SmmCommunicateHeader->HeaderGuid, &gEfiSmmVariableProtocolGuid);
  SmmCommunicateHeader->MessageLength = SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + sizeof (SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT);

  SmmVariableFunctionHeader = (SMM_VARIABLE_COMMUNICATE_HEADER2 *) SmmCommunicateHeader->Data;
  SmmVariableFunctionHeader->Function = SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT;
  SmmRuntimeVarCacheContext = (SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT *) SmmVariableFunctionHeader->Data;

  SmmRuntimeVarCacheContext->RuntimeNvCache = mVariableRuntimeNvCacheBuffer;
  SmmRuntimeVarCacheContext->RuntimeVolatileCache = mVariableRuntimeVolatileCacheBuffer;
  SmmRuntimeVarCacheContext->PendingUpdate = &mVariableRuntimeCachePendingUpdate;
  SmmRuntimeVarCacheContext->ReadLock = &mVariableRuntimeCacheReadLock;

  //
  // Send data to SMM.
  //
  Status = mSmmCommunication->Communicate (mSmmCommunication, CommBuffer, &CommSize);
  ASSERT_EFI_ERROR (Status);

  Status = SmmVariableFunctionHeader->ReturnStatus;
  if (EFI_ERROR (Status)) {
    goto Done;
  }

Done:
  if (CommBuffer != NULL) {
    FreePool (CommBuffer);
  }
  ReleaseLockOnlyAtBootTime (&mVariableServicesLock);
  return Status;
}

/**
  Initialize variable service and install Variable Architectural protocol.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.

**/
VOID
EFIAPI
SmmVariableReady (
  IN  EFI_EVENT                             Event,
  IN  VOID                                  *Context
  )
{
  EFI_STATUS                                Status;
  EFI_HANDLE                                *Handles;
  UINTN                                     Index;

  Status = gBS->LocateProtocol (&gEfiSmmVariableProtocolGuid, NULL, (VOID **)&mSmmVariable);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = gBS->LocateProtocol (&gEfiSmmCommunicationProtocolGuid, NULL, (VOID **) &mSmmCommunication);
  ASSERT_EFI_ERROR (Status);

  //
  // Allocate memory for variable communicate buffer.
  //
  Status = GetVariablePayloadSize (&mVariableBufferPayloadSize);
  ASSERT_EFI_ERROR (Status);
  mVariableBufferSize  = SMM_COMMUNICATE_HEADER_SIZE + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE + mVariableBufferPayloadSize;
  mVariableBuffer      = AllocateRuntimePool (mVariableBufferSize);
  ASSERT (mVariableBuffer != NULL);

  //
  // Save the buffer physical address used for SMM conmunication.
  //
  mVariableBufferPhysical = mVariableBuffer;

  //
  // Allocate memory for the runtime variable cache.
  //
  Status = GetTotalRuntimeStoreSize (&mVariableRuntimeNvCacheBufferSize, &mVariableRuntimeVolatileCacheBufferSize);
  if (!EFI_ERROR (Status)) {
    Status = InitVariableCache (&mVariableRuntimeNvCacheBuffer, &mVariableRuntimeNvCacheBufferSize);
    if (!EFI_ERROR (Status)) {
      Status = InitVariableCache (&mVariableRuntimeVolatileCacheBuffer, &mVariableRuntimeVolatileCacheBufferSize);
      if (!EFI_ERROR (Status)) {
        Status = InitVariableHelpers (CompareGuid (&mVariableRuntimeNvCacheBuffer->Signature, &gEfiAuthenticatedVariableGuid));
        ASSERT_EFI_ERROR (Status);

        Status = SendRuntimeVariableCacheContextToSmm ();
        if (!EFI_ERROR (Status)) {
          SyncRuntimeCache ();
        }
      }
    }
    if (EFI_ERROR (Status)) {
      mVariableRuntimeNvCacheBuffer = NULL;
      mVariableRuntimeVolatileCacheBuffer = NULL;
    }
  }
  ASSERT_EFI_ERROR (Status);

  //
  // Locate the EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOLs
  //
  Handles = NULL;
  Status  = gBS->LocateHandleBuffer (
                   ByProtocol,
                   &gEdkiiVariableStorageIoCompletionProtocolGuid,
                   NULL,
                   &mVariableStoreIoCompletionProtocolsCount,
                   &Handles
                   );
  if (!EFI_ERROR (Status)) {
    mVariableStoreIoCompletionProtocols = AllocateRuntimeZeroPool (
                                            sizeof (EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL *) *
                                            mVariableStoreIoCompletionProtocolsCount
                                            );
    ASSERT (mVariableStoreIoCompletionProtocols != NULL);
    if (mVariableStoreIoCompletionProtocols != NULL) {
      for ( Index = 0;
            Index < mVariableStoreIoCompletionProtocolsCount;
            Index++) {
        Status = gBS->OpenProtocol (
                  Handles[Index],
                  &gEdkiiVariableStorageIoCompletionProtocolGuid,
                  (VOID **) &mVariableStoreIoCompletionProtocols[Index],
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
        ASSERT_EFI_ERROR (Status);
        if (EFI_ERROR (Status)) {
          mVariableStoreIoCompletionProtocolsCount = Index + 1;
          break;
        }
      }
    }
    FreePool (Handles);
  } else {
    if (Status != EFI_NOT_FOUND) {
      ASSERT_EFI_ERROR (Status);
    }
  }

  gRT->GetVariable         = RuntimeServiceGetVariable;
  gRT->GetNextVariableName = RuntimeServiceGetNextVariableName;
  gRT->SetVariable         = RuntimeServiceSetVariable;
  gRT->QueryVariableInfo   = RuntimeServiceQueryVariableInfo;

  //
  // Install the Variable Architectural Protocol on a new handle.
  //
  Status = gBS->InstallProtocolInterface (
                  &mHandle,
                  &gEfiVariableArchProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  mVariableLock.RequestToLock = VariableLockRequestToLock;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mHandle,
                  &gEdkiiVariableLockProtocolGuid,
                  &mVariableLock,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  mVarCheck.RegisterSetVariableCheckHandler = VarCheckRegisterSetVariableCheckHandler;
  mVarCheck.VariablePropertySet = VarCheckVariablePropertySet;
  mVarCheck.VariablePropertyGet = VarCheckVariablePropertyGet;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mHandle,
                  &gEdkiiVarCheckProtocolGuid,
                  &mVarCheck,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  gBS->CloseEvent (Event);
}

/**
  Waits for any internal variable operations to finish before the variable write architecture
  protocol is installed.

  @param  Event                 Indicates the event that invoke this function.
  @param  Context               Indicates the calling context.
**/
VOID
EFIAPI
VariableWriteReadyWaitHandler (
  IN  EFI_EVENT                 Event,
  IN  VOID                      *Context
  )
{
  EFI_STATUS    Status;
  VOID          *ProtocolOps;
  UINT32        Count;
  EFI_TPL       OriginalTpl;

  gBS->CloseEvent (Event);

  OriginalTpl = EfiGetCurrentTpl ();

  //
  // Lower TPL to allow any TPL_CALLBACK (and higher) code required for asynchronous operations to execute
  //
  if (OriginalTpl > TPL_APPLICATION) {
    gBS->RestoreTPL (TPL_APPLICATION);
  }

  //
  // Wait for any possible asynchronous operations to finish (may take a while depending on storage technology)
  //
  Status = gBS->LocateProtocol (&gEdkiiVariableWriteReadyOperationsCompleteGuid, NULL, (VOID **) &ProtocolOps);
  for (Count = 0; (Status == EFI_NOT_FOUND) && (Count < 600); Count++) {
    Status = gBS->LocateProtocol (&gEdkiiVariableWriteReadyOperationsCompleteGuid, NULL, (VOID **) &ProtocolOps);
    if (Status == EFI_NOT_FOUND) {
      MicroSecondDelay (1000 * 50);
    }
  }
  ASSERT_EFI_ERROR (Status);

  if (OriginalTpl > TPL_APPLICATION) {
    gBS->RaiseTPL (OriginalTpl);
  }

  if (!EFI_ERROR (Status)) {
    Status = gBS->InstallProtocolInterface (
                    &mHandle,
                    &gEfiVariableWriteArchProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    NULL
                    );
    ASSERT_EFI_ERROR (Status);
  }
}


/**
  SMM Non-Volatile variable write service is ready notify event handler.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.

**/
VOID
EFIAPI
SmmVariableWriteReady (
  IN  EFI_EVENT                             Event,
  IN  VOID                                  *Context
  )
{
  EFI_STATUS                                Status;
  VOID                                      *ProtocolOps;

  //
  // Check whether the protocol is installed or not.
  //
  Status = gBS->LocateProtocol (&gSmmVariableWriteGuid, NULL, (VOID **) &ProtocolOps);
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Some Secure Boot Policy Var (SecureBoot, etc) updates following other
  // Secure Boot Policy Variable change.  Record their initial value.
  //
  RecordSecureBootPolicyVarData ();

  if (PcdGetBool (PcdNvVariableEmulationMode)) {
    Status = gBS->InstallProtocolInterface (
                    &mHandle,
                    &gEfiVariableWriteArchProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    NULL
                    );
    ASSERT_EFI_ERROR (Status);
  } else {
    gBS->SignalEvent (mVariableWriteReadyWaitEvent);
  }

  gBS->CloseEvent (Event);
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
VariableSmmRuntimeInitialize (
  IN EFI_HANDLE                             ImageHandle,
  IN EFI_SYSTEM_TABLE                       *SystemTable
  )
{
  VOID                                      *SmmVariableRegistration;
  VOID                                      *SmmVariableWriteRegistration;
  EFI_EVENT                                 OnReadyToBootEvent;
  EFI_EVENT                                 ExitBootServiceEvent;
  EFI_EVENT                                 LegacyBootEvent;

  EfiInitializeLock (&mVariableServicesLock, TPL_NOTIFY);

  //
  // Smm variable service is ready
  //
  EfiCreateProtocolNotifyEvent (
    &gEfiSmmVariableProtocolGuid,
    TPL_CALLBACK,
    SmmVariableReady,
    NULL,
    &SmmVariableRegistration
    );

  //
  // Smm Non-Volatile variable write service is ready
  //
  EfiCreateProtocolNotifyEvent (
    &gSmmVariableWriteGuid,
    TPL_CALLBACK,
    SmmVariableWriteReady,
    NULL,
    &SmmVariableWriteRegistration
    );

  //
  // Register the event to reclaim variable for OS usage.
  //
  EfiCreateEventReadyToBootEx (
    TPL_NOTIFY,
    OnReadyToBoot,
    NULL,
    &OnReadyToBootEvent
    );

  //
  // Register the event to inform SMM variable that it is at runtime.
  //
  gBS->CreateEventEx (
         EVT_NOTIFY_SIGNAL,
         TPL_NOTIFY,
         OnExitBootServices,
         NULL,
         &gEfiEventExitBootServicesGuid,
         &ExitBootServiceEvent
         );

  //
  // Register the event to inform SMM variable that it is at runtime for legacy boot.
  // Reuse OnExitBootServices() here.
  //
  EfiCreateEventLegacyBootEx(
    TPL_NOTIFY,
    OnExitBootServices,
    NULL,
    &LegacyBootEvent
    );

  //
  // Register an event to wait for variable write ready operations to complete.
  //
  gBS->CreateEvent (
        EVT_NOTIFY_SIGNAL,
        TPL_CALLBACK,
        VariableWriteReadyWaitHandler,
        NULL,
        &mVariableWriteReadyWaitEvent
        );

  //
  // Register the event to convert the pointer for runtime.
  //
  gBS->CreateEventEx (
         EVT_NOTIFY_SIGNAL,
         TPL_NOTIFY,
         VariableAddressChangeEvent,
         NULL,
         &gEfiEventVirtualAddressChangeGuid,
         &mVirtualAddressChangeEvent
         );

  return EFI_SUCCESS;
}

