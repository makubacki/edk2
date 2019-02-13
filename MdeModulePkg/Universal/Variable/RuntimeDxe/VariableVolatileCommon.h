/** @file
  The common variable volatile store routines shared by the DXE_RUNTIME variable
  module and the DXE_SMM variable module.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _VARIABLE_VOLATILE_COMMON_H_
#define _VARIABLE_VOLATILE_COMMON_H_

#include "Variable.h"

EFI_STATUS
EFIAPI
InitVariableCache (
  IN     VARIABLE_STORE_HEADER   **VariableCacheBuffer,
  IN OUT UINTN                   *TotalVariableCacheSize
  );

#endif