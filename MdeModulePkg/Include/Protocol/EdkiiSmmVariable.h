/** @file
  The EDKII SMM Variable Protocol is related to the EDKII-specific implementation of
  variables and intended for use as a means to store data in the EFI SMM environment.

  The EDKII SMM Variable Protocol differs from the EFI SMM Variable Protocol in that it allows
  asynchronous callbacks for read and write operations required for some variable storage media.

  If a platform does not need these asynchronous callbacks, the EFI SMM Variable Protocol is still
  compatible with the EDKII UEFI variable driver and may be used.

  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __EDKII_SMM_VARIABLE_H__
#define __EDKII_SMM_VARIABLE_H__

typedef struct _EDKII_SMM_VARIABLE_PROTOCOL  EDKII_SMM_VARIABLE_PROTOCOL;

/**
  A callback function that is invoked once the data retrieved by a GetVariable request is ready.

  @param[in]       Context       A pointer given when SmmGetVariable was called. The callback may use this to
                                 help identify which variable operation the callback is being invoked for.
  @param[in]       Status        The result of the variable read operation. Possible values are:
                                 EFI_SUCCESS            The variable read completed successfully.
                                 EFI_NOT_FOUND          The variable was not found.
                                 EFI_BUFFER_TOO_SMALL   The DataSize is too small for the result.
                                 EFI_DEVICE_ERROR       The variable could not be retrieved due to a hardware error.
                                 EFI_SECURITY_VIOLATION The variable could not be retrieved due to an authentication failure.
  @param[in]       Attributes    The attributes bitmask for the variable.
  @param[in]       DataSize      The size of data returned in Data.
  @param[in]       Data          The contents of the variable.

**/
typedef
VOID
(EFIAPI *EDKII_SMM_GET_VARIABLE_CALLBACK)(
  IN      VOID                              *Context,            OPTIONAL
  IN      EFI_STATUS                        Status,
  IN      UINT32                            Attributes,
  IN      UINTN                             DataSize,
  IN      VOID                              *Data
  );

/**
  Begins the operation to return the value of a variable. At a later point in time, when the value of the variable
  is ready the provided callback function will be invoked.

  @param[in]       Context       A caller provided pointer that will be passed to the callback
                                 function when the callback is invoked. The caller may use this to
                                 help identify which variable operation the callback is being invoked for.
  @param[in]       VariableName  A Null-terminated string that is the name of the vendor's
                                 variable.
  @param[in]       VendorGuid    A unique identifier for the vendor.
  @param[in]       DataSize      The size in bytes of the return Data buffer.
  @param[out]      Data          The buffer to return the contents of the variable.
  @param[in]       Callback      The function to invoke once the variable data is ready.

  @retval EFI_SUCCESS            The function completed successfully.
  @retval EFI_INVALID_PARAMETER  VariableName is NULL.
  @retval EFI_INVALID_PARAMETER  VendorGuid is NULL.
  @retval EFI_INVALID_PARAMETER  DataSize is NULL.
  @retval EFI_INVALID_PARAMETER  The DataSize is not too small and Data is NULL.
  @retval EFI_NOT_READY          Another driver is presently using the Variable Services

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_SMM_GET_VARIABLE)(
  IN      VOID                              *Context,       OPTIONAL
  IN      CHAR16                            *VariableName,
  IN      EFI_GUID                          *VendorGuid,
  IN      UINTN                             DataSize,
  IN      VOID                              *Data,
  IN      EDKII_SMM_GET_VARIABLE_CALLBACK   Callback
  );

/**
  A callback function that is invoked once the data provided to SetVariable has been written to non-volatile media.

  @param[in]       Context       A pointer given when SmmGetVariable was called. The callback may use this to
                                 help identify which variable operation the callback is being invoked for.
  @param[in]       Status        The result of the variable write operation. Possible values are:
                                 EFI_SUCCESS            The firmware has successfully stored the variable and its data as
                                                        defined by the Attributes.
                                 EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the variable and its data.
                                 EFI_DEVICE_ERROR       The variable could not be written due to a hardware error.
                                 EFI_WRITE_PROTECTED    The variable in question is read-only.
                                 EFI_WRITE_PROTECTED    The variable in question cannot be deleted.
                                 EFI_SECURITY_VIOLATION The variable could not be written due to EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS
                                                        or EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACESS being set, but the AuthInfo
                                                        does NOT pass the validation check carried out by the firmware.
                                 EFI_NOT_FOUND          The variable trying to be updated or deleted was not found.

**/
typedef
VOID
(EFIAPI *EDKII_SMM_SET_VARIABLE_CALLBACK)(
  IN      VOID                              *Context,            OPTIONAL
  IN      EFI_STATUS                        Status
  );

/**
  Begins the operation to write the value of a variable. At a later point in time, when the variable write operation
  is complete the provided callback function will be invoked.

  @param[in]  Context            A caller provided pointer that will be passed to the callback
                                 function when the callback is invoked. The caller may use this to
                                 help identify which variable operation the callback is being invoked for.
  @param[in]  VariableName       A Null-terminated string that is the name of the vendor's variable.
                                 Each VariableName is unique for each VendorGuid. VariableName must
                                 contain 1 or more characters. If VariableName is an empty string,
                                 then EFI_INVALID_PARAMETER is returned.
  @param[in]  VendorGuid         A unique identifier for the vendor.
  @param[in]  Attributes         Attributes bitmask to set for the variable.
  @param[in]  DataSize           The size in bytes of the Data buffer. Unless the EFI_VARIABLE_APPEND_WRITE,
                                 EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS, or
                                 EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute is set, a size of zero
                                 causes the variable to be deleted. When the EFI_VARIABLE_APPEND_WRITE attribute is
                                 set, then a SetVariable() call with a DataSize of zero will not cause any change to
                                 the variable value (the timestamp associated with the variable may be updated however
                                 even if no new data value is provided,see the description of the
                                 EFI_VARIABLE_AUTHENTICATION_2 descriptor below. In this case the DataSize will not
                                 be zero since the EFI_VARIABLE_AUTHENTICATION_2 descriptor will be populated).
  @param[in]  Data               The contents for the variable.
  @param[in]  Callback           The function to invoke once the variable is written.

  @retval EFI_SUCCESS            The write request has been successfully queued.
  @retval EFI_INVALID_PARAMETER  An invalid combination of attribute bits, name, and GUID was supplied, or the
                                 DataSize exceeds the maximum allowed.
  @retval EFI_INVALID_PARAMETER  VariableName is an empty string.
  @retval EFI_NOT_READY          Another driver is presently using the Variable Services

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_SMM_SET_VARIABLE)(
  IN      VOID                              *Context,         OPTIONAL
  IN      CHAR16                            *VariableName,
  IN      EFI_GUID                          *VendorGuid,
  IN      UINT32                            Attributes,
  IN      UINTN                             DataSize,
  IN      VOID                              *Data,
  IN      EDKII_SMM_SET_VARIABLE_CALLBACK   Callback
  );

///
/// EDKII SMM Variable Protocol is intended for use as a means
/// to store data in the EFI SMM environment.
///
struct _EDKII_SMM_VARIABLE_PROTOCOL {
  EDKII_SMM_GET_VARIABLE      SmmGetVariable;
  EFI_GET_NEXT_VARIABLE_NAME  SmmGetNextVariableName;
  EDKII_SMM_SET_VARIABLE      SmmSetVariable;
  EFI_QUERY_VARIABLE_INFO     SmmQueryVariableInfo;
};

extern EFI_GUID gEdkiiSmmVariableProtocolGuid;

#endif
