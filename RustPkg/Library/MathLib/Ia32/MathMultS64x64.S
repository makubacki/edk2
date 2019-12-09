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
#   MathMultS64x64.S
#
# Abstract:
#
#   64-bit Math Worker Function.
#   Multiplies a 64-bit signed or unsigned value by a 64-bit signed or unsigned value
#   and returns a 64-bit result
#
#------------------------------------------------------------------------------

    .686:
    .code:

ASM_GLOBAL ASM_PFX(_mulll), ASM_PFX(MultS64x64)

#------------------------------------------------------------------------------
#
# void __cdecl __mulll (void)
#
#------------------------------------------------------------------------------
ASM_PFX(__mulll):
    # Original local stack when calling __mulll
    #               -----------------
    #               |               |
    #               |---------------|
    #               |               |
    #               |--Multiplier --|
    #               |               |
    #               |---------------|
    #               |               |
    #               |--Multiplicand-|
    #               |               |
    #               |---------------|
    #               |  ReturnAddr** |
    #       ESP---->|---------------|
    #

    #
    # Set up the local stack for Multiplicand parameter
    #
    movl    16(%esp), %eax
    push    %eax
    movl    16(%esp), %eax
    push    %eax

    #
    # Set up the local stack for Multiplier parameter
    #
    movl    16(%esp), %eax
    push    %eax
    movl    16(%esp), %eax
    push    %eax

    #
    # Call native MulS64x64 of BaseLib
    #
    jmp     ASM_PFX(MultS64x64)

    #
    # Adjust stack
    #
    add     $16, %esp

    ret     $16
