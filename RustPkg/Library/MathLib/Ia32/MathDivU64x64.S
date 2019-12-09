#------------------------------------------------------------------------------
#
#  Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
#  This software and associated documentation (if any) is furnished
#  under a license and may only be used or copied in accordance
#  with the terms of the license. Except as permitted by such
#  license, no part of this software or documentation may be
#  reproduced, stored in a retrieval system, or transmitted in any
#  form or by any means without the express written consent of
#  Intel Corporation.
#
# Module Name:
#
#   MathDivU64x64.S
#
# Abstract:
#
#   64-bit Math Worker Function.
#   Divides a 64-bit unsigned value with a 64-bit unsigned value and returns
#   a 64-bit unsigned result.
#
#------------------------------------------------------------------------------

    .686:
    .code:

ASM_GLOBAL ASM_PFX(__udivdi3), ASM_PFX(DivU64x64Remainder)

#------------------------------------------------------------------------------
#
# void __cdecl __udivdi3 (void)
#
#------------------------------------------------------------------------------
ASM_PFX(__udivdi3):
    # Original local stack when calling __udivdi3
    #               -----------------
    #               |               |
    #               |---------------|
    #               |               |
    #               |--  Divisor  --|
    #               |               |
    #               |---------------|
    #               |               |
    #               |--  Dividend --|
    #               |               |
    #               |---------------|
    #               |  ReturnAddr** |
    #       ESP---->|---------------|
    #

    #
    # Set up the local stack for NULL Reminder pointer
    #
    xorl    %eax, %eax
    push    %eax

    #
    # Set up the local stack for Divisor parameter
    #
    movl    20(%esp), %eax
    push    %eax
    movl    20(%esp), %eax
    push    %eax

    #
    # Set up the local stack for Dividend parameter
    #
    movl    20(%esp), %eax
    push    %eax
    movl    20(%esp), %eax
    push    %eax

    #
    # Call native DivU64x64Remainder of BaseLib
    #
    jmp     ASM_PFX(DivU64x64Remainder)

    #
    # Adjust stack
    #
    addl    $20, %esp

    ret     $16
