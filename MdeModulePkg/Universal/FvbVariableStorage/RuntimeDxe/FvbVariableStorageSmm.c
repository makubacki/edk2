/** @file
  This module implements Firmware Volume Block (FVB) Variable Storage Services
  and installs an instance of the VariableStorage SMM protocol.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "FvbVariableStorage.h"

#include <Protocol/SmmVariable.h>
#include <Protocol/SmmFirmwareVolumeBlock.h>
#include <Protocol/SmmFaultTolerantWrite.h>

#include <Library/SmmServicesTableLib.h>

static BOOLEAN  mWriteServiceReady                = FALSE;
static BOOLEAN  mVssNotifyWriteServiceReadyCalled = FALSE;

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
  EFI_STATUS                  Status;
  EFI_SMM_VARIABLE_PROTOCOL   *SmmVariableProtocol;

  Status = gSmst->SmmLocateProtocol (
                    &gEfiSmmVariableProtocolGuid,
                    NULL,
                    (VOID **) &SmmVariableProtocol
                    );
  if (!EFI_ERROR (Status)) {
    Status = SmmVariableProtocol->SmmGetVariable (
                                    VariableName,
                                    VendorGuid,
                                    Attributes,
                                    DataSize,
                                    Data
                                    );
  } else {
    DEBUG ((DEBUG_WARN, "CoreGetVariable: Error getting EFI_SMM_VARIABLE_PROTOCOL: %r\n", Status));
  }
  return Status;
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
  EFI_STATUS                  Status;
  EFI_SMM_VARIABLE_PROTOCOL   *SmmVariableProtocol;

  Status = gSmst->SmmLocateProtocol (
                    &gEfiSmmVariableProtocolGuid,
                    NULL,
                    (VOID **) &SmmVariableProtocol
                    );
  if (!EFI_ERROR (Status)) {
    Status = SmmVariableProtocol->SmmSetVariable (
                                    VariableName,
                                    VendorGuid,
                                    Attributes,
                                    DataSize,
                                    Data
                                    );
  } else {
    DEBUG ((DEBUG_WARN, "CoreSetVariable: Error getting EFI_SMM_VARIABLE_PROTOCOL: %r\n", Status));
  }
  return Status;
}

/**
  Retrive the SMM Fault Tolerent Write protocol interface.

  @param[out] FtwProtocol       The interface of SMM Ftw protocol

  @retval EFI_SUCCESS           The SMM FTW protocol instance was found and returned in FtwProtocol.
  @retval EFI_NOT_FOUND         The SMM FTW protocol instance was not found.
  @retval EFI_INVALID_PARAMETER SarProtocol is NULL.

**/
EFI_STATUS
GetFtwProtocol (
  OUT VOID                                **FtwProtocol
  )
{
  EFI_STATUS                              Status;

  //
  // Locate Smm Fault Tolerent Write protocol
  //
  Status = gSmst->SmmLocateProtocol (
                    &gEfiSmmFaultTolerantWriteProtocolGuid,
                    NULL,
                    FtwProtocol
                    );
  return Status;
}

/**
  Retrive the SMM FVB protocol interface by HANDLE.

  @param[in]  FvBlockHandle     The handle of SMM FVB protocol that provides services for
                                reading, writing, and erasing the target block.
  @param[out] FvBlock           The interface of SMM FVB protocol

  @retval EFI_SUCCESS           The interface information for the specified protocol was returned.
  @retval EFI_UNSUPPORTED       The device does not support the SMM FVB protocol.
  @retval EFI_INVALID_PARAMETER FvBlockHandle is not a valid EFI_HANDLE or FvBlock is NULL.

**/
EFI_STATUS
GetFvbByHandle (
  IN  EFI_HANDLE                          FvBlockHandle,
  OUT EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  **FvBlock
  )
{
  //
  // To get the SMM FVB protocol interface on the handle
  //
  return gSmst->SmmHandleProtocol (
                  FvBlockHandle,
                  &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                  (VOID **) FvBlock
                  );
}

/**
  Function returns an array of handles that support the SMM FVB protocol
  in a buffer allocated from pool.

  @param[out]  NumberHandles    The number of handles returned in Buffer.
  @param[out]  Buffer           A pointer to the buffer to return the requested
                                array of  handles that support SMM FVB protocol.

  @retval EFI_SUCCESS           The array of handles was returned in Buffer, and the number of
                                handles in Buffer was returned in NumberHandles.
  @retval EFI_NOT_FOUND         No SMM FVB handle was found.
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
  UINTN                                   BufferSize;

  if ((NumberHandles == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  BufferSize     = 0;
  *NumberHandles = 0;
  *Buffer        = NULL;
  Status = gSmst->SmmLocateHandle (
                    ByProtocol,
                    &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                    NULL,
                    &BufferSize,
                    *Buffer
                    );
  if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
    return EFI_NOT_FOUND;
  }

  *Buffer = AllocatePool (BufferSize);
  if (*Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gSmst->SmmLocateHandle (
                    ByProtocol,
                    &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                    NULL,
                    &BufferSize,
                    *Buffer
                    );

  *NumberHandles = BufferSize / sizeof(EFI_HANDLE);
  if (EFI_ERROR(Status)) {
    *NumberHandles = 0;
    FreePool (*Buffer);
    *Buffer = NULL;
  }

  return Status;
}

/**
  SMM Fault Tolerant Write protocol notification event handler.

  Non-Volatile variable write may needs FTW protocol to reclaim when
  writting variable.

  @param  Protocol   Points to the protocol's unique identifier
  @param  Interface  Points to the interface instance
  @param  Handle     The handle on which the interface was installed

  @retval EFI_SUCCESS   SmmEventCallback runs successfully
  @retval EFI_NOT_FOUND The Fvb protocol for variable is not found.

 **/
EFI_STATUS
EFIAPI
SmmFtwNotificationEvent (
  IN CONST EFI_GUID                       *Protocol,
  IN VOID                                 *Interface,
  IN EFI_HANDLE                           Handle
  )
{
  EFI_STATUS                              Status;
  EFI_SMM_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *FvbProtocol;
  EFI_SMM_FAULT_TOLERANT_WRITE_PROTOCOL   *FtwProtocol;
  EFI_PHYSICAL_ADDRESS                    NvStorageVariableBase;
  UINTN                                   FtwMaxBlockSize;

  if (mFvbInstance != NULL) {
    return EFI_SUCCESS;
  }

  //
  // Ensure SMM FTW protocol is installed.
  //
  Status = GetFtwProtocol ((VOID **) &FtwProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
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
    return EFI_NOT_FOUND;
  }

  mFvbInstance = FvbProtocol;

  Status = FvbVariableStorageWriteServiceInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "FVB write service initialization failed. Status = %r\n", Status));
  } else {
    //
    // Notify the variable driver that FVB writes are ready
    //
    if (mVariableStorageSupport != NULL) {
      mVariableStorageSupport->NotifyWriteServiceReady ();
      mVssNotifyWriteServiceReadyCalled = TRUE;
    } else {
      mWriteServiceReady = TRUE;
    }
  }

  return Status;
}

/**
  SMM Variable Storage Support protocol notification event handler.

  The VSS protocol is needed to enable Non-Volatile variable writes

  @param  Protocol   Points to the protocol's unique identifier
  @param  Interface  Points to the interface instance
  @param  Handle     The handle on which the interface was installed

  @retval EFI_SUCCESS   SmmEventCallback runs successfully
  @retval EFI_NOT_FOUND The Vss protocol is not found.

 **/
EFI_STATUS
EFIAPI
SmmVssNotificationEvent (
  IN CONST EFI_GUID                       *Protocol,
  IN VOID                                 *Interface,
  IN EFI_HANDLE                           Handle
  )
{
  EFI_STATUS                              Status;

  if (mVssNotifyWriteServiceReadyCalled) {
    return EFI_SUCCESS;
  }

  //
  // Locate Smm Variable Storage Support protocol
  //
  Status = gSmst->SmmLocateProtocol (
                    &gEdkiiVariableStorageSupportProtocolGuid,
                    NULL,
                    &mVariableStorageSupport
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  if (mVariableStorageSupport == NULL) {
    return EFI_NOT_FOUND;
  }
  if (mWriteServiceReady) {
    mVariableStorageSupport->NotifyWriteServiceReady ();
    mVssNotifyWriteServiceReadyCalled = TRUE;
  }
  return EFI_SUCCESS;
}

/**
  Provide SMM functionality of CSE NVM variable storage services.

  @param  ImageHandle  The image handle.
  @param  SystemTable  The system table.

  @retval EFI_SUCCESS  The protocol was installed successfully.
  @retval Others       Protocol could not be installed.
**/
EFI_STATUS
EFIAPI
FvbVariableStorageSmmInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *SmmFtwRegistration;
  VOID        *SmmVssRegistration;
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

  //
  // Install the EDKII_VARIABLE_STORAGE_PROTOCOL
  //
  Status = gSmst->SmmInstallProtocolInterface (
                    &Handle,
                    &gEdkiiVariableStorageProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &mFvbVariableStorageProtocol
                    );

  //
  // Register FtwNotificationEvent () notify function.
  //
  Status = gSmst->SmmRegisterProtocolNotify (
                    &gEfiSmmFaultTolerantWriteProtocolGuid,
                    SmmFtwNotificationEvent,
                    &SmmFtwRegistration
                    );
  ASSERT_EFI_ERROR (Status);
  //
  // Register FtwNotificationEvent () notify function.
  //
  Status = gSmst->SmmRegisterProtocolNotify (
                    &gEdkiiVariableStorageSupportProtocolGuid,
                    SmmVssNotificationEvent,
                    &SmmVssRegistration
                    );
  ASSERT_EFI_ERROR (Status);

  SmmFtwNotificationEvent (NULL, NULL, NULL);
  SmmVssNotificationEvent (NULL, NULL, NULL);

  return Status;
}
