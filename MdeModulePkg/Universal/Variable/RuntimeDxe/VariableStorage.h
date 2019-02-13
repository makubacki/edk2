/** @file
  Variable storage routines used to interface with variable storage drivers.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _VARIABLE_STORAGE_H_
#define _VARIABLE_STORAGE_H_

#include "Variable.h"

/**
  Determines if any of the NV storage drivers requires asyncronous I/O or not regardless of caller source.

  @retval     TRUE                    Asyncronous I/O is required during OS runtime to call to Get/SetVariable()
  @retval     FALSE                   Asyncronous I/O is not required during OS runtime to call to Get/SetVariable()

**/
BOOLEAN
EFIAPI
VariableStorageAnyAsyncIoRequired (
  VOID
  );

/**
  Gets the EDKII_VARIABLE_STORAGE_PROTOCOL for the NV Storage that should store
  a variable with the given name and GUID

  @param[in]   VariableName             Name of the variable to be found.
  @param[in]   VendorGuid               Vendor GUID to be found.
  @param[out]  VariableStorageProtocol  The found EDKII_VARIABLE_STORAGE_PROTOCOL

  @retval EFI_INVALID_PARAMETER       If VariableName is an empty string, or
                                      VendorGuid is NULL.
  @retval EFI_SUCCESS                 EDKII_VARIABLE_STORAGE_PROTOCOL successfully found.
  @retval EFI_NOT_FOUND               EDKII_VARIABLE_STORAGE_PROTOCOL not found

**/
EFI_STATUS
GetVariableStorageProtocol (
  IN  CHAR16                      *VariableName,
  IN  EFI_GUID                    *VendorGuid,
  OUT EDKII_VARIABLE_STORAGE_PROTOCOL   **VariableStorageProtocol
  );

#endif
