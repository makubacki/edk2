/** @file
  The Variable Storage Selector Protocol is specific for the EDKII implementation of UEFI variables.
  This protocol used by the EDKII UEFI variable driver to acquire the variable storage instance ID (GUID)
  for a particular variable name and vendor GUID. This ID is used to find the appropriate variable storage
  protocol.

  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _VARIABLE_STORAGE_SELECTOR_PROTOCOL_H_
#define _VARIABLE_STORAGE_SELECTOR_PROTOCOL_H_

extern EFI_GUID gEdkiiVariableStorageSelectorProtocolGuid;
extern EFI_GUID gEdkiiSmmVariableStorageSelectorProtocolGuid;

///
/// Revision
///
#define VARIABLE_STORAGE_PROTOCOL_REVISION  1

typedef struct _EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL  EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL;

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
(EFIAPI *EDKII_VARIABLE_STORAGE_SELECTOR_GET_ID)(
  IN  CONST  CHAR16       *VariableName,
  IN  CONST  EFI_GUID     *VendorGuid,
  OUT        EFI_GUID     *VariableStorageId
  );

///
/// Variable Storage Selector Protocol
///
struct _EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL {
  EDKII_VARIABLE_STORAGE_SELECTOR_GET_ID    GetId;      ///< Retrieves an instance-specific variable storage ID
};

#endif