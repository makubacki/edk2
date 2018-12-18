/** @file
  This module implements Firmware Volume Block (FVB) Variable Storage Services
  and installs an instance of the VariableStorage Runtime DXE protocol.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "FvbVariableStorage.h"

#include <Library/DxeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>

//
// Module globals
//
static EFI_EVENT      mVirtualAddressChangeEvent = NULL;
static EFI_EVENT      mFtwRegistration           = NULL;
static EFI_EVENT      mVssRegistration           = NULL;
static BOOLEAN        mWriteServiceReady         = FALSE;

/**
  Calls GetVariable () on the core UEFI variable implementation

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
CoreGetVariable (
  IN      CHAR16            *VariableName,
  IN      EFI_GUID          *VendorGuid,
  OUT     UINT32            *Attributes OPTIONAL,
  IN OUT  UINTN             *DataSize,
  OUT     VOID              *Data
  )
{
  return gRT->GetVariable (
                VariableName,
                VendorGuid,
                Attributes,
                DataSize,
                Data
                );
}

/**
  Calls SetVariable () on the core UEFI variable implementation

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
CoreSetVariable (
  IN CHAR16                  *VariableName,
  IN EFI_GUID                *VendorGuid,
  IN UINT32                  Attributes,
  IN UINTN                   DataSize,
  IN VOID                    *Data
  )
{
  return gRT->SetVariable (
                VariableName,
                VendorGuid,
                Attributes,
                DataSize,
                Data
                );
}

/**
  Retrive the Fault Tolerent Write protocol interface.

  @param[out] FtwProtocol       The interface of Ftw protocol

  @retval EFI_SUCCESS           The FTW protocol instance was found and returned in FtwProtocol.
  @retval EFI_NOT_FOUND         The FTW protocol instance was not found.
  @retval EFI_INVALID_PARAMETER SarProtocol is NULL.

**/
EFI_STATUS
GetFtwProtocol (
  OUT VOID                                **FtwProtocol
  )
{
  EFI_STATUS                              Status;

  //
  // Locate Fault Tolerent Write protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiFaultTolerantWriteProtocolGuid,
                  NULL,
                  FtwProtocol
                  );
  return Status;
}

/**
  Fault Tolerant Write protocol notification event handler.

  Non-Volatile variable write may needs FTW protocol to reclaim when
  writting variable.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.

**/
VOID
EFIAPI
FtwNotificationEvent (
  IN  EFI_EVENT                           Event,
  IN  VOID                                *Context
  )
{
  EFI_STATUS                              Status;
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL      *FvbProtocol;
  EFI_FAULT_TOLERANT_WRITE_PROTOCOL       *FtwProtocol;
  EFI_PHYSICAL_ADDRESS                    NvStorageVariableBase;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR         GcdDescriptor;
  EFI_PHYSICAL_ADDRESS                    BaseAddress;
  UINT64                                  Length;
  EFI_PHYSICAL_ADDRESS                    VariableStoreBase;
  UINT64                                  VariableStoreLength;
  UINTN                                   FtwMaxBlockSize;

  //
  // Ensure FTW protocol is installed.
  //
  Status = GetFtwProtocol ((VOID**) &FtwProtocol);
  if (EFI_ERROR (Status)) {
    return ;
  }

  Status = FtwProtocol->GetMaxBlockSize (FtwProtocol, &FtwMaxBlockSize);
  if (!EFI_ERROR (Status)) {
    ASSERT (PcdGet32 (PcdFlashNvStorageVariableSize) <= FtwMaxBlockSize);
  }

  //
  // Find the proper FVB protocol for variable.
  //
  NvStorageVariableBase = (EFI_PHYSICAL_ADDRESS) PcdGet64 (PcdFlashNvStorageVariableBase64);
  if (NvStorageVariableBase == 0) {
    NvStorageVariableBase = (EFI_PHYSICAL_ADDRESS) PcdGet32 (PcdFlashNvStorageVariableBase);
  }
  Status = GetFvbInfoByAddress (NvStorageVariableBase, NULL, &FvbProtocol);
  if (EFI_ERROR (Status)) {
    return ;
  }
  mFvbInstance = FvbProtocol;

  //
  // Mark the variable storage region of the FLASH as RUNTIME.
  //
  VariableStoreBase   = NvStorageVariableBase + (((EFI_FIRMWARE_VOLUME_HEADER *)(UINTN)(NvStorageVariableBase))->HeaderLength);
  VariableStoreLength = ((VARIABLE_STORE_HEADER *)(UINTN)VariableStoreBase)->Size;
  BaseAddress = VariableStoreBase & (~EFI_PAGE_MASK);
  Length      = VariableStoreLength + (VariableStoreBase - BaseAddress);
  Length      = (Length + EFI_PAGE_SIZE - 1) & (~EFI_PAGE_MASK);

  Status      = gDS->GetMemorySpaceDescriptor (BaseAddress, &GcdDescriptor);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Variable driver failed to get flash memory attribute.\n"));
  } else {
    Status = gDS->SetMemorySpaceAttributes (
                    BaseAddress,
                    Length,
                    GcdDescriptor.Attributes | EFI_MEMORY_RUNTIME
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Variable driver failed to add EFI_MEMORY_RUNTIME attribute to Flash.\n"));
    }
  }

  Status = FvbVariableStorageWriteServiceInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "FVB write service initialization failed. Status = %r\n", Status));
  } else {
    //
    // Notify the variable driver that FVB writes are ready
    //
    if (mVariableStorageSupport != NULL) {
      mVariableStorageSupport->NotifyWriteServiceReady ();
    } else {
      mWriteServiceReady = TRUE;
    }
  }

  //
  // Close the notify event to avoid install gEfiVariableWriteArchProtocolGuid again.
  //
  gBS->CloseEvent (Event);
}

/**
  Variable Storage Support protocol notification event handler.

  The VSS protocol is needed to enable Non-Volatile variable writes

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.

**/
VOID
EFIAPI
VssNotificationEvent (
  IN  EFI_EVENT                           Event,
  IN  VOID                                *Context
  )
{
  EFI_STATUS                              Status;

  //
  // Locate Variable Storage Support protocol
  //
  Status = gBS->LocateProtocol (
                  &gEdkiiVariableStorageSupportProtocolGuid,
                  NULL,
                  &mVariableStorageSupport
                  );
  if (EFI_ERROR (Status) || mVariableStorageSupport == NULL) {
    return;
  }
  if (mWriteServiceReady) {
    mVariableStorageSupport->NotifyWriteServiceReady ();
  }
  //
  // Close the notify event to avoid re-running the NotifyWriteServiceReady() function
  //
  gBS->CloseEvent (Event);
}

/**
  Retrive the FVB protocol interface by HANDLE.

  @param[in]  FvBlockHandle     The handle of FVB protocol that provides services for
                                reading, writing, and erasing the target block.
  @param[out] FvBlock           The interface of FVB protocol

  @retval EFI_SUCCESS           The interface information for the specified protocol was returned.
  @retval EFI_UNSUPPORTED       The device does not support the FVB protocol.
  @retval EFI_INVALID_PARAMETER FvBlockHandle is not a valid EFI_HANDLE or FvBlock is NULL.

**/
EFI_STATUS
GetFvbByHandle (
  IN  EFI_HANDLE                          FvBlockHandle,
  OUT EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  **FvBlock
  )
{
  //
  // To get the FVB protocol interface on the handle
  //
  return gBS->HandleProtocol (
                FvBlockHandle,
                &gEfiFirmwareVolumeBlockProtocolGuid,
                (VOID **) FvBlock
                );
}


/**
  Function returns an array of handles that support the FVB protocol
  in a buffer allocated from pool.

  @param[out]  NumberHandles    The number of handles returned in Buffer.
  @param[out]  Buffer           A pointer to the buffer to return the requested
                                array of  handles that support FVB protocol.

  @retval EFI_SUCCESS           The array of handles was returned in Buffer, and the number of
                                handles in Buffer was returned in NumberHandles.
  @retval EFI_NOT_FOUND         No FVB handle was found.
  @retval EFI_OUT_OF_RESOURCES  There is not enough pool memory to store the matching results.
  @retval EFI_INVALID_PARAMETER NumberHandles is NULL or Buffer is NULL.

**/
EFI_STATUS
GetFvbCountAndBuffer (
  OUT UINTN                               *NumberHandles,
  OUT EFI_HANDLE                          **Buffer
  )
{
  EFI_STATUS                              Status;

  //
  // Locate all handles of Fvb protocol
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareVolumeBlockProtocolGuid,
                  NULL,
                  NumberHandles,
                  Buffer
                  );
  return Status;
}

/**
  Notification function of EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE.

  This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
  It converts pointers to a new virtual address.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
FvbVariableStorageAddressChangeEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance->GetBlockSize);
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance->GetPhysicalAddress);
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance->GetAttributes);
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance->SetAttributes);
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance->Read);
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance->Write);
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance->EraseBlocks);
  EfiConvertPointer (0x0, (VOID **) &mFvbInstance);

  EfiConvertPointer (0x0, (VOID **) &mNonVolatileVariableBase);
  EfiConvertPointer (0x0, (VOID **) &mNvVariableCache);

  EfiConvertPointer (0x0, (VOID **) &mVariableStorageSupport->NotifyWriteServiceReady);
  EfiConvertPointer (0x0, (VOID **) &mVariableStorageSupport->NotifySmmIoComplete);
  EfiConvertPointer (0x0, (VOID **) &mVariableStorageSupport->UpdateNvCache);
  EfiConvertPointer (0x0, (VOID **) &mVariableStorageSupport);
}

/**
  Provide DXE functionality of CSE NVM variable storage services.

  @param  ImageHandle  The image handle.
  @param  SystemTable  The system table.

  @retval EFI_SUCCESS  The protocol was installed successfully.
  @retval Others       Protocol could not be installed.
**/
EFI_STATUS
EFIAPI
FvbVariableStorageRuntimeDxeInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle = NULL;

  if (PcdGetBool (PcdNvVariableEmulationMode) || !PcdGetBool (PcdEnableFvbVariableStorage)) {
    DEBUG ((DEBUG_INFO, "FVB Variable Storage Protocol is disabled.\n"));
    return EFI_SUCCESS;
  }

  Status = FvbVariableStorageCommonInitialize ();

  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  Status = gBS->InstallProtocolInterface (
                  &Handle,
                  &gEdkiiVariableStorageProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mFvbVariableStorageProtocol
                  );

  //
  // Register FtwNotificationEvent () notify function.
  //
  EfiCreateProtocolNotifyEvent (
    &gEfiFaultTolerantWriteProtocolGuid,
    TPL_CALLBACK,
    FtwNotificationEvent,
    (VOID *)SystemTable,
    &mFtwRegistration
    );
  //
  // Register VssNotificationEvent () notify function.
  //
  EfiCreateProtocolNotifyEvent (
    &gEdkiiVariableStorageSupportProtocolGuid,
    TPL_CALLBACK,
    VssNotificationEvent,
    (VOID *)SystemTable,
    &mVssRegistration
    );

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  FvbVariableStorageAddressChangeEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mVirtualAddressChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
