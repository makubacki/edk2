/** @file
  The common variable volatile store routines shared by the DXE_RUNTIME variable
  module and the DXE_SMM variable module.

  Caution: This module requires additional review when modified.
  This driver will have external input - variable data. They may be input in SMM mode.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "VariableVolatileCommon.h"

EFI_STATUS
EFIAPI
InitVariableCache (
  IN     VARIABLE_STORE_HEADER   **VariableCacheBuffer,
  IN OUT UINTN                   *TotalVariableCacheSize
  )
{
  VARIABLE_STORE_HEADER   *VariableCacheStorePtr;

  *TotalVariableCacheSize = ALIGN_VALUE (*TotalVariableCacheSize, sizeof (UINT32));

  if (VariableCacheBuffer == NULL || *TotalVariableCacheSize < sizeof (VARIABLE_STORE_HEADER)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Allocate NV Storage Cache and initialize it to all 1's (like an erased FV)
  //
  // The NV Storage Cache allows the variable services to cache any NV variable
  // returned from the EDKII_VARIABLE_STORAGE_PROTOCOLs so that subsequent reads will
  // be returned from memory and not require additional NV access
  //
  *VariableCacheBuffer =  (VARIABLE_STORE_HEADER *) AllocateRuntimePages (
                            EFI_SIZE_TO_PAGES (*TotalVariableCacheSize)
                            );
  if (*VariableCacheBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  VariableCacheStorePtr = *VariableCacheBuffer;
  SetMem32 ((VOID *) VariableCacheStorePtr, *TotalVariableCacheSize, (UINT32) 0xFFFFFFFF);

  ZeroMem ((VOID *) VariableCacheStorePtr, sizeof (VARIABLE_STORE_HEADER));
  // Todo: Update to not assume authenticated variables
  //if (mVariableModuleGlobal->VariableGlobal.AuthFormat) {
    CopyGuid (&VariableCacheStorePtr->Signature, &gEfiAuthenticatedVariableGuid);
  //} else {
  //  CopyMem (&VariableCacheStorePtr->Signature, &gEfiVariableGuid, sizeof (EFI_GUID));
  //}
  VariableCacheStorePtr->Size    = (UINT32) *TotalVariableCacheSize;
  VariableCacheStorePtr->Format  = VARIABLE_STORE_FORMATTED;
  VariableCacheStorePtr->State   = VARIABLE_STORE_HEALTHY;

  return EFI_SUCCESS;
}
