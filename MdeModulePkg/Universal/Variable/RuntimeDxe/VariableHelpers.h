/** @file
  The common variable helper routines are shared by the DXE_RUNTIME variable
  module and the DXE_SMM variable module.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _VARIABLE_HELPERS_H_
#define _VARIABLE_HELPERS_H_

#include "Variable.h"

/**

  This code checks if variable header is valid or not.

  @param Variable           Pointer to the Variable Header.
  @param VariableStoreEnd   Pointer to the Variable Store End.

  @retval TRUE              Variable header is valid.
  @retval FALSE             Variable header is not valid.

**/
BOOLEAN
IsValidVariableHeader (
  IN  VARIABLE_HEADER       *Variable,
  IN  VARIABLE_HEADER       *VariableStoreEnd
  );

/**

  This code gets the current status of Variable Store.

  @param VarStoreHeader  Pointer to the Variable Store Header.

  @retval EfiRaw         Variable store status is raw.
  @retval EfiValid       Variable store status is valid.
  @retval EfiInvalid     Variable store status is invalid.

**/
VARIABLE_STORE_STATUS
GetVariableStoreStatus (
  IN VARIABLE_STORE_HEADER *VarStoreHeader
  );

/**
  This code gets the size of variable header.

  @return Size of variable header in bytes in type UINTN.

**/
UINTN
GetVariableHeaderSize (
  VOID
  );

/**

  This code gets the size of name of variable.

  @param Variable        Pointer to the Variable Header.

  @return UINTN          Size of variable in bytes.

**/
UINTN
NameSizeOfVariable (
  IN  VARIABLE_HEADER   *Variable
  );

/**
  This code sets the size of name of variable.

  @param[in] Variable   Pointer to the Variable Header.
  @param[in] NameSize   Name size to set.

**/
VOID
SetNameSizeOfVariable (
  IN VARIABLE_HEADER    *Variable,
  IN UINTN              NameSize
  );

/**

  This code gets the size of variable data.

  @param Variable        Pointer to the Variable Header.

  @return Size of variable in bytes.

**/
UINTN
DataSizeOfVariable (
  IN  VARIABLE_HEADER   *Variable
  );

/**
  This code sets the size of variable data.

  @param[in] Variable   Pointer to the Variable Header.
  @param[in] DataSize   Data size to set.

**/
VOID
SetDataSizeOfVariable (
  IN VARIABLE_HEADER    *Variable,
  IN UINTN              DataSize
  );

/**

  This code gets the pointer to the variable name.

  @param Variable        Pointer to the Variable Header.

  @return Pointer to Variable Name which is Unicode encoding.

**/
CHAR16 *
GetVariableNamePtr (
  IN  VARIABLE_HEADER   *Variable
  );

/**
  This code gets the pointer to the variable guid.

  @param Variable   Pointer to the Variable Header.

  @return A EFI_GUID* pointer to Vendor Guid.

**/
EFI_GUID *
GetVendorGuidPtr (
  IN VARIABLE_HEADER    *Variable
  );

/**

  This code gets the pointer to the variable data.

  @param Variable        Pointer to the Variable Header.

  @return Pointer to Variable Data.

**/
UINT8 *
GetVariableDataPtr (
  IN  VARIABLE_HEADER   *Variable
  );

/**
  This code gets the variable data offset related to variable header.

  @param Variable        Pointer to the Variable Header.

  @return Variable Data offset.

**/
UINTN
GetVariableDataOffset (
  IN  VARIABLE_HEADER   *Variable
  );

/**

  This code gets the pointer to the next variable header.

  @param Variable        Pointer to the Variable Header.

  @return Pointer to next variable header.

**/
VARIABLE_HEADER *
GetNextVariablePtr (
  IN  VARIABLE_HEADER   *Variable
  );

/**

  Gets the pointer to the first variable header in given variable store area.

  @param VarStoreHeader  Pointer to the Variable Store Header.

  @return Pointer to the first variable header.

**/
VARIABLE_HEADER *
GetStartPointer (
  IN VARIABLE_STORE_HEADER       *VarStoreHeader
  );

/**

  Gets the pointer to the end of the variable storage area.

  This function gets pointer to the end of the variable storage
  area, according to the input variable store header.

  @param VarStoreHeader  Pointer to the Variable Store Header.

  @return Pointer to the end of the variable storage area.

**/
VARIABLE_HEADER *
GetEndPointer (
  IN VARIABLE_STORE_HEADER       *VarStoreHeader
  );

/**
  Returns if this is a variable that always requires authenticated writes.
  There may be other scenarios that result in a variable not identified by the
  function to also require authentication.

  @param[in]  VariableName       Name of variable.
  @param[in]  VendorGuid         Guid of variable.

  @retval  TRUE            The variable always requires authenticated writes
  @retval  FALSE           The variable may or may not require authenticated writes
**/
BOOLEAN
IsAuthenticatedVariable (
  IN CHAR16                   *VariableName,
  IN EFI_GUID                 *VendorGuid
  );

/**
  Compare two EFI_TIME data.


  @param FirstTime           A pointer to the first EFI_TIME data.
  @param SecondTime          A pointer to the second EFI_TIME data.

  @retval  TRUE              The FirstTime is not later than the SecondTime.
  @retval  FALSE             The FirstTime is later than the SecondTime.

**/
BOOLEAN
VariableCompareTimeStampInternal (
  IN EFI_TIME               *FirstTime,
  IN EFI_TIME               *SecondTime
  );

/**
  Find the variable in the specified variable store.

  @param[in]       VariableName        Name of the variable to be found
  @param[in]       VendorGuid          Vendor GUID to be found.
  @param[in]       IgnoreRtCheck       Ignore EFI_VARIABLE_RUNTIME_ACCESS attribute
                                       check at runtime when searching variable.
  @param[in, out]  PtrTrack            Variable Track Pointer structure that contains Variable Information.

  @retval          EFI_SUCCESS         Variable found successfully
  @retval          EFI_NOT_FOUND       Variable not found
**/
EFI_STATUS
FindVariableEx (
  IN     CHAR16                  *VariableName,
  IN     EFI_GUID                *VendorGuid,
  IN     BOOLEAN                 IgnoreRtCheck,
  IN OUT VARIABLE_POINTER_TRACK  *PtrTrack
  );

/**
  Initializes context needed for variable helpers.

  @param[in]       AuthFormat          If true then indicates authenticated variables are supported

  @retval          EFI_SUCCESS         Initialized successfully
  @retval          Others              An error occurred during initialization
**/
EFI_STATUS
EFIAPI
InitVariableHelpers (
  IN  BOOLEAN   AuthFormat
  );

#endif