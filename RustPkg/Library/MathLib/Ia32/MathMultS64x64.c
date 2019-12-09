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

#include <Library/BaseLib.h>

/*
 * Multiplies a 64-bit signed or unsigned value by a 64-bit signed or unsigned value
 * and returns a 64-bit result.
 */
__declspec(naked) void __cdecl _allmul (void)
{
  //
  // Wrapper Implementation over EDKII MultS64x64() routine
  //    INT64
  //    EFIAPI
  //    MultS64x64 (
  //      IN      INT64      Multiplicand,
  //      IN      INT64      Multiplier
  //      )
  //
  _asm {
    ; Original local stack when calling _allmul
    ;               -----------------
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--Multiplier --|
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--Multiplicand-|
    ;               |               |
    ;               |---------------|
    ;               |  ReturnAddr** |
    ;       ESP---->|---------------|
    ;

    ;
    ; Set up the local stack for Multiplicand parameter
    ;
    mov  eax, [esp + 16]
    push eax
    mov  eax, [esp + 16]
    push eax

    ;
    ; Set up the local stack for Multiplier parameter
    ;
    mov  eax, [esp + 16]
    push eax
    mov  eax, [esp + 16]
    push eax

    ;
    ; Call native MulS64x64 of BaseLib
    ;
    call MultS64x64

    ;
    ; Adjust stack
    ;
    add  esp, 16

    ret  16
  }
} 
