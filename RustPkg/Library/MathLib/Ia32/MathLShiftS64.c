/** @file
  64-bit Math Worker Function.
  The 32-bit versions of C compiler generate calls to library routines
  to handle 64-bit math. These functions use non-standard calling conventions.

  Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
  This software and associated documentation (if any) is furnished
  under a license and may only be used or copied in accordance
  with the terms of the license. Except as permitted by such
  license, no part of this software or documentation may be
  reproduced, stored in a retrieval system, or transmitted in any
  form or by any means without the express written consent of
  Intel Corporation.

**/


/*
 * Shifts a 64-bit signed value left by a particular number of bits.
 */
__declspec(naked) void __cdecl _allshl (void)
{
  _asm {
    ;
    ; Handle shifting of 64 or more bits (return 0)
    ;
    cmp     cl, 64
    jae     short ReturnZero

    ;
    ; Handle shifting of between 0 and 31 bits
    ;
    cmp     cl, 32              
    jae     short More32
    shld    edx, eax, cl
    shl     eax, cl
    ret

    ;
    ; Handle shifting of between 32 and 63 bits
    ;
More32:
    mov     edx, eax
    xor     eax, eax
    and     cl, 31
    shl     edx, cl
    ret

ReturnZero:
    xor     eax,eax
    xor     edx,edx
    ret
  }
}
