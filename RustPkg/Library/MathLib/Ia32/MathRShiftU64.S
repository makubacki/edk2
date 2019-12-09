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
#   MathRShiftU64.S
#
# Abstract:
#
#   64-bit Math Worker Function.
#   Shifts a 64-bit unsigned value right by a certain number of bits.
#
#------------------------------------------------------------------------------


    .686:
    .code:

ASM_GLOBAL ASM_PFX(__ashrdi3)

#------------------------------------------------------------------------------
#
# void __cdecl __ashrdi3 (void)
#
#------------------------------------------------------------------------------
ASM_PFX(__ashrdi3):
    #
    # Checking: Only handle 64bit shifting or more
    #
    cmpb    $64, %cl
    jae     _Exit

    #
    # Handle shifting between 0 and 31 bits
    #
    cmpb    $32, %cl
    jae     More32
    shrd    %cl, %edx, %eax
    shr     %cl, %edx
    ret

    #
    # Handle shifting of 32-63 bits
    #
More32:
    movl    %edx, %eax
    xor     %edx, %edx
    and     $31, %cl
    shr     %cl, %eax
    ret

    #
    # Invalid number (less then 32bits), return 0
    #
_Exit:
    xor     %eax, %eax
    xor     %edx, %edx
    ret
