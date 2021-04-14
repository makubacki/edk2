/** @file
  64-bit Math Worker Function.
  The 32-bit versions of C compiler generate calls to library routines
  to handle 64-bit math. These functions use non-standard calling conventions.

Copyright (c) 2014, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/*
 * Shifts a 64-bit unsigned value right by a certain number of bits.
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
_aullshr (
  void
  )
{
  _asm {
Checking: Only handle 64bit shifting or more
    ;
    cmp  cl, 64
    jae     _Exit

    ;
    Handle shifting  between 0 and 31 bits
    ;
    cmp  cl, 32
    jae     More32
    shrd    eax, edx, cl
    shr     edx, cl
         ret

    ;
    Handle shifting  of 32-63 bits
    ;
More32:
    mov     eax, edx
    xor     edx, edx
    and     cl, 31
    shr     eax, cl
    ret

    ;
    Invalid
    number (
      less then 32bits
      ), return 0
    ;

_Exit:
    xor     eax, eax
    xor     edx, edx
    ret
  }
}
