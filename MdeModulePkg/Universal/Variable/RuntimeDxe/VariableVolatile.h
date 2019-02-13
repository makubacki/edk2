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

#ifndef _VARIABLE_VOLATILE_H_
#define _VARIABLE_VOLATILE_H_

#include "Variable.h"

/**
  Copies any pending updates to runtime variable caches.

  @retval EFI_UNSUPPORTED         The volatile store to be updated is not initialized properly.
  @retval EFI_SUCCESS             The volatile store was updated successfully.

**/
EFI_STATUS
SynchronizeRuntimeVariableCacheEx (
  VOID
  );

/**
  Synchronizes the runtime variable caches with all pending updates outside runtime.

  Ensures all conditions are met to maintain coherency for runtime cache updates.

  @param[in] VariableRuntimeCache Variable runtime cache structure for the runtime cache being synchronized.
  @param[in] Offset               Offset in bytes to apply the update.
  @param[in] Length               Length of data in bytes of the update.

  @retval EFI_UNSUPPORTED         The volatile store to be updated is not initialized properly.
  @retval EFI_SUCCESS             The volatile store was updated successfully.

**/
EFI_STATUS
SynchronizeRuntimeVariableCache (
  IN  VARIABLE_RUNTIME_CACHE          *VariableRuntimeCache,
  IN  UINTN                           Offset,
  IN  UINTN                           Length
  );

/**
  This function writes data to a volatile variable buffer at the given offset.

  @param Global                  Pointer to VARIABLE_GLOBAL structure.
  @param Volatile                Point out the Variable is Volatile or Non-Volatile.
  @param SetByIndex              TRUE if target pointer is given as index.
                                 FALSE if target pointer is absolute.
  @param DataPtrIndex            Pointer to the Data from the end of VARIABLE_STORE_HEADER
                                 structure.
  @param DataSize                Size of data to be written.
  @param SourceDataBuffer        Pointer to the buffer from which data is written.
  @param DestinationStoreBuffer  Pointer to a buffer that will hold the variable store.

  @retval EFI_INVALID_PARAMETER  Parameters not valid.
  @retval EFI_SUCCESS            Variable store successfully updated.

**/
EFI_STATUS
UpdateVolatileVariableStore (
  IN  VARIABLE_GLOBAL                     *Global,
  IN  BOOLEAN                             SetByIndex,
  IN  UINTN                               DataPtrIndex,
  IN  UINT32                              DataSize,
  IN  UINT8                               *SourceDataBuffer,
  OUT VARIABLE_STORE_HEADER               *DestinationStoreBuffer
  );

/**
  Update the non-volatile variable cache with a new value for the given variable

  @param[in]  VariableName       Name of variable.
  @param[in]  VendorGuid         Guid of variable.
  @param[in]  Data               Variable data.
  @param[in]  DataSize           Size of data. 0 means delete.
  @param[in]  Attributes         Attributes of the variable.
  @param[in]  KeyIndex           Index of associated public key.
  @param[in]  MonotonicCount     Value of associated monotonic count.
  @param[in]  TimeStamp          Value of associated TimeStamp.

  @retval EFI_SUCCESS           The update operation is success.
  @retval EFI_OUT_OF_RESOURCES  Variable region is full, can not write other data into this region.

**/
EFI_STATUS
EFIAPI
VariableStorageSupportUpdateNvCache (
  IN      CHAR16                      *VariableName,
  IN      EFI_GUID                    *VendorGuid,
  IN      VOID                        *Data,
  IN      UINTN                       DataSize,
  IN      UINT32                      Attributes      OPTIONAL,
  IN      UINT32                      KeyIndex        OPTIONAL,
  IN      UINT64                      MonotonicCount  OPTIONAL,
  IN      EFI_TIME                    *TimeStamp      OPTIONAL
  );

/**
  Determines if a variable exists in the default HOB

  @param  VariableName          A pointer to a null-terminated string that is the variable's name.
  @param  VariableGuid          A pointer to an EFI_GUID that is the variable's GUID. The combination of
                                VariableGuid and VariableName must be unique.

  @retval TRUE                  The variable exists in the HOBs
  @retval FALSE                 The variable does not exist in the HOBs

**/
BOOLEAN
EFIAPI
VariableExistsInHob (
  IN CONST  CHAR16                          *VariableName,
  IN CONST  EFI_GUID                        *VariableGuid
  );

/**
  This code Finds the Next available variable.

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode. This function will do basic validation, before parse the data.

  @param[in]  VariableName  Pointer to variable name.
  @param[in]  VendorGuid    Variable Vendor Guid.
  @param[out] VariablePtr   Pointer to variable header address.

  @return EFI_SUCCESS       Find the specified variable.
  @return EFI_NOT_FOUND     Not found.

**/
EFI_STATUS
EFIAPI
VariableServiceGetNextInMemoryVariableInternal (
  IN  CHAR16                *VariableName,
  IN  EFI_GUID              *VendorGuid,
  OUT VARIABLE_HEADER       **VariablePtr,
  OUT BOOLEAN               *CurrentVariableInMemory
  );

#endif