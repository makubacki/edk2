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
 * Divides a 64-bit unsigned value with a 64-bit unsigned value and returns
 * a 64-bit unsigned result and 64-bit unsigned remainder.
 */
__declspec(naked) void __cdecl _aulldvrm (void)
{
  //
  // Wrapper Implementation over EDKII DivU64x64Reminder() routine
  //    UINT64
  //    EFIAPI
  //    DivU64x64Remainder (
  //      IN      UINT64     Dividend,
  //      IN      UINT64     Divisor,
  //      OUT     UINT64     *Remainder  OPTIONAL
  //      )
  //
  _asm {

    ; Original local stack when calling _aulldvrm
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
    ; Call native DivU64x64Remainder of BaseLib
    ;
    call DivU64x64Remainder

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
