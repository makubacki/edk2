/** @file
*
*  Copyright (c) 2011-2014, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/ArmGicLib.h>

/*
 * This function configures the interrupts set by the mask to be secure.
 *
 */
VOID
EFIAPI
ArmGicSetSecureInterrupts (
  IN  UINTN         GicDistributorBase,
  IN  UINTN *GicSecureInterruptMask,
  IN  UINTN         GicSecureInterruptMaskSize
  )
{
  UINTN   Index;
  UINT32  InterruptStatus;

  // We must not have more interrupts defined by the mask than the number of available interrupts
  ASSERT (GicSecureInterruptMaskSize <= (ArmGicGetMaxNumInterrupts (GicDistributorBase) / 32));

  // Set all the interrupts defined by the mask as Secure
  for (Index = 0; Index < GicSecureInterruptMaskSize; Index++) {
    InterruptStatus = MmioRead32 (GicDistributorBase + ARM_GIC_ICDISR + (Index * 4));
    MmioWrite32 (GicDistributorBase + ARM_GIC_ICDISR + (Index * 4), InterruptStatus & (~GicSecureInterruptMask[Index]));
  }
}

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
VOID
EFIAPI
ArmGicEnableDistributor (
  IN  INTN          GicDistributorBase
  )
{
  // Turn on the GIC distributor
  MmioWrite32 (GicDistributorBase + ARM_GIC_ICDDCR, 1);
}

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
VOID
EFIAPI
ArmGicSetupNonSecure (
  IN  UINTN         MpId,
  IN  INTN          GicDistributorBase,
  IN  INTN          GicInterruptInterfaceBase
  )
{
  ArmGicV2SetupNonSecure (MpId, GicDistributorBase, GicInterruptInterfaceBase);
}
