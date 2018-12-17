/** @file
  GUID used to identify a variable cache HOB in PEI.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under
the terms and conditions of the BSD License that accompanies this distribution.
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __VARIABLE_CACHE_HOB_H__
#define __VARIABLE_CACHE_HOB_H__

#define VARIABLE_CACHE_HOB_GUID \
  { \
    0x35212b29, 0x128a, 0x4754, {0xb9, 0x96, 0x62, 0x45, 0xcc, 0xa8, 0xa0, 0x66} \
  }

extern EFI_GUID gPeiVariableCacheHobGuid;

#endif
