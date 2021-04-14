/** @file
*
*  Copyright (c) 2014, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/ArmLib.h>
#include <Library/ArmGicLib.h>

STATIC ARM_GIC_ARCH_REVISION  mGicArchRevision;

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
EFIAPI
ArmGicArchLibInitialize (
  VOID
  )
{
  UINT32  IccSre;

  // Ideally we would like to use the GICC IIDR Architecture version here, but
  // this does not seem to be very reliable as the implementation could easily
  // get it wrong. It is more reliable to check if the GICv3 System Register
  // feature is implemented on the CPU. This is also convenient as our GICv3
  // driver requires SRE. If only Memory mapped access is available we try to
  // drive the GIC as a v2.
  if (ArmHasGicSystemRegisters ()) {
    // Make sure System Register access is enabled (SRE). This depends on the
    // higher privilege level giving us permission, otherwise we will either
    // cause an exception here, or the write doesn't stick in which case we need
    // to fall back to the GICv2 MMIO interface.
    // Note: We do not need to set ICC_SRE_EL2.Enable because the OS is started
    // at the same exception level.
    // It is the OS responsibility to set this bit.
    IccSre = ArmGicV3GetControlSystemRegisterEnable ();
    if (!(IccSre & ICC_SRE_EL2_SRE)) {
      ArmGicV3SetControlSystemRegisterEnable (IccSre | ICC_SRE_EL2_SRE);
      IccSre = ArmGicV3GetControlSystemRegisterEnable ();
    }

    if (IccSre & ICC_SRE_EL2_SRE) {
      mGicArchRevision = ARM_GIC_ARCH_REVISION_3;
      goto Done;
    }
  }

  mGicArchRevision = ARM_GIC_ARCH_REVISION_2;

Done:
  return RETURN_SUCCESS;
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
ARM_GIC_ARCH_REVISION
EFIAPI
ArmGicGetSupportedArchRevision (
  VOID
  )
{
  return mGicArchRevision;
}
