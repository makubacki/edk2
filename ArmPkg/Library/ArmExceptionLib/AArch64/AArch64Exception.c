/** @file
*  Exception Handling support specific for AArch64
*
*  Copyright (c) 2016 HP Development Company, L.P.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>

#include <Chipset/AArch64.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/DebugSupport.h> // for MAX_AARCH64_EXCEPTION

UINTN                   gMaxExceptionNumber = MAX_AARCH64_EXCEPTION;
EFI_EXCEPTION_CALLBACK  gExceptionHandlers[MAX_AARCH64_EXCEPTION + 1] = { 0 };
EFI_EXCEPTION_CALLBACK  gDebuggerExceptionHandlers[MAX_AARCH64_EXCEPTION + 1] = { 0 };
PHYSICAL_ADDRESS        gExceptionVectorAlignmentMask = ARM_VECTOR_TABLE_ALIGNMENT;
UINTN                   gDebuggerNoHandlerValue = 0; // todo: define for AArch64

#define EL0_STACK_SIZE  EFI_PAGES_TO_SIZE (2)
STATIC UINTN  mNewStackBase[EL0_STACK_SIZE / sizeof (UINTN)];

VOID
RegisterEl0Stack (
  IN  VOID    *Stack
  );

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
  UINTN  HcrReg;

  // Round down sp by 16 bytes alignment
  RegisterEl0Stack (
                    (VOID *) (((UINTN) mNewStackBase + EL0_STACK_SIZE) & ~0xFUL)
                    );

  if (ArmReadCurrentEL () == AARCH64_EL2) {
    HcrReg = ArmReadHcr ();

    // Trap General Exceptions. All exceptions that would be routed to EL1 are routed to EL2
    HcrReg |= ARM_HCR_TGE;

    ArmWriteHcr (HcrReg);
  }

  return RETURN_SUCCESS;
}
