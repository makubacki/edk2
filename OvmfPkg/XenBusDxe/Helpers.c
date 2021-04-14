/** @file
  [TEMPLATE] - Provide a file description!

  Describe the purpose of the file. Try to make the description as
  specific to this file as possible. Do not copy/paste the same
  description between all files in a driver or library.

  Copyright (c) Microsoft Corporation.

**/
#include "XenBusDxe.h"

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
CHAR8 *
AsciiStrDup (
  IN CONST CHAR8 *Str
  )
{
  return AllocateCopyPool (AsciiStrSize (Str), Str);
}
