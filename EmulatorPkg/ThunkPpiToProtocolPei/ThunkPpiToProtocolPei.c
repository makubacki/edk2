/*++ @file
  UEFI/PI PEIM to abstract construction of firmware volume in a Unix environment.

Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2010 - 2011, Apple Inc. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/DebugLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/HobLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeiServicesTablePointerLib.h>

#include <Ppi/EmuThunk.h>
#include <Protocol/EmuThunk.h>

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
PeiInitialzeThunkPpiToProtocolPei (
  IN       EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES          **PeiServices
  )

/*++

Routine Description:

  Perform a call-back into the SEC simulator to get Unix Stuff

Arguments:

  PeiServices - General purpose services available to every PEIM.

Returns:

  None

**/
{
  EFI_STATUS              Status;
  EFI_PEI_PPI_DESCRIPTOR  *PpiDescriptor;
  EMU_THUNK_PPI           *Thunk;
  VOID                    *Ptr;

  DEBUG ((EFI_D_ERROR, "Emu Thunk PEIM Loaded\n"));

  Status = PeiServicesLocatePpi (
             &gEmuThunkPpiGuid,         // GUID
             0,                         // INSTANCE
             &PpiDescriptor,            // EFI_PEI_PPI_DESCRIPTOR
             (VOID **)&Thunk            // PPI
             );
  ASSERT_EFI_ERROR (Status);

  Ptr = Thunk->Thunk ();

  BuildGuidDataHob (
    &gEmuThunkProtocolGuid,              // Guid
    &Ptr,                                // Buffer
    sizeof (VOID *)                      // Sizeof Buffer
    );
  return Status;
}
