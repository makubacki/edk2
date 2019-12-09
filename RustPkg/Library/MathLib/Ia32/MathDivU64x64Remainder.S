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
#   MathDivU64x64Remainder.S
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

ASM_GLOBAL ASM_PFX(__udivmoddi4), ASM_PFX(DivU64x64Remainder)

#------------------------------------------------------------------------------
#
# void __cdecl __udivmoddi4 (void)
#
#------------------------------------------------------------------------------
ASM_PFX(__udivmoddi4):
    # Original local stack when calling __udivmoddi4
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
    # Call native DivU64x64Remainder of BaseLib
    #
    jmp     ASM_PFX(DivU64x64Remainder)

    ret     $16
