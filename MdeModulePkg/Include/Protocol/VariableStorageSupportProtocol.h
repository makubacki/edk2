/** @file
  The Variable Storage Support Protocol is specific for the EDKII implementation of UEFI variables.
  This protocol provides services needed by the EDKII UEFI variable driver to interact with variable storage drivers.

  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _VARIABLE_STORAGE_SUPPORT_PROTOCOL_H_
#define _VARIABLE_STORAGE_SUPPORT_PROTOCOL_H_

extern EFI_GUID gEdkiiVariableStorageSupportProtocolGuid;

///
/// Revision
///
#define EDKII_VARIABLE_STORAGE_PROTOCOL_REVISION  1

typedef struct _EDKII_VARIABLE_STORAGE_SUPPORT_PROTOCOL  EDKII_VARIABLE_STORAGE_SUPPORT_PROTOCOL;

/**
  Notifies the core variable driver that the Variable Storage Driver's WriteServiceIsReady() function
  is now returning TRUE instead of FALSE.

  The Variable Storage Driver is required to call this function as quickly as possible.

**/
typedef
VOID
(EFIAPI *EDKII_VARIABLE_STORAGE_SUPPORT_NOTIFY_WRITE_SERVICE_READY)(
  VOID
  );

/**
  Notifies the core variable driver that the Variable Storage Driver has completed the previously requested
  SMM phase asyncronous I/O. If the operation was a read, the data is now present in the NV storage cache.

  This function will only be used by the SMM implementation of UEFI Variables

  @param[in]  VariableStorageInstanceGuid   The instance GUID of the variable storage protocol invoking this function
  @param[in]  Status                        The result of the variable I/O operation. Possible values are:
                            EFI_SUCCESS            The variable I/O completed successfully.
                            EFI_NOT_FOUND          The variable was not found.
                            EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the variable and its data.
                            EFI_DEVICE_ERROR       The variable I/O could not complete due to a hardware error.
                            EFI_WRITE_PROTECTED    The variable in question is read-only.
                            EFI_WRITE_PROTECTED    The variable in question cannot be deleted.
                            EFI_SECURITY_VIOLATION The variable I/O could not complete due to an authentication failure.

**/
typedef
VOID
(EFIAPI *EDKII_VARIABLE_STORAGE_SUPPORT_NOTIFY_SMM_IO_COMPLETE)(
  IN      EFI_GUID                    *VariableStorageInstanceGuid,
  IN      EFI_STATUS                  Status
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
typedef
EFI_STATUS
(EFIAPI *EDKII_VARIABLE_STORAGE_SUPPORT_UPDATE_NV_CACHE) (
  IN      CHAR16                      *VariableName,
  IN      EFI_GUID                    *VendorGuid,
  IN      VOID                        *Data,
  IN      UINTN                       DataSize,
  IN      UINT32                      Attributes      OPTIONAL,
  IN      UINT32                      KeyIndex        OPTIONAL,
  IN      UINT64                      MonotonicCount  OPTIONAL,
  IN      EFI_TIME                    *TimeStamp      OPTIONAL
  );

///
/// Variable Storage Support Protocol
/// Interface functions for variable storage driver to access core variable driver functions in DXE/SMM phase.
///
struct _EDKII_VARIABLE_STORAGE_SUPPORT_PROTOCOL {
  EDKII_VARIABLE_STORAGE_SUPPORT_NOTIFY_WRITE_SERVICE_READY     NotifyWriteServiceReady;  ///< Notify that SetVariable() is ready
  EDKII_VARIABLE_STORAGE_SUPPORT_NOTIFY_SMM_IO_COMPLETE         NotifySmmIoComplete;      ///< Notify that SMM I/O is complete
  EDKII_VARIABLE_STORAGE_SUPPORT_UPDATE_NV_CACHE                UpdateNvCache;            ///< Update the NV cache with a new value for the given variable
};

#endif
