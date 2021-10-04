/** @file
*
*  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/ArmGicLib.h>

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
  ARM_GIC_ARCH_REVISION  Revision;

  /*
   * Enable GIC distributor in Non-Secure world.
   * Note: The ICDDCR register is banked when Security extensions are implemented
   */
  Revision = ArmGicGetSupportedArchRevision ();
  if (Revision == ARM_GIC_ARCH_REVISION_2) {
    MmioWrite32 (GicDistributorBase + ARM_GIC_ICDDCR, 0x1);
  } else {
    if (MmioRead32 (GicDistributorBase + ARM_GIC_ICDDCR) & ARM_GIC_ICDDCR_ARE) {
      MmioOr32 (GicDistributorBase + ARM_GIC_ICDDCR, 0x2);
    } else {
      MmioOr32 (GicDistributorBase + ARM_GIC_ICDDCR, 0x1);
    }
  }
}
