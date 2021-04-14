/*++ @file

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2011, Apple Inc. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PiPei.h"
#include <Ppi/EmuThunk.h>
#include <Ppi/MemoryDiscovered.h>

#include <Library/DebugLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeiServicesTablePointerLib.h>

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
PeimInitializeAutoScanPei (
  IN       EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES          **PeiServices
  )

/*++

Routine Description:
  Perform a call-back into the SEC simulator to get a memory value

Arguments:
  FfsHeader   - General purpose data available to every PEIM
  PeiServices - General purpose services available to every PEIM.

Returns:
  None

**/
{
  EFI_STATUS                   Status;
  EFI_PEI_PPI_DESCRIPTOR       *PpiDescriptor;
  EMU_THUNK_PPI                *Thunk;
  UINT64                       MemorySize;
  EFI_PHYSICAL_ADDRESS         MemoryBase;
  UINTN                        Index;
  EFI_RESOURCE_ATTRIBUTE_TYPE  Attributes;

  DEBUG ((EFI_D_ERROR, "Emu Autoscan PEIM Loaded\n"));

  //
  // Get the PEI UNIX Autoscan PPI
  //
  Status = PeiServicesLocatePpi (
                                 &gEmuThunkPpiGuid, // GUID
                                 0,                 // INSTANCE
                                 &PpiDescriptor,    // EFI_PEI_PPI_DESCRIPTOR
                                 (VOID **) &Thunk   // PPI
                                 );
  ASSERT_EFI_ERROR (Status);

  Index = 0;
  do {
    Status = Thunk->MemoryAutoScan (Index, &MemoryBase, &MemorySize);
    if (!EFI_ERROR (Status)) {
      Attributes =
        (
         EFI_RESOURCE_ATTRIBUTE_PRESENT |
         EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
         EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
         EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
         EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
         EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE
        );

      if (Index == 0) {
        //
        // Register the memory with the PEI Core
        //
        Status = PeiServicesInstallPeiMemory (MemoryBase, MemorySize);
        ASSERT_EFI_ERROR (Status);

        Attributes |= EFI_RESOURCE_ATTRIBUTE_TESTED;
      }

      BuildResourceDescriptorHob (
                                  EFI_RESOURCE_SYSTEM_MEMORY,
                                  Attributes,
                                  MemoryBase,
                                  MemorySize
                                  );
    }

    Index++;
  } while (!EFI_ERROR (Status));

  //
  // Build the CPU hob with 57-bit addressing and 16-bits of IO space.
  //
  BuildCpuHob (57, 16);

  return Status;
}
