/** @file
*  Exception handling support specific for ARM
*
* Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
* Copyright (c) 2014 - 2021, Arm Limited. All rights reserved.<BR>
* Copyright (c) 2016 HP Development Company, L.P.<BR>
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>

#include <Chipset/ArmV7.h>

#include <Library/ArmLib.h>

#include <Protocol/DebugSupport.h> // for MAX_ARM_EXCEPTION

UINTN                   gMaxExceptionNumber = MAX_ARM_EXCEPTION;
EFI_EXCEPTION_CALLBACK  gExceptionHandlers[MAX_ARM_EXCEPTION + 1] = { 0 };
EFI_EXCEPTION_CALLBACK  gDebuggerExceptionHandlers[MAX_ARM_EXCEPTION + 1] = { 0 };
PHYSICAL_ADDRESS        gExceptionVectorAlignmentMask = ARM_VECTOR_TABLE_ALIGNMENT;

// Exception handler contains branch to vector location (jmp $) so no handler
// NOTE: This code assumes vectors are ARM and not Thumb code
UINTN  gDebuggerNoHandlerValue = 0xEAFFFFFE;

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
RETURN_STATUS
ArchVectorConfig (
  IN  UINTN       VectorBaseAddress
  )
{
  // if the vector address corresponds to high vectors
  if (VectorBaseAddress == 0xFFFF0000) {
    // set SCTLR.V to enable high vectors
    ArmSetHighVectors ();
  } else {
    // Set SCTLR.V to 0 to enable VBAR to be used
    ArmSetLowVectors ();
  }

  return RETURN_SUCCESS;
}
