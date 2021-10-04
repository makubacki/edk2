/*++ @file
  PEIM to build GUIDed HOBs for platform specific flash map

Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2011, Apple Inc. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PiPei.h"

#include <Guid/SystemNvDataGuid.h>
#include <Ppi/EmuThunk.h>

#include <Library/DebugLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/HobLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeiServicesTablePointerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>

/**
  [TEMPLATE] - Provide a function description!

  Function overview/purpose.

  Anything a caller should be aware of must be noted in the description.

  All parameters must be described. Parameter names must be Pascal case.

  @retval must be used and each unique return code should be clearly
  described. Providing "Others" is only acceptable if a return code
  is bubbled up from a function called internal to this function. However,
  that's usually not helpful. Try to provide explicit values that mean
  something to the caller.

  Examples:
  @param[in]      ParameterName         Brief parameter description.
  @param[out]     ParameterName         Brief parameter description.
  @param[in,out]  ParameterName         Brief parameter description.

  @retval   EFI_SUCCESS                 Brief return code description.

**/
EFI_STATUS
EFIAPI
PeimInitializeFlashMap (
  IN       EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES          **PeiServices
  )

/*++

Routine Description:
  Build GUIDed HOBs for platform specific flash map

Arguments:
  FfsHeader   - A pointer to the EFI_FFS_FILE_HEADER structure.
  PeiServices - General purpose services available to every PEIM.

Returns:
  EFI_STATUS

**/
{
  EFI_STATUS              Status;
  EMU_THUNK_PPI           *Thunk;
  EFI_PEI_PPI_DESCRIPTOR  *PpiDescriptor;
  EFI_PHYSICAL_ADDRESS    FdBase;
  EFI_PHYSICAL_ADDRESS    FdFixUp;
  UINT64                  FdSize;

  DEBUG ((EFI_D_ERROR, "EmulatorPkg Flash Map PEIM Loaded\n"));

  //
  // Get the Fwh Information PPI
  //
  Status = PeiServicesLocatePpi (
             &gEmuThunkPpiGuid, // GUID
             0,                 // INSTANCE
             &PpiDescriptor,    // EFI_PEI_PPI_DESCRIPTOR
             (VOID **)&Thunk    // PPI
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Assume that FD0 contains the Flash map.
  //
  Status = Thunk->FirmwareDevices (0, &FdBase, &FdSize, &FdFixUp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PcdSet64S (PcdFlashNvStorageVariableBase64, PcdGet64 (PcdEmuFlashNvStorageVariableBase) + FdFixUp);
  PcdSet64S (PcdFlashNvStorageFtwWorkingBase64, PcdGet64 (PcdEmuFlashNvStorageFtwWorkingBase) + FdFixUp);
  PcdSet64S (PcdFlashNvStorageFtwSpareBase64, PcdGet64 (PcdEmuFlashNvStorageFtwSpareBase) + FdFixUp);

  return EFI_SUCCESS;
}
