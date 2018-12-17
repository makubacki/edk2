/** @file
  The Variable Storage IO Completion Protocol is specific for the EDKII implementation of UEFI variables.
  This protocol is used to execute commands with a variable storage driver to complete a variable transaction.

  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL_H_
#define _VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL_H_

extern EFI_GUID  gEdkiiVariableStorageIoCompletionProtocolGuid;

///
/// Revision
///
#define EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL_REVISION  1

typedef struct _EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL;

/**
  Retrieves a protocol instance-specific GUID.

  Returns a unique GUID per EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL instance.

  @param[in]     This                     A pointer to this protocol instance.
  @param[out]    InstanceGuid             A pointer to an EFI_GUID that is this protocol instance's GUID.

  @retval        EFI_SUCCESS              The protocol instance GUID was returned successfully.
  @retval        EFI_INVALID_PARAMETER    The InstanceGuid parameter provided was NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_VARIABLE_STORAGE_IO_COMPLETION_GET_ID)(
  IN  EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   *This,
  OUT EFI_GUID                                        *InstanceGuid
  );

/**
  Completes the variable storage transaction registered for asynchronous I/O.

  @param[in]     This                     A pointer to this protocol instance.
  @param[in]     SetVariableIoCompletion  TRUE if Complete() is being called to perform I/O for SetVariable
                                          FALSE if Complete() is being called to perform I/O for GetVariable

  @retval        EFI_SUCCESS              The commands executed successfully.
  @retval        Others                   An error occurred executing commands in the command queue.

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_VARIABLE_STORAGE_IO_COMPLETION_COMPLETE)(
  IN  EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL   *This,
  IN  BOOLEAN                                         SetVariableIoCompletion
  );

///
/// Variable Storage IO Completion Protocol
///
struct _EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL {
  EDKII_VARIABLE_STORAGE_IO_COMPLETION_COMPLETE      Complete;  ///< Returns when IO request is complete.
  EDKII_VARIABLE_STORAGE_IO_COMPLETION_GET_ID        GetId;     ///< Retrieves a protocol instance-specific GUID
};

#endif