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
 * Divides a 64-bit signed value with a 64-bit signed value and returns
 * a 64-bit signed result and 64-bit signed remainder.
 */
__declspec(naked) void __cdecl _alldvrm (void)
{
  //
  // Wrapper Implementation over EDKII DivS64x64Reminder() routine
  //    INT64
  //    EFIAPI
  //    DivS64x64Remainder (
  //      IN      INT64     Dividend,
  //      IN      INT64     Divisor,
  //      OUT     INT64     *Remainder  OPTIONAL
  //      )
  //
  _asm {

    ; Original local stack when calling _alldvrm
    ;               -----------------
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--  Divisor  --|
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--  Dividend --|
    ;               |               |
    ;               |---------------|
    ;               |  ReturnAddr** |
    ;       ESP---->|---------------|
    ;

    ;
    ; Set up the local stack for Reminder pointer
    ;
    sub  esp, 8
    push esp

    ;
    ; Set up the local stack for Divisor parameter
    ;
    mov  eax, [esp + 28]
    push eax
    mov  eax, [esp + 28]
    push eax

    ;
    ; Set up the local stack for Dividend parameter
    ;
    mov  eax, [esp + 28]
    push eax
    mov  eax, [esp + 28]
    push eax

    ;
    ; Call native DivS64x64Remainder of BaseLib
    ;
    call DivS64x64Remainder

    ;
    ; Put the Reminder in EBX:ECX as return value
    ;
    mov  ecx, [esp + 20]
    mov  ebx, [esp + 24]

    ;
    ; Adjust stack
    ;
    add  esp, 28

    ret  16
  }
}
