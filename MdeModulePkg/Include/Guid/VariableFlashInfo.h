/** @file
  This file defines the GUID and data structure used to pass information about
  a variable store mapped on flash (i.e. a MMIO firmware volume) to the DXE and MM environment.

  Copyright (c) Microsoft Corporation.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __VARIABLE_FLASH_INFO_H__
#define __VARIABLE_FLASH_INFO_H__

#define VARIABLE_FLASH_INFO_HOB_GUID \
  { 0x5d11c653, 0x8154, 0x4ac3, { 0xa8, 0xc2, 0xfb, 0xa2, 0x89, 0x20, 0xfc, 0x90 }}

extern EFI_GUID gVariableFlashInfoHobGuid;

///
/// Use this data structure to store variable-related info, which can decrease
/// the cost of access to NV.
///
typedef struct {
  UINT32          StorageSize;
  UINT64          StorageBase;
} VARIABLE_FLASH_INFO;

#endif // __VARIABLE_FLASH_INFO_H__
