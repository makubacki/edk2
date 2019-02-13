/** @file
  The sample implementation for SMM variable protocol. And this driver
  implements an SMI handler to communicate with the DXE runtime driver
  to provide variable services.

  Caution: This module requires additional review when modified.
  This driver will have external input - variable data and communicate buffer in SMM mode.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

  SmmVariableHandler() will receive untrusted input and do basic validation.

  Each sub function VariableServiceGetVariable(), VariableServiceGetNextVariableName(),
  VariableServiceSetVariable(), VariableServiceQueryVariableInfo(), ReclaimForOS(),
  SmmVariableGetStatistics() should also do validation based on its own knowledge.

Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Protocol/EdkiiSmmVariable.h>
#include <Protocol/SmmVariable.h>
#include <Protocol/SmmFirmwareVolumeBlock.h>
#include <Protocol/SmmFaultTolerantWrite.h>
#include <Protocol/SmmEndOfDxe.h>
#include <Protocol/SmmVarCheck.h>

#include <Library/SmmServicesTableLib.h>
#include <Library/SmmMemLib.h>
#include <Guid/SmmVariableCommon.h>

#include "Variable.h"
#include "VariableHelpers.h"
#include "VariableNonVolatile.h"
#include "VariableVolatile.h"

typedef struct _SMM_VARIABLE_IO_COMPLETION_STATE {
  BOOLEAN                           CommandInProgress;
  BOOLEAN                           ReenterFunction;
  BOOLEAN                           FromSmm;
  EFI_GUID                          InProgressNvStorageInstanceId;
  UINTN                             VariableFunction;
  VOID                              *Context;
  CHAR16                            VariableName[MAX_VARIABLE_NAME_SIZE];
  EFI_GUID                          VendorGuid;
  UINT32                            Attributes;
  UINTN                             DataSize;
  VOID                              *Data;
  EDKII_SMM_GET_VARIABLE_CALLBACK   GetVariableCallback;
  EDKII_SMM_SET_VARIABLE_CALLBACK   SetVariableCallback;
} SMM_VARIABLE_IO_COMPLETION_STATE;

typedef struct _SMM_VARIABLE_FLUSH_HOB_CONTEXT {
  CHAR16                                            *VariableName;
  EFI_GUID                                          *VendorGuid;
  VARIABLE_HEADER                                   *Variable;
  BOOLEAN                                           ErrorFlag;
  EFI_PHYSICAL_ADDRESS                              HobVariableBase;
  VARIABLE_HOB_FLUSH_COMPLETE_CALLBACK              HobFlushCompleteCallback;
} SMM_VARIABLE_FLUSH_HOB_CONTEXT;

extern VARIABLE_INFO_ENTRY                          *gVariableInfo;
EFI_HANDLE                                          mSmmVariableHandle            = NULL;
EFI_HANDLE                                          mVariableHandle               = NULL;
BOOLEAN                                             mAtRuntime                    = FALSE;
UINT8                                               *mVariableBufferPayload       = NULL;
UINTN                                               mVariableBufferPayloadSize;
extern BOOLEAN                                      mEndOfDxe;
extern VAR_CHECK_REQUEST_SOURCE                     mRequestSource;
extern BOOLEAN                                      mFromSmm;
extern BOOLEAN                                      mIgnoreAuthCheck;
static SMM_VARIABLE_FLUSH_HOB_CONTEXT               mSmmVariableFlushHobContext;
static SMM_VARIABLE_IO_COMPLETION_STATE             mSmmVariableIoCompletionState;

/**
  SecureBoot Hook for SetVariable.

  @param[in] VariableName                 Name of Variable to be found.
  @param[in] VendorGuid                   Variable vendor GUID.

**/
VOID
EFIAPI
SecureBootHook (
  IN CHAR16                                 *VariableName,
  IN EFI_GUID                               *VendorGuid
  )
{
  return ;
}

/**
  Begins the operation to find variable in storage blocks (Volatile or Non-Volatile).
  At a later point in time, when the value of the variable is ready the provided
  callback function will be invoked.

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode, and datasize is external input.
  This function will do basic validation, before parse the data.

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
EFI_STATUS
EFIAPI
SmmVariableGetVariable (
  IN      VOID                              *Context,       OPTIONAL
  IN      CHAR16                            *VariableName,
  IN      EFI_GUID                          *VendorGuid,
  IN      UINTN                             DataSize,
  IN      VOID                              *Data,
  IN      EDKII_SMM_GET_VARIABLE_CALLBACK   Callback
  )
{
  EFI_STATUS  Status;
  UINTN       dataSize = DataSize;
  UINT32      Attributes;

  if (mSmmVariableIoCompletionState.CommandInProgress) {
    DEBUG ((DEBUG_WARN, "UEFI Variable Services already in use, unable to perform GetVariable().\n"));
    return EFI_NOT_READY;
  }
  if (Callback == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Indicate that the caller is SMM code
  //
  ZeroMem ((VOID *) &mSmmVariableIoCompletionState, sizeof (SMM_VARIABLE_IO_COMPLETION_STATE));
  mFromSmm                                          = TRUE;
  mSmmVariableIoCompletionState.FromSmm             = TRUE;
  mSmmVariableIoCompletionState.VariableFunction    = SMM_VARIABLE_FUNCTION_GET_VARIABLE;
  mSmmVariableIoCompletionState.Context             = Context;
  mSmmVariableIoCompletionState.DataSize            = DataSize;
  mSmmVariableIoCompletionState.Data                = Data;
  mSmmVariableIoCompletionState.GetVariableCallback = Callback;
  Status    = VariableServiceGetVariable (
                VariableName,
                VendorGuid,
                &Attributes,
                &dataSize,
                Data,
                &mSmmVariableIoCompletionState.CommandInProgress,
                &mSmmVariableIoCompletionState.InProgressNvStorageInstanceId
                );
  mFromSmm  = FALSE;
  if (!mSmmVariableIoCompletionState.CommandInProgress) {
    ZeroMem ((VOID *) &mSmmVariableIoCompletionState, sizeof (SMM_VARIABLE_IO_COMPLETION_STATE));
    if (Callback != NULL) {
      Callback (Context, Status, Attributes, dataSize, Data);
    }
    return EFI_SUCCESS;
  } else {
    StrCpyS (
      &mSmmVariableIoCompletionState.VariableName[0],
      sizeof (mSmmVariableIoCompletionState.VariableName),
      VariableName
      );
    CopyMem (&mSmmVariableIoCompletionState.VendorGuid, &VendorGuid, sizeof (EFI_GUID));
  }
  return Status;
}

/**

  This code finds variable in storage blocks (Volatile or Non-Volatile).

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode, and datasize is external input.
  This function will do basic validation, before parse the data.

  @param VariableName               Name of Variable to be found.
  @param VendorGuid                 Variable vendor GUID.
  @param Attributes                 Attribute value of the variable found.
  @param DataSize                   Size of Data found. If size is less than the
                                    data, this value contains the required size.
  @param Data                       Data pointer.

  @return EFI_INVALID_PARAMETER     Invalid parameter.
  @return EFI_SUCCESS               Find the specified variable.
  @return EFI_NOT_FOUND             Not found.
  @return EFI_BUFFER_TO_SMALL       DataSize is too small for the result.

**/
EFI_STATUS
EFIAPI
SmmLegacyVariableGetVariable (
  IN      CHAR16            *VariableName,
  IN      EFI_GUID          *VendorGuid,
  OUT     UINT32            *Attributes OPTIONAL,
  IN OUT  UINTN             *DataSize,
  OUT     VOID              *Data
  )
{
  EFI_STATUS                 Status;
  BOOLEAN                    CommandInProgress;
  EFI_GUID                   InProgressNvStorageInstanceId;

  if (mSmmVariableIoCompletionState.CommandInProgress) {
    DEBUG ((DEBUG_WARN, "UEFI Variable Services already in use, unable to perform GetVariable().\n"));
    return EFI_NOT_READY;
  }
  //
  // Indicate that the caller is SMM code
  //
  mFromSmm  = TRUE;
  Status    = VariableServiceGetVariable (
                VariableName,
                VendorGuid,
                Attributes,
                DataSize,
                Data,
                &CommandInProgress,
                &InProgressNvStorageInstanceId
                );
  mFromSmm  = FALSE;
  //
  // Asyncronous reads are not supported by the old protocol
  //
  ASSERT (!CommandInProgress);
  if (CommandInProgress) {
    return EFI_DEVICE_ERROR;
  }
  return Status;
}

/**

  This code Finds the Next available variable.

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode. This function will do basic validation, before parse the data.

  @param VariableNameSize           Size of the variable name.
  @param VariableName               Pointer to variable name.
  @param VendorGuid                 Variable Vendor Guid.

  @return EFI_INVALID_PARAMETER     Invalid parameter.
  @return EFI_SUCCESS               Find the specified variable.
  @return EFI_NOT_FOUND             Not found.
  @return EFI_BUFFER_TO_SMALL       DataSize is too small for the result.

**/
EFI_STATUS
EFIAPI
SmmVariableGetNextVariableName (
  IN OUT  UINTN             *VariableNameSize,
  IN OUT  CHAR16            *VariableName,
  IN OUT  EFI_GUID          *VendorGuid
  )
{
  EFI_STATUS                 Status;

  //
  // Indicate that the caller is SMM code
  //
  mFromSmm  = TRUE;
  Status    = VariableServiceGetNextVariableName (VariableNameSize, VariableName, VendorGuid);
  mFromSmm  = FALSE;
  return Status;
}

/**
  Begins the operation to set a variable in storage blocks (Volatile or Non-Volatile).
  At a later point in time, when the variable write operation is complete the provided
  callback function will be invoked.

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
  @param[in]  IgnoreAuthCheck    Indicates whether the set variable service should ignore authenticated variable checks.
                                 This should only be TRUE if authenticated variables are flushed before AuthSupport is
                                 enabled.

  @retval EFI_SUCCESS            The write request has been successfully queued.
  @retval EFI_INVALID_PARAMETER  An invalid combination of attribute bits, name, and GUID was supplied, or the
                                 DataSize exceeds the maximum allowed.
  @retval EFI_INVALID_PARAMETER  VariableName is an empty string.
  @retval EFI_NOT_READY          Another driver is presently using the Variable Services

**/
EFI_STATUS
EFIAPI
SmmVariableSetVariableInternal (
  IN      VOID                              *Context,         OPTIONAL
  IN      CHAR16                            *VariableName,
  IN      EFI_GUID                          *VendorGuid,
  IN      UINT32                            Attributes,
  IN      UINTN                             DataSize,
  IN      VOID                              *Data,
  IN      EDKII_SMM_SET_VARIABLE_CALLBACK   Callback,
  IN      BOOLEAN                           IgnoreAuthCheck
  )
{
  EFI_STATUS    Status;

  if (mSmmVariableIoCompletionState.CommandInProgress) {
    DEBUG ((DEBUG_WARN, "UEFI Variable Services already in use, unable to perform SetVariable().\n"));
    return EFI_NOT_READY;
  }
  ZeroMem ((VOID *) &mSmmVariableIoCompletionState, sizeof (SMM_VARIABLE_IO_COMPLETION_STATE));
  mSmmVariableIoCompletionState.FromSmm             = TRUE;
  mSmmVariableIoCompletionState.VariableFunction    = SMM_VARIABLE_FUNCTION_SET_VARIABLE;
  mSmmVariableIoCompletionState.Context             = Context;
  mSmmVariableIoCompletionState.SetVariableCallback = Callback;
  //
  // Disable write protection when the calling SetVariable() through EFI_SMM_VARIABLE_PROTOCOL.
  //
  mFromSmm          = TRUE;
  mIgnoreAuthCheck  = IgnoreAuthCheck;
  mRequestSource    = VarCheckFromTrusted;
  Status            = VariableServiceSetVariable (
                        VariableName,
                        VendorGuid,
                        Attributes,
                        DataSize,
                        Data,
                        &mSmmVariableIoCompletionState.CommandInProgress,
                        &mSmmVariableIoCompletionState.InProgressNvStorageInstanceId,
                        &mSmmVariableIoCompletionState.ReenterFunction
                        );
  mFromSmm          = FALSE;
  mIgnoreAuthCheck  = FALSE;
  mRequestSource    = VarCheckFromUntrusted;
  if (!mSmmVariableIoCompletionState.CommandInProgress) {
    ZeroMem ((VOID *) &mSmmVariableIoCompletionState, sizeof (SMM_VARIABLE_IO_COMPLETION_STATE));
    if (Callback != NULL) {
      Callback (Context, Status);
      return EFI_SUCCESS;
    }
  } else {
    StrCpyS (
      &mSmmVariableIoCompletionState.VariableName[0],
      sizeof (mSmmVariableIoCompletionState.VariableName),
      VariableName
      );
    CopyMem (&mSmmVariableIoCompletionState.VendorGuid, &VendorGuid, sizeof (EFI_GUID));
    mSmmVariableIoCompletionState.Attributes  = Attributes;
    mSmmVariableIoCompletionState.DataSize    = DataSize;
    mSmmVariableIoCompletionState.Data        = Data;
  }
  return Status;
}

/**
  Begins the operation to set a variable in storage blocks (Volatile or Non-Volatile).
  At a later point in time, when the variable write operation is complete the provided
  callback function will be invoked.

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
EFI_STATUS
EFIAPI
SmmVariableSetVariable (
  IN      VOID                              *Context,         OPTIONAL
  IN      CHAR16                            *VariableName,
  IN      EFI_GUID                          *VendorGuid,
  IN      UINT32                            Attributes,
  IN      UINTN                             DataSize,
  IN      VOID                              *Data,
  IN      EDKII_SMM_SET_VARIABLE_CALLBACK   Callback
  )
{
  if (Callback == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  return SmmVariableSetVariableInternal (Context, VariableName, VendorGuid, Attributes, DataSize, Data, Callback, FALSE);
}

/**

  This code sets variable in storage blocks (Volatile or Non-Volatile).

  @param VariableName                     Name of Variable to be found.
  @param VendorGuid                       Variable vendor GUID.
  @param Attributes                       Attribute value of the variable found
  @param DataSize                         Size of Data found. If size is less than the
                                          data, this value contains the required size.
  @param Data                             Data pointer.

  @return EFI_INVALID_PARAMETER           Invalid parameter.
  @return EFI_SUCCESS                     Set successfully.
  @return EFI_OUT_OF_RESOURCES            Resource not enough to set variable.
  @return EFI_NOT_FOUND                   Not found.
  @return EFI_WRITE_PROTECTED             Variable is read-only.

**/
EFI_STATUS
EFIAPI
SmmLegacyVariableSetVariable (
  IN CHAR16                  *VariableName,
  IN EFI_GUID                *VendorGuid,
  IN UINT32                  Attributes,
  IN UINTN                   DataSize,
  IN VOID                    *Data
  )
{
  return SmmVariableSetVariableInternal (NULL, VariableName, VendorGuid, Attributes, DataSize, Data, NULL, FALSE);
}

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
VOID
EFIAPI
VariableStorageSupportNotifySmmIoComplete (
  IN      EFI_GUID                    *VariableStorageInstanceGuid,
  IN      EFI_STATUS                  Status
  )
{
  EFI_GUID    InProgressNvStorageInstanceId;
  EFI_STATUS  Status2;
  UINTN       DataSize;
  UINT32      Attributes;
  BOOLEAN     CommandInProgress;

  if (!mSmmVariableIoCompletionState.CommandInProgress) {
    return;
  }
  if (!CompareGuid (&mSmmVariableIoCompletionState.InProgressNvStorageInstanceId, VariableStorageInstanceGuid)) {
    return;
  }
  switch (mSmmVariableIoCompletionState.VariableFunction) {
    case SMM_VARIABLE_FUNCTION_GET_VARIABLE:
      if (EFI_ERROR (Status)) {
        if (mSmmVariableIoCompletionState.GetVariableCallback != NULL) {
          mSmmVariableIoCompletionState.GetVariableCallback (
            mSmmVariableIoCompletionState.Context,
            Status,
            0,
            0,
            mSmmVariableIoCompletionState.Data
            );
        }
        goto Done;
      }
      //
      // The data should be in the NV cache now, we should be able to retrieve it
      //
      DataSize  = mSmmVariableIoCompletionState.DataSize;
      mFromSmm  = TRUE;
      Status2   = VariableServiceGetVariable (
                    &mSmmVariableIoCompletionState.VariableName[0],
                    &mSmmVariableIoCompletionState.VendorGuid,
                    &Attributes,
                    &DataSize,
                    mSmmVariableIoCompletionState.Data,
                    &CommandInProgress,
                    &InProgressNvStorageInstanceId
                    );
      mFromSmm  = FALSE;
      ASSERT (!CommandInProgress);
      if (CommandInProgress) {
        goto Done;
      }
      if (EFI_ERROR (Status2)) {
        if (mSmmVariableIoCompletionState.GetVariableCallback != NULL) {
          mSmmVariableIoCompletionState.GetVariableCallback (
            mSmmVariableIoCompletionState.Context,
            Status2,
            0,
            0,
            mSmmVariableIoCompletionState.Data
            );
        }
        goto Done;
      }
      if (mSmmVariableIoCompletionState.GetVariableCallback != NULL) {
        mSmmVariableIoCompletionState.GetVariableCallback (
          mSmmVariableIoCompletionState.Context,
          Status,
          Attributes,
          DataSize,
          mSmmVariableIoCompletionState.Data
          );
      }
      break;
    case SMM_VARIABLE_FUNCTION_SET_VARIABLE:
      if (EFI_ERROR (Status)) {
        if (mSmmVariableIoCompletionState.SetVariableCallback) {
          mSmmVariableIoCompletionState.CommandInProgress = FALSE;
          mSmmVariableIoCompletionState.SetVariableCallback (
            mSmmVariableIoCompletionState.Context,
            Status
            );
        }
        goto Done;
      }
      if (mSmmVariableIoCompletionState.ReenterFunction) {
        mSmmVariableIoCompletionState.CommandInProgress = FALSE;
        mSmmVariableIoCompletionState.ReenterFunction   = FALSE;
        mSmmVariableIoCompletionState.FromSmm           = TRUE;
        ZeroMem (&mSmmVariableIoCompletionState.InProgressNvStorageInstanceId, sizeof (EFI_GUID));
        mFromSmm        = TRUE;
        mRequestSource  = VarCheckFromTrusted;
        Status2         = VariableServiceSetVariable (
                            &mSmmVariableIoCompletionState.VariableName[0],
                            &mSmmVariableIoCompletionState.VendorGuid,
                            mSmmVariableIoCompletionState.Attributes,
                            mSmmVariableIoCompletionState.DataSize,
                            mSmmVariableIoCompletionState.Data,
                            &mSmmVariableIoCompletionState.CommandInProgress,
                            &mSmmVariableIoCompletionState.InProgressNvStorageInstanceId,
                            &mSmmVariableIoCompletionState.ReenterFunction
                            );
        mRequestSource  = VarCheckFromUntrusted;
        mFromSmm        = FALSE;
        if (!mSmmVariableIoCompletionState.CommandInProgress || EFI_ERROR (Status2)) {
          if (mSmmVariableIoCompletionState.SetVariableCallback) {
            mSmmVariableIoCompletionState.CommandInProgress = FALSE;
            mSmmVariableIoCompletionState.SetVariableCallback (
              mSmmVariableIoCompletionState.Context,
              Status2
              );
          }
          goto Done;
        }
        //
        // Don't clear mSmmVariableIoCompletionState in this case since we will reenter this function later
        //
        return;
      } else {
        if (mSmmVariableIoCompletionState.SetVariableCallback) {
          mSmmVariableIoCompletionState.CommandInProgress = FALSE;
          mSmmVariableIoCompletionState.SetVariableCallback (
            mSmmVariableIoCompletionState.Context,
            Status
            );
        }
      }
      break;
    default:
      break;
  }
Done:
  if (!mSmmVariableIoCompletionState.CommandInProgress) {
    ZeroMem ((VOID *) &mSmmVariableIoCompletionState, sizeof (SMM_VARIABLE_IO_COMPLETION_STATE));
  }
}

/**

  This code returns information about the EFI variables.

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode. This function will do basic validation, before parse the data.

  @param Attributes                     Attributes bitmask to specify the type of variables
                                        on which to return information.
  @param MaximumVariableStorageSize     Pointer to the maximum size of the storage space available
                                        for the EFI variables associated with the attributes specified.
  @param RemainingVariableStorageSize   Pointer to the remaining size of the storage space available
                                        for EFI variables associated with the attributes specified.
  @param MaximumVariableSize            Pointer to the maximum size of an individual EFI variables
                                        associated with the attributes specified.

  @return EFI_SUCCESS                   Query successfully.

**/
EFI_STATUS
EFIAPI
SmmVariableQueryVariableInfo (
  IN  UINT32                 Attributes,
  OUT UINT64                 *MaximumVariableStorageSize,
  OUT UINT64                 *RemainingVariableStorageSize,
  OUT UINT64                 *MaximumVariableSize
  )
{
  EFI_STATUS                 Status;

  //
  // Indicate that the caller is SMM code
  //
  mFromSmm  = TRUE;
  Status    = VariableServiceQueryVariableInfoInternal (
                Attributes,
                MaximumVariableStorageSize,
                RemainingVariableStorageSize,
                MaximumVariableSize
                );
  mFromSmm  = FALSE;
  return Status;
}

EDKII_SMM_VARIABLE_PROTOCOL             gSmmVariable = {
  SmmVariableGetVariable,
  SmmVariableGetNextVariableName,
  SmmVariableSetVariable,
  SmmVariableQueryVariableInfo
};

///
/// This is the legacy SMM Variable Protocol, which is only provided for backward compatibility
/// new code should use EDKII_SMM_VARIABLE_PROTOCOL whenever possible.
///
EFI_SMM_VARIABLE_PROTOCOL               gSmmLegacyVariable = {
  SmmLegacyVariableGetVariable,
  SmmVariableGetNextVariableName,
  SmmLegacyVariableSetVariable,
  SmmVariableQueryVariableInfo
};

EDKII_SMM_VAR_CHECK_PROTOCOL            mSmmVarCheck = { VarCheckRegisterSetVariableCheckHandler,
                                                         VarCheckVariablePropertySet,
                                                         VarCheckVariablePropertyGet };

EDKII_VARIABLE_STORAGE_SUPPORT_PROTOCOL mVariableStorageSupport = {
  VariableStorageSupportNotifyWriteServiceReady,
  VariableStorageSupportNotifySmmIoComplete,
  VariableStorageSupportUpdateNvCache
};

/**
  Return TRUE if ExitBootServices () has been called.

  @retval TRUE If ExitBootServices () has been called.
**/
BOOLEAN
AtRuntime (
  VOID
  )
{
  return mAtRuntime;
}

/**
  Initializes a basic mutual exclusion lock.

  This function initializes a basic mutual exclusion lock to the released state
  and returns the lock.  Each lock provides mutual exclusion access at its task
  priority level.  Since there is no preemption or multiprocessor support in EFI,
  acquiring the lock only consists of raising to the locks TPL.
  If Lock is NULL, then ASSERT().
  If Priority is not a valid TPL value, then ASSERT().

  @param  Lock       A pointer to the lock data structure to initialize.
  @param  Priority   EFI TPL is associated with the lock.

  @return The lock.

**/
EFI_LOCK *
InitializeLock (
  IN OUT EFI_LOCK                         *Lock,
  IN EFI_TPL                              Priority
  )
{
  return Lock;
}

/**
  Acquires lock only at boot time. Simply returns at runtime.

  This is a temperary function that will be removed when
  EfiAcquireLock() in UefiLib can handle the call in UEFI
  Runtimer driver in RT phase.
  It calls EfiAcquireLock() at boot time, and simply returns
  at runtime.

  @param  Lock         A pointer to the lock to acquire.

**/
VOID
AcquireLockOnlyAtBootTime (
  IN EFI_LOCK                             *Lock
  )
{

}


/**
  Releases lock only at boot time. Simply returns at runtime.

  This is a temperary function which will be removed when
  EfiReleaseLock() in UefiLib can handle the call in UEFI
  Runtimer driver in RT phase.
  It calls EfiReleaseLock() at boot time and simply returns
  at runtime.

  @param  Lock         A pointer to the lock to release.

**/
VOID
ReleaseLockOnlyAtBootTime (
  IN EFI_LOCK                             *Lock
  )
{

}

/**
  Get the variable statistics information from the information buffer pointed by gVariableInfo.

  Caution: This function may be invoked at SMM runtime.
  InfoEntry and InfoSize are external input. Care must be taken to make sure not security issue at runtime.

  @param[in, out]  InfoEntry    A pointer to the buffer of variable information entry.
                                On input, point to the variable information returned last time. if
                                InfoEntry->VendorGuid is zero, return the first information.
                                On output, point to the next variable information.
  @param[in, out]  InfoSize     On input, the size of the variable information buffer.
                                On output, the returned variable information size.

  @retval EFI_SUCCESS           The variable information is found and returned successfully.
  @retval EFI_UNSUPPORTED       No variable inoformation exists in variable driver. The
                                PcdVariableCollectStatistics should be set TRUE to support it.
  @retval EFI_BUFFER_TOO_SMALL  The buffer is too small to hold the next variable information.
  @retval EFI_INVALID_PARAMETER Input parameter is invalid.

**/
EFI_STATUS
SmmVariableGetStatistics (
  IN OUT VARIABLE_INFO_ENTRY                           *InfoEntry,
  IN OUT UINTN                                         *InfoSize
  )
{
  VARIABLE_INFO_ENTRY                                  *VariableInfo;
  UINTN                                                NameSize;
  UINTN                                                StatisticsInfoSize;
  CHAR16                                               *InfoName;
  UINTN                                                InfoNameMaxSize;
  EFI_GUID                                             VendorGuid;

  if (InfoEntry == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  VariableInfo = gVariableInfo;
  if (VariableInfo == NULL) {
    return EFI_UNSUPPORTED;
  }

  StatisticsInfoSize = sizeof (VARIABLE_INFO_ENTRY);
  if (*InfoSize < StatisticsInfoSize) {
    *InfoSize = StatisticsInfoSize;
    return EFI_BUFFER_TOO_SMALL;
  }
  InfoName = (CHAR16 *)(InfoEntry + 1);
  InfoNameMaxSize = (*InfoSize - sizeof (VARIABLE_INFO_ENTRY));

  CopyGuid (&VendorGuid, &InfoEntry->VendorGuid);

  if (IsZeroGuid (&VendorGuid)) {
    //
    // Return the first variable info
    //
    NameSize = StrSize (VariableInfo->Name);
    StatisticsInfoSize = sizeof (VARIABLE_INFO_ENTRY) + NameSize;
    if (*InfoSize < StatisticsInfoSize) {
      *InfoSize = StatisticsInfoSize;
      return EFI_BUFFER_TOO_SMALL;
    }
    CopyMem (InfoEntry, VariableInfo, sizeof (VARIABLE_INFO_ENTRY));
    CopyMem (InfoName, VariableInfo->Name, NameSize);
    *InfoSize = StatisticsInfoSize;
    return EFI_SUCCESS;
  }

  //
  // Get the next variable info
  //
  while (VariableInfo != NULL) {
    if (CompareGuid (&VariableInfo->VendorGuid, &VendorGuid)) {
      NameSize = StrSize (VariableInfo->Name);
      if (NameSize <= InfoNameMaxSize) {
        if (CompareMem (VariableInfo->Name, InfoName, NameSize) == 0) {
          //
          // Find the match one
          //
          VariableInfo = VariableInfo->Next;
          break;
        }
      }
    }
    VariableInfo = VariableInfo->Next;
  };

  if (VariableInfo == NULL) {
    *InfoSize = 0;
    return EFI_SUCCESS;
  }

  //
  // Output the new variable info
  //
  NameSize = StrSize (VariableInfo->Name);
  StatisticsInfoSize = sizeof (VARIABLE_INFO_ENTRY) + NameSize;
  if (*InfoSize < StatisticsInfoSize) {
    *InfoSize = StatisticsInfoSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (InfoEntry, VariableInfo, sizeof (VARIABLE_INFO_ENTRY));
  CopyMem (InfoName, VariableInfo->Name, NameSize);
  *InfoSize = StatisticsInfoSize;

  return EFI_SUCCESS;
}


/**
  Communication service SMI Handler entry.

  This SMI handler provides services for the variable wrapper driver.

  Caution: This function may receive untrusted input.
  This variable data and communicate buffer are external input, so this function will do basic validation.
  Each sub function VariableServiceGetVariable(), VariableServiceGetNextVariableName(),
  VariableServiceSetVariable(), VariableServiceQueryVariableInfo(), ReclaimForOS(),
  SmmVariableGetStatistics() should also do validation based on its own knowledge.

  @param[in]     DispatchHandle  The unique handle assigned to this handler by SmiHandlerRegister().
  @param[in]     RegisterContext Points to an optional handler context which was specified when the
                                 handler was registered.
  @param[in, out] CommBuffer     A pointer to a collection of data in memory that will
                                 be conveyed from a non-SMM environment into an SMM environment.
  @param[in, out] CommBufferSize The size of the CommBuffer.

  @retval EFI_SUCCESS                         The interrupt was handled and quiesced. No other handlers
                                              should still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_QUIESCED  The interrupt has been quiesced but other handlers should
                                              still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_PENDING   The interrupt is still pending and other handlers should still
                                              be called.
  @retval EFI_INTERRUPT_PENDING               The interrupt could not be quiesced.
**/
EFI_STATUS
EFIAPI
SmmVariableHandler (
  IN     EFI_HANDLE                                       DispatchHandle,
  IN     CONST VOID                                       *RegisterContext,
  IN OUT VOID                                             *CommBuffer,
  IN OUT UINTN                                            *CommBufferSize
  )
{
  EFI_STATUS                                              Status;
  SMM_VARIABLE_COMMUNICATE_HEADER2                        *SmmVariableFunctionHeader;
  SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE                *SmmVariableHeader;
  SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME         *GetNextVariableName;
  SMM_VARIABLE_COMMUNICATE_QUERY_VARIABLE_INFO            *QueryVariableInfo;
  SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE               *GetPayloadSize;
  SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT *RuntimeVariableCacheContext;
  SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE           *GetTotalStoreSize;
  VARIABLE_INFO_ENTRY                                     *VariableInfo;
  SMM_VARIABLE_COMMUNICATE_LOCK_VARIABLE                  *VariableToLock;
  SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY    *CommVariableProperty;
  UINTN                                                   InfoSize;
  UINTN                                                   NameBufferSize;
  UINTN                                                   CommBufferPayloadSize;
  UINTN                                                   TempCommBufferSize;

  //
  // If input is invalid, stop processing this SMI
  //
  if (CommBuffer == NULL || CommBufferSize == NULL) {
    return EFI_SUCCESS;
  }

  TempCommBufferSize = *CommBufferSize;

  if (TempCommBufferSize < SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE) {
    DEBUG ((EFI_D_ERROR, "SmmVariableHandler: SMM communication buffer size invalid!\n"));
    return EFI_SUCCESS;
  }
  CommBufferPayloadSize = TempCommBufferSize - SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE;
  if (CommBufferPayloadSize > mVariableBufferPayloadSize) {
    DEBUG ((EFI_D_ERROR, "SmmVariableHandler: SMM communication buffer payload size invalid!\n"));
    return EFI_SUCCESS;
  }

  if (!SmmIsBufferOutsideSmmValid ((UINTN)CommBuffer, TempCommBufferSize)) {
    DEBUG ((EFI_D_ERROR, "SmmVariableHandler: SMM communication buffer in SMRAM or overflow!\n"));
    return EFI_SUCCESS;
  }

  SmmVariableFunctionHeader = (SMM_VARIABLE_COMMUNICATE_HEADER2 *)CommBuffer;
  switch (SmmVariableFunctionHeader->Function) {
    case SMM_VARIABLE_FUNCTION_GET_VARIABLE:
      if (CommBufferPayloadSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) {
        DEBUG ((EFI_D_ERROR, "GetVariable: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      if (mSmmVariableIoCompletionState.CommandInProgress && mSmmVariableIoCompletionState.FromSmm) {
        //
        // The variable services are already in use by SMM, inform the Runtime DXE driver to wait for SMM to finish
        //
        DEBUG ((DEBUG_INFO, "UEFI Variable Services already in use, waiting in Runtime DXE for completion.\n"));
        SmmVariableFunctionHeader->VariableServicesInUse = TRUE;
        return EFI_SUCCESS;
      }
      //
      // Copy the input communicate buffer payload to pre-allocated SMM variable buffer payload.
      //
      CopyMem (mVariableBufferPayload, SmmVariableFunctionHeader->Data, CommBufferPayloadSize);
      SmmVariableHeader = (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *) mVariableBufferPayload;
      if (((UINTN)(~0) - SmmVariableHeader->DataSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) ||
         ((UINTN)(~0) - SmmVariableHeader->NameSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) + SmmVariableHeader->DataSize)) {
        //
        // Prevent InfoSize overflow happen
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }
      InfoSize = OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)
                 + SmmVariableHeader->DataSize + SmmVariableHeader->NameSize;

      //
      // SMRAM range check already covered before
      //
      if (InfoSize > CommBufferPayloadSize) {
        DEBUG ((EFI_D_ERROR, "GetVariable: Data size exceed communication buffer size limit!\n"));
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      //
      // The MemoryLoadFence() call here is to ensure the previous range/content
      // checks for the CommBuffer have been completed before the subsequent
      // consumption of the CommBuffer content.
      //
      MemoryLoadFence ();
      if (SmmVariableHeader->NameSize < sizeof (CHAR16) || SmmVariableHeader->Name[SmmVariableHeader->NameSize/sizeof (CHAR16) - 1] != L'\0') {
        //
        // Make sure VariableName is A Null-terminated string.
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      SmmVariableFunctionHeader->CommandInProgress = FALSE;
      Status = VariableServiceGetVariable (
                 SmmVariableHeader->Name,
                 &SmmVariableHeader->Guid,
                 &SmmVariableHeader->Attributes,
                 &SmmVariableHeader->DataSize,
                 (UINT8 *)SmmVariableHeader->Name + SmmVariableHeader->NameSize,
                 &SmmVariableFunctionHeader->CommandInProgress,
                 &SmmVariableFunctionHeader->InProgressNvStorageInstanceId
                 );
      CopyMem (SmmVariableFunctionHeader->Data, mVariableBufferPayload, CommBufferPayloadSize);
      if (SmmVariableFunctionHeader->CommandInProgress) {
        SmmVariableFunctionHeader->ReenterFunction      = TRUE;
        mSmmVariableIoCompletionState.CommandInProgress = TRUE;
        mSmmVariableIoCompletionState.FromSmm           = FALSE;
      } else {
        mSmmVariableIoCompletionState.CommandInProgress = FALSE;
        mSmmVariableIoCompletionState.FromSmm           = FALSE;
      }
      break;

    case SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME:
      if (CommBufferPayloadSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name)) {
        DEBUG ((EFI_D_ERROR, "GetNextVariableName: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      //
      // Copy the input communicate buffer payload to pre-allocated SMM variable buffer payload.
      //
      CopyMem (mVariableBufferPayload, SmmVariableFunctionHeader->Data, CommBufferPayloadSize);
      GetNextVariableName = (SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME *) mVariableBufferPayload;
      if ((UINTN)(~0) - GetNextVariableName->NameSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name)) {
        //
        // Prevent InfoSize overflow happen
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }
      InfoSize = OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name) + GetNextVariableName->NameSize;

      //
      // SMRAM range check already covered before
      //
      if (InfoSize > CommBufferPayloadSize) {
        DEBUG ((EFI_D_ERROR, "GetNextVariableName: Data size exceed communication buffer size limit!\n"));
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      NameBufferSize = CommBufferPayloadSize - OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME, Name);
      if (NameBufferSize < sizeof (CHAR16) || GetNextVariableName->Name[NameBufferSize/sizeof (CHAR16) - 1] != L'\0') {
        //
        // Make sure input VariableName is A Null-terminated string.
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      Status = VariableServiceGetNextVariableName (
                 &GetNextVariableName->NameSize,
                 GetNextVariableName->Name,
                 &GetNextVariableName->Guid
                 );
      CopyMem (SmmVariableFunctionHeader->Data, mVariableBufferPayload, CommBufferPayloadSize);
      break;

    case SMM_VARIABLE_FUNCTION_SET_VARIABLE:
      if (CommBufferPayloadSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) {
        DEBUG ((EFI_D_ERROR, "SetVariable: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      if (mSmmVariableIoCompletionState.CommandInProgress && mSmmVariableIoCompletionState.FromSmm) {
        //
        // The variable services are already in use by SMM, inform the Runtime DXE driver to wait for SMM to finish
        //
        DEBUG ((DEBUG_INFO, "UEFI Variable Services already in use, waiting in Runtime DXE for completion.\n"));
        SmmVariableFunctionHeader->VariableServicesInUse = TRUE;
        return EFI_SUCCESS;
      }
      //
      // Copy the input communicate buffer payload to pre-allocated SMM variable buffer payload.
      //
      CopyMem (mVariableBufferPayload, SmmVariableFunctionHeader->Data, CommBufferPayloadSize);
      SmmVariableHeader = (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *) mVariableBufferPayload;
      if (((UINTN)(~0) - SmmVariableHeader->DataSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) ||
         ((UINTN)(~0) - SmmVariableHeader->NameSize < OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) + SmmVariableHeader->DataSize)) {
        //
        // Prevent InfoSize overflow happen
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }
      InfoSize = OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)
                 + SmmVariableHeader->DataSize + SmmVariableHeader->NameSize;

      //
      // SMRAM range check already covered before
      // Data buffer should not contain SMM range
      //
      if (InfoSize > CommBufferPayloadSize) {
        DEBUG ((EFI_D_ERROR, "SetVariable: Data size exceed communication buffer size limit!\n"));
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      //
      // The MemoryLoadFence() call here is to ensure the previous range/content
      // checks for the CommBuffer have been completed before the subsequent
      // consumption of the CommBuffer content.
      //
      MemoryLoadFence ();
      if (SmmVariableHeader->NameSize < sizeof (CHAR16) || SmmVariableHeader->Name[SmmVariableHeader->NameSize/sizeof (CHAR16) - 1] != L'\0') {
        //
        // Make sure VariableName is A Null-terminated string.
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      Status = VariableServiceSetVariable (
                 SmmVariableHeader->Name,
                 &SmmVariableHeader->Guid,
                 SmmVariableHeader->Attributes,
                 SmmVariableHeader->DataSize,
                 (UINT8 *)SmmVariableHeader->Name + SmmVariableHeader->NameSize,
                 &SmmVariableFunctionHeader->CommandInProgress,
                 &SmmVariableFunctionHeader->InProgressNvStorageInstanceId,
                 &SmmVariableFunctionHeader->ReenterFunction
                 );
      if (SmmVariableFunctionHeader->CommandInProgress) {
        mSmmVariableIoCompletionState.CommandInProgress = TRUE;
        mSmmVariableIoCompletionState.FromSmm           = FALSE;
      } else {
        mSmmVariableIoCompletionState.CommandInProgress = FALSE;
        mSmmVariableIoCompletionState.FromSmm           = FALSE;
      }
      break;

    case SMM_VARIABLE_FUNCTION_QUERY_VARIABLE_INFO:
      if (CommBufferPayloadSize < sizeof (SMM_VARIABLE_COMMUNICATE_QUERY_VARIABLE_INFO)) {
        DEBUG ((EFI_D_ERROR, "QueryVariableInfo: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      QueryVariableInfo = (SMM_VARIABLE_COMMUNICATE_QUERY_VARIABLE_INFO *) SmmVariableFunctionHeader->Data;

      Status = VariableServiceQueryVariableInfo (
                 QueryVariableInfo->Attributes,
                 &QueryVariableInfo->MaximumVariableStorageSize,
                 &QueryVariableInfo->RemainingVariableStorageSize,
                 &QueryVariableInfo->MaximumVariableSize
                 );
      break;

    case SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE:
      if (CommBufferPayloadSize < sizeof (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE)) {
        DEBUG ((EFI_D_ERROR, "GetPayloadSize: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      GetPayloadSize = (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *) SmmVariableFunctionHeader->Data;
      GetPayloadSize->VariablePayloadSize = mVariableBufferPayloadSize;
      Status = EFI_SUCCESS;
      break;

    case SMM_VARIABLE_FUNCTION_READY_TO_BOOT:
      if (AtRuntime()) {
        Status = EFI_UNSUPPORTED;
        break;
      }
      if (!mEndOfDxe) {
        MorLockInitAtEndOfDxe ();
        mEndOfDxe = TRUE;
        VarCheckLibInitializeAtEndOfDxe (NULL);
        //
        // The initialization for variable quota.
        //
        InitializeVariableQuota ();
      }
      ReclaimForOS ();
      Status = EFI_SUCCESS;
      break;

    case SMM_VARIABLE_FUNCTION_EXIT_BOOT_SERVICE:
      mAtRuntime = TRUE;
      Status = EFI_SUCCESS;
      break;

    case SMM_VARIABLE_FUNCTION_GET_STATISTICS:
      VariableInfo = (VARIABLE_INFO_ENTRY *) SmmVariableFunctionHeader->Data;
      InfoSize = TempCommBufferSize - SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE;

      //
      // Do not need to check SmmVariableFunctionHeader->Data in SMRAM here.
      // It is covered by previous CommBuffer check
      //

      if (!SmmIsBufferOutsideSmmValid ((EFI_PHYSICAL_ADDRESS)(UINTN)CommBufferSize, sizeof(UINTN))) {
        DEBUG ((EFI_D_ERROR, "GetStatistics: SMM communication buffer in SMRAM!\n"));
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      Status = SmmVariableGetStatistics (VariableInfo, &InfoSize);
      *CommBufferSize = InfoSize + SMM_VARIABLE_COMMUNICATE_HEADER2_SIZE;
      break;

    case SMM_VARIABLE_FUNCTION_LOCK_VARIABLE:
      if (mEndOfDxe) {
        Status = EFI_ACCESS_DENIED;
      } else {
        VariableToLock = (SMM_VARIABLE_COMMUNICATE_LOCK_VARIABLE *) SmmVariableFunctionHeader->Data;
        Status = VariableLockRequestToLock (
                   NULL,
                   VariableToLock->Name,
                   &VariableToLock->Guid
                   );
      }
      break;
    case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_SET:
      if (mEndOfDxe) {
        Status = EFI_ACCESS_DENIED;
      } else {
        CommVariableProperty = (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY *) SmmVariableFunctionHeader->Data;
        Status = VarCheckVariablePropertySet (
                   CommVariableProperty->Name,
                   &CommVariableProperty->Guid,
                   &CommVariableProperty->VariableProperty
                   );
      }
      break;
    case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_GET:
      if (CommBufferPayloadSize < OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name)) {
        DEBUG ((EFI_D_ERROR, "VarCheckVariablePropertyGet: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      //
      // Copy the input communicate buffer payload to pre-allocated SMM variable buffer payload.
      //
      CopyMem (mVariableBufferPayload, SmmVariableFunctionHeader->Data, CommBufferPayloadSize);
      CommVariableProperty = (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY *) mVariableBufferPayload;
      if ((UINTN) (~0) - CommVariableProperty->NameSize < OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name)) {
        //
        // Prevent InfoSize overflow happen
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }
      InfoSize = OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name) + CommVariableProperty->NameSize;

      //
      // SMRAM range check already covered before
      //
      if (InfoSize > CommBufferPayloadSize) {
        DEBUG ((EFI_D_ERROR, "VarCheckVariablePropertyGet: Data size exceed communication buffer size limit!\n"));
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      //
      // The MemoryLoadFence() call here is to ensure the previous range/content
      // checks for the CommBuffer have been completed before the subsequent
      // consumption of the CommBuffer content.
      //
      MemoryLoadFence ();
      if (CommVariableProperty->NameSize < sizeof (CHAR16) || CommVariableProperty->Name[CommVariableProperty->NameSize/sizeof (CHAR16) - 1] != L'\0') {
        //
        // Make sure VariableName is A Null-terminated string.
        //
        Status = EFI_ACCESS_DENIED;
        goto EXIT;
      }

      Status = VarCheckVariablePropertyGet (
                 CommVariableProperty->Name,
                 &CommVariableProperty->Guid,
                 &CommVariableProperty->VariableProperty
                 );
      CopyMem (SmmVariableFunctionHeader->Data, mVariableBufferPayload, CommBufferPayloadSize);
      break;
    case SMM_VARIABLE_FUNCTION_CLEAR_COMMAND_IN_PROGRESS:
      if (mSmmVariableIoCompletionState.CommandInProgress && !mSmmVariableIoCompletionState.FromSmm) {
        mSmmVariableIoCompletionState.CommandInProgress = FALSE;
      }
      break;
    case SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT:
      if (CommBufferPayloadSize < sizeof (SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT)) {
        DEBUG ((EFI_D_ERROR, "InitRuntimeVariableCacheContext: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      if (mEndOfDxe) {
        DEBUG ((EFI_D_ERROR, "InitRuntimeVariableCacheContext: Cannot init context after end of DXE!\n"));
        return EFI_SUCCESS;
      } else {
        RuntimeVariableCacheContext = (SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT *) SmmVariableFunctionHeader->Data;

        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.Store =
          RuntimeVariableCacheContext->RuntimeNvCache;
        ASSERT (mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.Store != 0);
        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.Store =
          RuntimeVariableCacheContext->RuntimeVolatileCache;
        ASSERT (mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.Store != 0);
        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate =
          RuntimeVariableCacheContext->PendingUpdate;
        ASSERT (mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate != 0);
        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.ReadLock =
          RuntimeVariableCacheContext->ReadLock;
        ASSERT (mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.ReadLock != 0);
      }
      Status = EFI_SUCCESS;
      break;
    case SMM_VARIABLE_FUNCTION_SYNC_RUNTIME_CACHE:
      Status = SynchronizeRuntimeVariableCacheEx ();
      break;
    case SMM_VARIABLE_FUNCTION_GET_TOTAL_STORE_SIZE:
      if (CommBufferPayloadSize < sizeof (SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE)) {
        DEBUG ((EFI_D_ERROR, "GetTotalStoreSize: SMM communication buffer size invalid!\n"));
        return EFI_SUCCESS;
      }
      GetTotalStoreSize = (SMM_VARIABLE_COMMUNICATE_GET_TOTAL_STORE_SIZE *) SmmVariableFunctionHeader->Data;
      GetTotalStoreSize->TotalVolatileStorageSize = PcdGet32 (PcdVariableStoreSize) + GetNonVolatileMaxVariableSize ();
      Status = GetTotalNonVolatileVariableStorageSize (&GetTotalStoreSize->TotalNvStorageSize);
      if (EFI_ERROR (Status)) {
        GetTotalStoreSize->TotalNvStorageSize = 0;
      }
      break;

    default:
      Status = EFI_UNSUPPORTED;
  }

EXIT:

  SmmVariableFunctionHeader->ReturnStatus = Status;

  return EFI_SUCCESS;
}

/**
  SMM END_OF_DXE protocol notification event handler.

  @param  Protocol   Points to the protocol's unique identifier
  @param  Interface  Points to the interface instance
  @param  Handle     The handle on which the interface was installed

  @retval EFI_SUCCESS   SmmEndOfDxeCallback runs successfully

**/
EFI_STATUS
EFIAPI
SmmEndOfDxeCallback (
  IN CONST EFI_GUID                       *Protocol,
  IN VOID                                 *Interface,
  IN EFI_HANDLE                           Handle
  )
{
  DEBUG ((EFI_D_INFO, "[Variable]SMM_END_OF_DXE is signaled\n"));
  MorLockInitAtEndOfDxe ();
  mEndOfDxe = TRUE;
  VarCheckLibInitializeAtEndOfDxe (NULL);
  //
  // The initialization for variable quota.
  //
  InitializeVariableQuota ();
  if (PcdGetBool (PcdReclaimVariableSpaceAtEndOfDxe)) {
    ReclaimForOS ();
  }

  return EFI_SUCCESS;
}

/**
  Non-Volatile variable write service is ready event handler.

**/
VOID
EFIAPI
InstallVariableWriteReady (
  VOID
  )
{
  EFI_STATUS    Status;
  VOID          *Interface;

  DEBUG ((DEBUG_INFO, "Installing SmmVariableWriteGuid to notify wrapper\n"));

  Status = gBS->LocateProtocol (&gSmmVariableWriteGuid, NULL, (VOID **) &Interface);
  if (Status == EFI_NOT_FOUND) {
    //
    // Notify the variable wrapper driver the variable write service is ready
    //
    Status = gBS->InstallProtocolInterface (
                    &mSmmVariableHandle,
                    &gSmmVariableWriteGuid,
                    EFI_NATIVE_INTERFACE,
                    NULL
                    );
    ASSERT_EFI_ERROR (Status);
  }
}

/**
  Common variable initialization for SMM variable support.

  @retval EFI_SUCCESS    SMM variable initialization ran successfully

**/
EFI_STATUS
EFIAPI
VariableSmmCommonInitialize (
  VOID
  )
{
  EFI_STATUS             Status;
  EFI_HANDLE             VariableHandle;

  ///
  /// Variable initialize.
  ///
  Status = VariableCommonInitialize ();
  ASSERT_EFI_ERROR (Status);

  ///
  /// Install the Smm Variable Protocol on a new handle.
  ///
  VariableHandle = NULL;
  Status = gSmst->SmmInstallProtocolInterface (
                    &VariableHandle,
                    &gEdkiiSmmVariableProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &gSmmVariable
                    );
  ASSERT_EFI_ERROR (Status);

  Status = gSmst->SmmInstallProtocolInterface (
                    &VariableHandle,
                    &gEfiSmmVariableProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &gSmmLegacyVariable
                    );
  ASSERT_EFI_ERROR (Status);

  Status = gSmst->SmmInstallProtocolInterface (
                    &VariableHandle,
                    &gEdkiiSmmVarCheckProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &mSmmVarCheck
                    );
  ASSERT_EFI_ERROR (Status);

  Status = gSmst->SmmInstallProtocolInterface (
                    &VariableHandle,
                    &gEdkiiVariableStorageSupportProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &mVariableStorageSupport
                    );
  ASSERT_EFI_ERROR (Status);

  mVariableBufferPayloadSize = GetNonVolatileMaxVariableSize () +
                               OFFSET_OF (SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name) - GetVariableHeaderSize ();

  Status = gSmst->SmmAllocatePool (
                    EfiRuntimeServicesData,
                    mVariableBufferPayloadSize,
                    (VOID **)&mVariableBufferPayload
                    );
  ASSERT_EFI_ERROR (Status);

  ///
  /// Register SMM variable SMI handler
  ///
  VariableHandle = NULL;
  Status = gSmst->SmiHandlerRegister (SmmVariableHandler, &gEfiSmmVariableProtocolGuid, &VariableHandle);
  ASSERT_EFI_ERROR (Status);

  //
  // Notify the variable wrapper driver the variable service is ready
  //
  Status = gBS->InstallProtocolInterface (
                  &mVariableHandle,
                  &gEfiSmmVariableProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &gSmmLegacyVariable
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  SMM EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL notification event handler.

  @param  Protocol       Points to the protocol's unique identifier
  @param  Interface      Points to the interface instance
  @param  Handle         The handle on which the interface was installed

  @retval EFI_SUCCESS    SmmVariableStorageSelectorNotify runs successfully
  @retval EFI_NOT_READY  The SMM variable storage selector protocol was not found

**/
EFI_STATUS
EFIAPI
SmmVariableStorageSelectorNotify (
  IN CONST EFI_GUID      *Protocol,
  IN VOID                *Interface,
  IN EFI_HANDLE          Handle
  )
{
  EFI_STATUS             Status;
  EFI_HANDLE             *Handles;
  UINTN                  Index;

  Handles = NULL;

  ///
  /// Locate the variable storage selector protocol
  ///
  Status = gSmst->SmmLocateProtocol (
                    &gEdkiiSmmVariableStorageSelectorProtocolGuid,
                    NULL,
                    (VOID **) &mVariableModuleGlobal->VariableGlobal.VariableStorageSelectorProtocol
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "VariableStorageSelectorProtocol not found.\n"));
    return EFI_NOT_READY;
  }
  ///
  /// Determine the number of handles
  ///
  Status = gSmst->SmmLocateHandle (
                    ByProtocol,
                    &gEdkiiVariableStorageProtocolGuid,
                    NULL,
                    &mVariableModuleGlobal->VariableGlobal.VariableStoresCount,
                    Handles
                    );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_INFO, "No SMM VariableStorageProtocol instances exist\n"));
    return EFI_NOT_READY;
  }
  Handles = AllocateZeroPool (mVariableModuleGlobal->VariableGlobal.VariableStoresCount);
  if (Handles == NULL) {
    ASSERT (Handles != NULL);
    return EFI_OUT_OF_RESOURCES;
  }
  mVariableModuleGlobal->VariableGlobal.VariableStores = NULL;
  Status = gSmst->SmmAllocatePool (
                    EfiRuntimeServicesData,
                    sizeof (EDKII_VARIABLE_STORAGE_PROTOCOL *) * (mVariableModuleGlobal->VariableGlobal.VariableStoresCount / sizeof(EFI_HANDLE)),
                    (VOID **)&mVariableModuleGlobal->VariableGlobal.VariableStores
                    );
  ASSERT_EFI_ERROR (Status);
  ASSERT (mVariableModuleGlobal->VariableGlobal.VariableStores != NULL);
  if (EFI_ERROR (Status) || mVariableModuleGlobal->VariableGlobal.VariableStores == NULL) {
    FreePool (Handles);
    return EFI_OUT_OF_RESOURCES;
  }
  ///
  /// Get the handles
  ///
  Status = gSmst->SmmLocateHandle (
                    ByProtocol,
                    &gEdkiiVariableStorageProtocolGuid,
                    NULL,
                    &mVariableModuleGlobal->VariableGlobal.VariableStoresCount,
                    Handles
                    );
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    gSmst->SmmFreePool (mVariableModuleGlobal->VariableGlobal.VariableStores);
    FreePool (Handles);
    return Status;
  }
  mVariableModuleGlobal->VariableGlobal.VariableStoresCount /= sizeof (EFI_HANDLE);

  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    Status = gSmst->SmmHandleProtocol (
              Handles[Index],
              &gEdkiiVariableStorageProtocolGuid,
              (VOID **) &mVariableModuleGlobal->VariableGlobal.VariableStores[Index]
              );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      FreePool (Handles);
      return Status;
    }
  }
  FreePool (Handles);

  Status = VariableSmmCommonInitialize ();
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  A callback function that is invoked after the last HOB variable was attempted to be flushed to non-volatile media.

  @param[in]       Context       The HOB variable flush context.
  @param[in]       Status        The result of the last HOB flush variable write operation.

**/
VOID
EFIAPI
FlushHobVariableToStorageSmmCallback (
  IN      VOID                              *Context,            OPTIONAL
  IN      EFI_STATUS                        Status
  )
{
  //
  // Clear completion state context to prepare for a potentially new SetVariable call
  //
  ZeroMem ((VOID *) &mSmmVariableIoCompletionState, sizeof (SMM_VARIABLE_IO_COMPLETION_STATE));

  if (!EFI_ERROR (Status)) {
    //
    // If set variable successful, or the updated or deleted variable is matched with the HOB variable,
    // set the HOB variable to DELETED state in local.
    //
    DEBUG ((
      EFI_D_INFO,
      "  Variable Driver: HOB variable set to DELETED state in local: %g %s\n",
      GetVendorGuidPtr (mSmmVariableFlushHobContext.Variable),
      GetVariableNamePtr (mSmmVariableFlushHobContext.Variable)
      ));
    mSmmVariableFlushHobContext.Variable->State &= VAR_DELETED;
  } else {
    DEBUG ((DEBUG_ERROR, "Variable Driver: Error attempting to set the HOB variable. Status = %r.\n", Status));
    mSmmVariableFlushHobContext.ErrorFlag = TRUE;
  }
  //
  // Restore HOB variable base to the value in the original call
  //
  mVariableModuleGlobal->VariableGlobal.HobVariableBase = mSmmVariableFlushHobContext.HobVariableBase;

  FlushHobVariableToStorage (
    mSmmVariableFlushHobContext.VariableName,
    mSmmVariableFlushHobContext.VendorGuid,
    mSmmVariableFlushHobContext.HobFlushCompleteCallback
    );
}

/**
  Flush the HOB variable to storage.

  @param[in] VariableName       Name of variable has been updated or deleted.
  @param[in] VendorGuid         Guid of variable has been updated or deleted.
  @param[in] Callback           An optional callback function with activities that should occur after HOB flush.

**/
VOID
FlushHobVariableToStorage (
  IN CHAR16                               *VariableName,
  IN EFI_GUID                             *VendorGuid,
  IN VARIABLE_HOB_FLUSH_COMPLETE_CALLBACK Callback      OPTIONAL
  )
{
  EFI_STATUS                    Status;
  VARIABLE_STORE_HEADER         *VariableStoreHeader;
  VARIABLE_HEADER               *Variable;

  if (mVariableModuleGlobal->VariableGlobal.HobVariableBase == 0) {
    return;
  } else if (mSmmVariableFlushHobContext.ErrorFlag) {
    mSmmVariableFlushHobContext.ErrorFlag = FALSE;
    ZeroMem ((VOID *) &mSmmVariableFlushHobContext, sizeof (SMM_VARIABLE_FLUSH_HOB_CONTEXT));
    if (Callback != NULL) {
      Callback ();
    }
    return;
  }

  mSmmVariableFlushHobContext.HobFlushCompleteCallback = Callback;
  mSmmVariableFlushHobContext.HobVariableBase = mVariableModuleGlobal->VariableGlobal.HobVariableBase;
  mSmmVariableFlushHobContext.VariableName = VariableName;
  mSmmVariableFlushHobContext.VendorGuid = VendorGuid;

  VariableStoreHeader = (VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.HobVariableBase;
  //
  // Set HobVariableBase to 0, it can avoid SetVariable to call back.
  //
  mVariableModuleGlobal->VariableGlobal.HobVariableBase = 0;
  for ( Variable = GetStartPointer (VariableStoreHeader)
      ; IsValidVariableHeader (Variable, GetEndPointer (VariableStoreHeader))
      ; Variable = GetNextVariablePtr (Variable)
      ) {
    if (Variable->State != VAR_ADDED) {
      //
      // The HOB variable has been set to DELETED state in local.
      //
      continue;
    }
    ASSERT ((Variable->Attributes & EFI_VARIABLE_NON_VOLATILE) != 0);
    if (
      VendorGuid == NULL ||
      VariableName == NULL ||
      !CompareGuid (VendorGuid, GetVendorGuidPtr (Variable)) ||
      StrCmp (VariableName, GetVariableNamePtr (Variable)) != 0
      ) {
      mSmmVariableFlushHobContext.Variable = Variable;
      Status =  SmmVariableSetVariableInternal (
                  (VOID *) &mSmmVariableFlushHobContext,
                  GetVariableNamePtr (Variable),
                  GetVendorGuidPtr (Variable),
                  Variable->Attributes,
                  DataSizeOfVariable (Variable),
                  GetVariableDataPtr (Variable),
                  FlushHobVariableToStorageSmmCallback,
                  TRUE
                  );
      DEBUG ((
        EFI_D_INFO,
        "  Variable Driver: Flushing HOB variable to storage: %g %s %r\n",
        GetVendorGuidPtr (Variable),
        GetVariableNamePtr (Variable),
        Status
        ));
      break;
    }
  }

  if (!IsValidVariableHeader (Variable, GetEndPointer (VariableStoreHeader))) {
    if (mSmmVariableFlushHobContext.ErrorFlag) {
      //
      // Some HOB variable(s) are still not flushed to storage.
      //
      mVariableModuleGlobal->VariableGlobal.HobVariableBase = (EFI_PHYSICAL_ADDRESS) (UINTN) VariableStoreHeader;
    } else {
      //
      // All HOB variables have been flushed to storage.
      //
      DEBUG ((EFI_D_INFO, "  Variable Driver: All HOB variables have been flushed to storage.\n"));
      if (!AtRuntime ()) {
        FreePool ((VOID *) VariableStoreHeader);
      }
    }
    ZeroMem ((VOID *) &mSmmVariableFlushHobContext, sizeof (SMM_VARIABLE_FLUSH_HOB_CONTEXT));
    if (Callback != NULL) {
      Callback ();
    }
  }
}

/**
  Variable SMM Driver main entry point.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       Variable service successfully initialized.

**/
EFI_STATUS
EFIAPI
VariableServiceInitialize (
  IN EFI_HANDLE                           ImageHandle,
  IN EFI_SYSTEM_TABLE                     *SystemTable
  )
{
  EFI_STATUS                              Status;
  VOID                                    *SmmEndOfDxeRegistration;
  VOID                                    *SmmVariableStorageSelectorNotifyRegistration;

  ///
  /// Allocate runtime memory for variable driver global structure.
  ///
  mVariableModuleGlobal = NULL;
  Status = gSmst->SmmAllocatePool (
                    EfiRuntimeServicesData,
                    sizeof (VARIABLE_MODULE_GLOBAL),
                    (VOID **)&mVariableModuleGlobal
                    );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status) || mVariableModuleGlobal == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem (mVariableModuleGlobal, sizeof (VARIABLE_MODULE_GLOBAL));
  ZeroMem ((VOID *) &mSmmVariableIoCompletionState, sizeof (SMM_VARIABLE_IO_COMPLETION_STATE));

  mVariableModuleGlobal->VariableGlobal.VariableStoresCount = 0;

  ///
  /// Determine if Force Volatile Mode is enabled
  ///
  mNvVariableEmulationMode = PcdGetBool (PcdNvVariableEmulationMode);

  if (!mNvVariableEmulationMode) {
    //
    // Register EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL notify function.
    //
    Status = gSmst->SmmRegisterProtocolNotify (
                      &gEdkiiSmmVariableStorageSelectorProtocolGuid,
                      SmmVariableStorageSelectorNotify,
                      &SmmVariableStorageSelectorNotifyRegistration
                      );
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }
    Status = SmmVariableStorageSelectorNotify (NULL, NULL, NULL);
    if (EFI_ERROR (Status) && Status != EFI_NOT_READY) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }
  } else {
    Status = VariableSmmCommonInitialize ();
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Register EFI_SMM_END_OF_DXE_PROTOCOL_GUID notify function.
  //
  Status = gSmst->SmmRegisterProtocolNotify (
                    &gEfiSmmEndOfDxeProtocolGuid,
                    SmmEndOfDxeCallback,
                    &SmmEndOfDxeRegistration
                    );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
