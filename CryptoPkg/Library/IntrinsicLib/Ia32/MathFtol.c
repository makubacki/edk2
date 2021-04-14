/** @file
  64-bit Math Worker Function.
  The 32-bit versions of C compiler generate calls to library routines
  to handle 64-bit math. These functions use non-standard calling conventions.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/*
 * Floating point to integer conversion.
 */
__declspec(naked/**
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
           ) void
_ftol2 (
  void
  )
{
  _asm {
  fistp qword  ptr[esp-8]
    mov   edx, [esp-4]
    mov   eax, [esp-8]
    ret
  }
}
