/** @file
  Common variable non-volatile store routines.

  Caution: This module requires additional review when modified.
  This driver will have external input - variable data. They may be input in SMM mode.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "VariableHelpers.h"
#include "VariableNonVolatile.h"

extern VOID                     *mReclaimBuffer;
extern UINT32                   mReclaimBufferSize;
extern VARIABLE_MODULE_GLOBAL   *mVariableModuleGlobal;
extern CHAR16                   mVariableNameBuffer[MAX_VARIABLE_NAME_SIZE];


/**
  Get non-volatile maximum variable size.

  @return Non-volatile maximum variable size.

**/
UINTN
GetNonVolatileMaxVariableSize (
  VOID
  )
{
  if (PcdGet32 (PcdHwErrStorageSize) != 0) {
    return MAX (MAX (PcdGet32 (PcdMaxVariableSize), PcdGet32 (PcdMaxAuthVariableSize)),
                PcdGet32 (PcdMaxHardwareErrorVariableSize));
  } else {
    return MAX (PcdGet32 (PcdMaxVariableSize), PcdGet32 (PcdMaxAuthVariableSize));
  }
}

/**
  Finds the first variable from the first NV storage

  @param[out]  PtrTrack               VARIABLE_POINTER_TRACK structure for output,
                                      including the range searched and the target position.
  @param[in]   Global                 Pointer to VARIABLE_GLOBAL structure, including
                                      base of volatile variable storage area, base of
                                      NV variable storage area, and a lock.
  @param[in]   IgnoreRtCheck          Ignore EFI_VARIABLE_RUNTIME_ACCESS attribute
                                      check at runtime when searching variable.
  @param[out]  CommandInProgress      TRUE if the command requires asyncronous I/O and has not completed yet.
                                      If this parameter is TRUE, then PtrTrack will not be updated and will
                                      not contain valid data.  Asyncronous I/O should only be required during
                                      OS runtime phase, this return value will be FALSE during all Pre-OS stages.
                                      If CommandInProgress is returned TRUE, then this function will return EFI_SUCCESS
  @param[out]  InProgressInstanceGuid If CommandInProgress is TRUE, this will contain the instance GUID of the Variable
                                      Storage driver that is performing the asyncronous I/O

  @retval EFI_INVALID_PARAMETER       If VariableName is not an empty string, while
                                      VendorGuid is NULL.
  @retval EFI_SUCCESS                 Variable successfully found.
  @retval EFI_NOT_FOUND               Variable not found

**/
EFI_STATUS
FindFirstNvVariable (
  OUT VARIABLE_POINTER_TRACK  *PtrTrack,
  IN  VARIABLE_GLOBAL         *Global,
  IN  BOOLEAN                 IgnoreRtCheck,
  OUT BOOLEAN                 *CommandInProgress,
  OUT EFI_GUID                *InProgressInstanceGuid
  )
{
  EDKII_VARIABLE_STORAGE_PROTOCOL   *VariableStorage;
  UINTN                             VariableNameBufferSize;
  UINTN                             Index;
  UINT32                            VariableAttributes;
  EFI_GUID                          TempGuid;
  EFI_STATUS                        Status;

  Status                  = EFI_NOT_FOUND;
  VariableNameBufferSize  = sizeof (mVariableNameBuffer);
  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    ZeroMem ((VOID *) &mVariableNameBuffer[0], VariableNameBufferSize);
    ZeroMem ((VOID *) &TempGuid, sizeof (EFI_GUID));
    VariableStorage = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
    Status = VariableStorage->GetNextVariableName (
                                VariableStorage,
                                &VariableNameBufferSize,
                                &mVariableNameBuffer[0],
                                &TempGuid,
                                &VariableAttributes
                                );
    if (EFI_ERROR (Status) && Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_INFO, "VariableStorageProtocol->GetNextVariableName status %r\n", Status));
      ASSERT (Status != EFI_BUFFER_TOO_SMALL);
      return Status;
    }
  }
  if (EFI_ERROR (Status)) {
    return Status;
  } else {
    return FindVariable (
             &mVariableNameBuffer[0],
             &TempGuid,
             PtrTrack,
             Global,
             IgnoreRtCheck,
             CommandInProgress,
             InProgressInstanceGuid
             );
  }
}

/**
  Calculates the total variable storage size for non-volatile variable storage.

  @param[out]  TotalNonVolatileStorageSpace   The total size in bytes calculated for non-volatile storage.

  @retval EFI_SUCCESS                         The size was determined successfully.
  @retval Others                              The size could not be determined successfully.

**/
EFI_STATUS
EFIAPI
GetTotalNonVolatileVariableStorageSize (
  OUT UINTN *TotalNonVolatileStorageSize
  )
{
  EFI_STATUS                          Status;
  UINTN                               Index;
  UINT32                              CommonVariablesTotalSize;
  UINT32                              HwErrVariablesTotalSize;
  UINT32                              VariableStoreSize;
  EDKII_VARIABLE_STORAGE_PROTOCOL     *VariableStorageProtocol;

  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
    Status = VariableStorageProtocol->GetStorageUsage (
                                        VariableStorageProtocol,
                                        AtRuntime (),
                                        &VariableStoreSize,
                                        &CommonVariablesTotalSize,
                                        &HwErrVariablesTotalSize
                                        );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    DEBUG ((EFI_D_VERBOSE, "+-+-> Variable Driver: Data from StorageUsage VariableStorageProtocol[%d]:\n  VariableStoreSize = %d bytes.\n  CommonVariablesTotalSize = %d bytes.  HwErrVariablesTotalSize = %d bytes.\n",
            Index, VariableStoreSize, CommonVariablesTotalSize, HwErrVariablesTotalSize));
  }
  *TotalNonVolatileStorageSize = VariableStoreSize;

  return EFI_SUCCESS;
}

/**
  Init non-volatile variable store.

  @retval EFI_SUCCESS           Function successfully executed.
  @retval EFI_OUT_OF_RESOURCES  Fail to allocate enough memory resource.
  @retval EFI_VOLUME_CORRUPTED  Variable Store or Firmware Volume for Variable Store is corrupted.

**/
EFI_STATUS
InitNonVolatileVariableStore (
  VOID
  )
{
  EFI_STATUS                        Status;
  UINTN                             Index;
  EDKII_VARIABLE_STORAGE_PROTOCOL   *VariableStorageProtocol;
  UINT32                            NvStoreTotalSize;
  UINT32                            VariableStoreSize;
  UINT32                            CommonVariablesTotalSize;
  UINT32                            HwErrVariablesTotalSize;
  UINT32                            HwErrStorageSize;
  UINT32                            MaxUserNvVariableSpaceSize;
  UINT32                            BoottimeReservedNvVariableSpaceSize;

  //
  // Get the total NV storage size from all EDKII_VARIABLE_STORAGE_PROTOCOLs, and the amount of space
  // used
  //
  NvStoreTotalSize = 0;

  if (!mNvVariableEmulationMode) {
    for ( Index = 0;
          Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
          Index++) {
      VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
      Status = VariableStorageProtocol->GetStorageUsage (
                                          VariableStorageProtocol,
                                          AtRuntime (),
                                          &VariableStoreSize,
                                          &CommonVariablesTotalSize,
                                          &HwErrVariablesTotalSize
                                          );
      ASSERT_EFI_ERROR (Status);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      NvStoreTotalSize                                += VariableStoreSize;
      mVariableModuleGlobal->CommonVariableTotalSize  += CommonVariablesTotalSize;
      mVariableModuleGlobal->HwErrVariableTotalSize   += HwErrVariablesTotalSize;
    }
  } else {
    NvStoreTotalSize                                = DEFAULT_NV_STORE_SIZE;
    mVariableModuleGlobal->CommonVariableTotalSize  = 0;
    mVariableModuleGlobal->HwErrVariableTotalSize   = 0;
  }
  ALIGN_VALUE (NvStoreTotalSize, sizeof (UINT32));

  DEBUG ((EFI_D_VERBOSE, "+-+-> Variable Driver: Total data from StorageUsage:\n  NvStoreTotalSize = %d bytes.\n  CommonVariablesTotalSize = %d bytes.  HwErrVariablesTotalSize = %d bytes.\n",
            NvStoreTotalSize, mVariableModuleGlobal->CommonVariableTotalSize, mVariableModuleGlobal->HwErrVariableTotalSize));

  //
  // Allocate the Reclaim Buffer and initialize it to all 1's (like an erased FV)
  // The reclaim buffer is used to rebuild the NV Storage Cache with all the unused data freed
  //
  mReclaimBufferSize  = ALIGN_VALUE (MAX (NvStoreTotalSize, PcdGet32 (PcdVariableStoreSize)), sizeof (UINT32));
  mReclaimBuffer      = AllocateRuntimePool (mReclaimBufferSize);
  if (mReclaimBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  SetMem32 (mReclaimBuffer, mReclaimBufferSize, (UINT32) 0xFFFFFFFF);

  //
  // Initialize mVariableModuleGlobal
  //
  HwErrStorageSize = PcdGet32 (PcdHwErrStorageSize);
  MaxUserNvVariableSpaceSize = PcdGet32 (PcdMaxUserNvVariableSpaceSize);
  BoottimeReservedNvVariableSpaceSize = PcdGet32 (PcdBoottimeReservedNvVariableSpaceSize);
  //
  // Note that in EdkII variable driver implementation, Hardware Error Record type variable
  // is stored with common variable in the same NV region. So the platform integrator should
  // ensure that the value of PcdHwErrStorageSize is less than the value of
  // (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER)).
  //
  ASSERT (HwErrStorageSize < (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER)));
  //
  // Ensure that the value of PcdMaxUserNvVariableSpaceSize is less than the value of
  // (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER)) - PcdGet32 (PcdHwErrStorageSize).
  //
  ASSERT (MaxUserNvVariableSpaceSize < (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER) - HwErrStorageSize));
  //
  // Ensure that the value of PcdBoottimeReservedNvVariableSpaceSize is less than the value of
  // (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER)) - PcdGet32 (PcdHwErrStorageSize).
  //
  ASSERT (BoottimeReservedNvVariableSpaceSize < (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER) - HwErrStorageSize));

  mVariableModuleGlobal->CommonVariableSpace = ((UINTN) NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER) - HwErrStorageSize);
  mVariableModuleGlobal->CommonMaxUserVariableSpace = ((MaxUserNvVariableSpaceSize != 0) ? MaxUserNvVariableSpaceSize : mVariableModuleGlobal->CommonVariableSpace);
  mVariableModuleGlobal->CommonRuntimeVariableSpace = mVariableModuleGlobal->CommonVariableSpace - BoottimeReservedNvVariableSpaceSize;

  DEBUG ((EFI_D_VERBOSE, "  Variable Driver: Variable driver common space: 0x%x 0x%x 0x%x\n", mVariableModuleGlobal->CommonVariableSpace, mVariableModuleGlobal->CommonMaxUserVariableSpace, mVariableModuleGlobal->CommonRuntimeVariableSpace));

  //
  // The max NV variable size should be < (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER)).
  //
  ASSERT (GetNonVolatileMaxVariableSize () < (NvStoreTotalSize - sizeof (VARIABLE_STORE_HEADER)));

  mVariableModuleGlobal->MaxVariableSize = PcdGet32 (PcdMaxVariableSize);
  mVariableModuleGlobal->MaxAuthVariableSize = ((PcdGet32 (PcdMaxAuthVariableSize) != 0) ? PcdGet32 (PcdMaxAuthVariableSize) : mVariableModuleGlobal->MaxVariableSize);

  return EFI_SUCCESS;
}
