/** @file
  Common variable non-volatile store routines.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _VARIABLE_NON_VOLATILE_H_
#define _VARIABLE_NON_VOLATILE_H_

#include "Variable.h"

/**
  Get non-volatile maximum variable size.

  @return Non-volatile maximum variable size.

**/
UINTN
GetNonVolatileMaxVariableSize (
  VOID
  );

/**
  Finds the first variable from the first NV storage

  @param[out]  PtrTrack               VARIABLE_POINTER_TRACK structure for output,
                                      including the range searched and the target position.
  @param[in]   Global                 Pointer to VARIABLE_GLOBAL structure, including
                                      base of volatile variable storage area, base of
                                      NV variable storage area, and a lock.
  @param[in]   IgnoreRtCheck          Ignore EFI_VARIABLE_RUNTIME_ACCESS attribute
                                      check at runtime when searching variable.
  @param[out]  CommandInProgress      TRUE if the command requires asyncronous I/O and has not completed yet.
                                      If this parameter is TRUE, then PtrTrack will not be updated and will
                                      not contain valid data.  Asyncronous I/O should only be required during
                                      OS runtime phase, this return value will be FALSE during all Pre-OS stages.
                                      If CommandInProgress is returned TRUE, then this function will return EFI_SUCCESS
  @param[out]  InProgressInstanceGuid If CommandInProgress is TRUE, this will contain the instance GUID of the Variable
                                      Storage driver that is performing the asyncronous I/O

  @retval EFI_INVALID_PARAMETER       If VariableName is not an empty string, while
                                      VendorGuid is NULL.
  @retval EFI_SUCCESS                 Variable successfully found.
  @retval EFI_NOT_FOUND               Variable not found

**/
EFI_STATUS
FindFirstNvVariable (
  OUT VARIABLE_POINTER_TRACK  *PtrTrack,
  IN  VARIABLE_GLOBAL         *Global,
  IN  BOOLEAN                 IgnoreRtCheck,
  OUT BOOLEAN                 *CommandInProgress,
  OUT EFI_GUID                *InProgressInstanceGuid
  );

/**
  Calculates the total variable storage size for non-volatile variable storage.

  @param[out]  TotalNonVolatileStorageSpace   The total size in bytes calculated for non-volatile storage.

  @retval EFI_SUCCESS                         The size was determined successfully.
  @retval Others                              The size could not be determined successfully.

**/
EFI_STATUS
EFIAPI
GetTotalNonVolatileVariableStorageSize (
  OUT UINTN *TotalNonVolatileStorageSize
  );

/**
  Init non-volatile variable store.

  @retval EFI_SUCCESS           Function successfully executed.
  @retval EFI_OUT_OF_RESOURCES  Fail to allocate enough memory resource.
  @retval EFI_VOLUME_CORRUPTED  Variable Store or Firmware Volume for Variable Store is corrupted.

**/
EFI_STATUS
InitNonVolatileVariableStore (
  VOID
  );

#endif