/** @file
  NULL Tcg2PhysicalPresenceLib library instance

  Copyright (c) 2018, Red Hat, Inc.
  Copyright (c) 2013 - 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/Tcg2PhysicalPresenceLib.h>

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
Tcg2PhysicalPresenceLibProcessRequest (
  IN      TPM2B_AUTH                     *PlatformAuth  OPTIONAL
  )
{
  //
  // do nothing
  //
}
