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
#   MathLShiftS64.S
#
# Abstract:
#
#   64-bit Math Worker Function.
#   Shifts a 64-bit signed value left by a certain number of bits.
#
#------------------------------------------------------------------------------

    .686:
    .code:

ASM_GLOBAL ASM_PFX(__ashldi3)

#------------------------------------------------------------------------------
#
# void __cdecl __ashldi3 (void)
#
#------------------------------------------------------------------------------
ASM_PFX(__ashldi3):
    #
    # Handle shifting of 64 or more bits (return 0)
    #
    cmpb    $64, %cl
    jae     ReturnZero

    #
    # Handle shifting of between 0 and 31 bits
    #
    cmpb    $32, %cl
    jae     More32
    shld    %cl, %eax, %edx
    shl     %cl, %eax
    ret

    #
    # Handle shifting of between 32 and 63 bits
    #
More32:
    movl    %eax, %edx
    xor     %eax, %eax
    and     $31, %cl
    shl     %cl, %edx
    ret

ReturnZero:
    xor     %eax, %eax
    xor     %edx, %edx
    ret
