/** @file
  This EDKII-specific variable PPI is used to acquire the variable storage instance ID (GUID) for a
  particular variable name and vendor GUID. This ID is used to locate the appropriate variable storage PPI.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

This program and the accompanying materials
are licensed and made available under the terms and conditions
of the BSD License which accompanies this distribution.  The
full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#ifndef _PEI_VARIABLE_STORAGE_SELECTOR_PPI_H_
#define _PEI_VARIABLE_STORAGE_SELECTOR_PPI_H_

extern EFI_GUID gEdkiiVariableStorageSelectorPpiGuid;

///
/// Revision
///
#define PEI_VARIABLE_STORAGE_PPI_REVISION  1

typedef struct _EDKII_VARIABLE_STORAGE_SELECTOR_PPI  EDKII_VARIABLE_STORAGE_SELECTOR_PPI;

/**
  Gets the ID (GUID) for the variable storage instance that is used to store a given variable.

  @param[in]  VariableName       A pointer to a null-terminated string that is
                                 the variable's name.
  @param[in]  VariableGuid       A pointer to an EFI_GUID that is the variable's
                                 GUID. The combination of VariableGuid and
                                 VariableName must be unique.
  @param[out] VariableStorageId  The ID for the variable storage instance that
                                 stores a given variable

  @retval     EFI_SUCCESS        Variable storage instance ID that was retrieved
**/
typedef
EFI_STATUS
(EFIAPI *PEI_EDKII_VARIABLE_STORAGE_SELECTOR_GET_ID)(
  IN  CONST  CHAR16       *VariableName,
  IN  CONST  EFI_GUID     *VendorGuid,
  OUT        EFI_GUID     *VariableStorageId
  );

///
/// Variable Storage PPI
///
struct _EDKII_VARIABLE_STORAGE_SELECTOR_PPI {
  PEI_EDKII_VARIABLE_STORAGE_SELECTOR_GET_ID    GetId;      ///< Retrieves an instance-specific variable storage ID
};

#endif