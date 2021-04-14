/** @file
  64-bit Math Worker Function.
  The 32-bit versions of C compiler generate calls to library routines
  to handle 64-bit math. These functions use non-standard calling conventions.

Copyright (c) 2014, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/*
 * Shifts a 64-bit signed value left by a particular number of bits.
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
           ) void __cdecl
_allshl (
  void
  )
{
  _asm {
  Handle shifting  of 64 or more
    bits (
      return 0
      )
    ;

    cmp  cl, 64
    jae     short ReturnZero

    ;
    Handle shifting of  between 0 and 31 bits
    ;
    cmp  cl, 32
    jae     short More32
    shld    edx, eax, cl
    shl     eax, cl
         ret

    ;
    Handle shifting of  between 32 and 63 bits
    ;
More32:
    mov     edx, eax
    xor     eax, eax
    and     cl, 31
    shl     edx, cl
    ret

ReturnZero:
    xor     eax, eax
    xor     edx, edx
    ret
  }
}
