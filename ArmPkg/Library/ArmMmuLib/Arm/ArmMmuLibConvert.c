/** @file
*  File managing the MMU for ARMv7 architecture
*
*  Copyright (c) 2011-2016, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>

#include <Library/ArmLib.h>

#include <Chipset/ArmV7.h>

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
UINT32
ConvertSectionAttributesToPageAttributes (
  IN UINT32   SectionAttributes,
  IN BOOLEAN  IsLargePage
  )
{
  UINT32  PageAttributes;

  PageAttributes  = 0;
  PageAttributes |= TT_DESCRIPTOR_CONVERT_TO_PAGE_CACHE_POLICY (SectionAttributes, IsLargePage);
  PageAttributes |= TT_DESCRIPTOR_CONVERT_TO_PAGE_AP (SectionAttributes);
  PageAttributes |= TT_DESCRIPTOR_CONVERT_TO_PAGE_XN (SectionAttributes, IsLargePage);
  PageAttributes |= TT_DESCRIPTOR_CONVERT_TO_PAGE_NG (SectionAttributes);
  PageAttributes |= TT_DESCRIPTOR_CONVERT_TO_PAGE_S (SectionAttributes);

  return PageAttributes;
}
