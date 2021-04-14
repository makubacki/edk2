/** @file
*
*  Copyright (c) 2011-2014, ARM Limited. All rights reserved.
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
ArmGicV2EnableInterruptInterface (
  IN  INTN          GicInterruptInterfaceBase
  )
{
  /*
  * Enable the CPU interface in Non-Secure world
  * Note: The ICCICR register is banked when Security extensions are implemented
  */
  MmioWrite32 (GicInterruptInterfaceBase + ARM_GIC_ICCICR, 0x1);
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
ArmGicV2DisableInterruptInterface (
  IN  INTN          GicInterruptInterfaceBase
  )
{
  // Disable Gic Interface
  MmioWrite32 (GicInterruptInterfaceBase + ARM_GIC_ICCICR, 0x0);
  MmioWrite32 (GicInterruptInterfaceBase + ARM_GIC_ICCPMR, 0x0);
}
