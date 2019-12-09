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
#   MathDivS64x64.S
#
# Abstract:
#
#   64-bit Math Worker Function.
#   Divides a 64-bit signed value with a 64-bit signed value and returns
#   a 64-bit signed result.
#
#------------------------------------------------------------------------------

    .686:
    .code:

ASM_GLOBAL ASM_PFX(__divdi3), ASM_PFX(DivS64x64Remainder)

#------------------------------------------------------------------------------
#
# void __cdecl __divdi3 (void)
#
#------------------------------------------------------------------------------
ASM_PFX(__divdi3):
    # Original local stack when calling __divdi3
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
    # Call native DivS64x64Remainder of BaseLib
    #
    jmp     ASM_PFX(DivS64x64Remainder)

    #
    # Adjust stack
    #
    addl    $20, %esp

    ret     $16
