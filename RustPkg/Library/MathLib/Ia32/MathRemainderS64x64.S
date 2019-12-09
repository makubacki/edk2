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
#   MathRemainderS64x64.S
#
# Abstract:
#
#   64-bit Math Worker Function.
#   Divides a 64-bit signed value by another 64-bit signed value and returns
#   the 64-bit signed remainder
#
#------------------------------------------------------------------------------

    .686:
    .code:

ASM_GLOBAL ASM_PFX(__moddi3), ASM_PFX(DivS64x64Remainder)

#------------------------------------------------------------------------------
#
# void __cdecl __moddi3 (void)
#
#------------------------------------------------------------------------------
ASM_PFX(__moddi3):
    # Original local stack when calling __moddi3
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
    # Set up the local stack for Reminder pointer
    #
    sub     $8, %esp
    push    %esp

    #
    # Set up the local stack for Divisor parameter
    #
    movl    28(%esp), %eax
    push    %eax
    movl    28(%esp), %eax
    push    %eax

    #
    # Set up the local stack for Dividend parameter
    #
    movl    28(%esp), %eax
    push    %eax
    movl    28(%esp), %eax
    push    %eax

    #
    # Call native DivS64x64Remainder of BaseLib
    #
    jmp     ASM_PFX(DivS64x64Remainder)

    #
    # Put the Reminder in EDX:EAX as return value
    #
    movl    20(%esp), %eax
    movl    24(%esp), %edx

    #
    # Adjust stack
    #
    add     $28, %esp
    
    ret     $16
