/** @file
  The common variable volatile store routines shared by the DXE_RUNTIME variable
  module and the DXE_SMM variable module.

  Caution: This module requires additional review when modified.
  This driver will have external input - variable data. They may be input in SMM mode.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "VariableHelpers.h"
#include "VariableVolatile.h"

extern VARIABLE_MODULE_GLOBAL   *mVariableModuleGlobal;
extern VARIABLE_STORE_HEADER    *mNvVariableCache;
extern CHAR16                   mVariableNameBuffer[MAX_VARIABLE_NAME_SIZE];

/**
  Copies any pending updates to runtime variable caches.

  @retval EFI_UNSUPPORTED         The volatile store to be updated is not initialized properly.
  @retval EFI_SUCCESS             The volatile store was updated successfully.

**/
EFI_STATUS
SynchronizeRuntimeVariableCacheEx (
  VOID
  )
{
  if (
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.Store == NULL ||
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.Store == NULL ||
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate == NULL
    ) {
    return EFI_UNSUPPORTED;
  }

  if (*(mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate)) {
    CopyMem (
      (VOID *) (
        ((UINT8 *) mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.Store) +
        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.PendingUpdateOffset
        ),
      (VOID *) (
        ((UINT8 *) mNvVariableCache) +
        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.PendingUpdateOffset
        ),
      mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.PendingUpdateLength
      );
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.PendingUpdateLength = 0;
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache.PendingUpdateOffset = 0;

    CopyMem (
      (VOID *) (
        ((UINT8 *) mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.Store) +
        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.PendingUpdateOffset
      ),
      (VOID *) (
        ((UINT8 *) (UINTN) mVariableModuleGlobal->VariableGlobal.VolatileVariableBase) +
        mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.PendingUpdateOffset
        ),
      mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.PendingUpdateLength
      );
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.PendingUpdateLength = 0;
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache.PendingUpdateOffset = 0;
    *(mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate) = FALSE;
  }

  return EFI_SUCCESS;
}

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
  )
{
  if (VariableRuntimeCache == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (
    VariableRuntimeCache->Store == NULL ||
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate == NULL ||
    mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.ReadLock == NULL
    ) {
    return EFI_UNSUPPORTED;
  }

  if (
    *(mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate) &&
    VariableRuntimeCache->PendingUpdateLength > 0
    ) {
    VariableRuntimeCache->PendingUpdateLength =
      (UINT32) (
        MAX (
          (UINTN) (VariableRuntimeCache->PendingUpdateOffset + VariableRuntimeCache->PendingUpdateLength),
          Offset + Length
        ) - MIN ((UINTN) VariableRuntimeCache->PendingUpdateOffset, Offset)
      );
    VariableRuntimeCache->PendingUpdateOffset =
      (UINT32) MIN ((UINTN) VariableRuntimeCache->PendingUpdateOffset, Offset);
  } else {
    VariableRuntimeCache->PendingUpdateLength = (UINT32) Length;
    VariableRuntimeCache->PendingUpdateOffset = (UINT32) Offset;
  }
  *(mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.PendingUpdate) = TRUE;

  if (*(mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.ReadLock) == FALSE) {
    return SynchronizeRuntimeVariableCacheEx ();
  }

  return EFI_SUCCESS;
}

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
  )
{
  EFI_PHYSICAL_ADDRESS  DataPtr;

  if (Global == NULL || SourceDataBuffer == NULL || DestinationStoreBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Data Pointer should point to the actual address where the data is to be written.
  //
  DataPtr = DataPtrIndex;
  if (SetByIndex) {
    DataPtr += (EFI_PHYSICAL_ADDRESS) (UINTN) DestinationStoreBuffer;
  }

  if ((DataPtr + DataSize) >= ((UINTN) ((UINT8 *) DestinationStoreBuffer + DestinationStoreBuffer->Size))) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem ((VOID *) (UINTN) DataPtr, (VOID *) SourceDataBuffer, DataSize);

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS              Status;
  VARIABLE_POINTER_TRACK  PtrTrack;
  BOOLEAN                 TempCommandInProgress;
  EFI_GUID                TempInProgressInstanceGuid;

  PtrTrack.CurrPtr                 = NULL;
  PtrTrack.InDeletedTransitionPtr  = NULL;
  PtrTrack.StartPtr                = GetStartPointer (mNvVariableCache);
  PtrTrack.EndPtr                  = GetEndPointer   (mNvVariableCache);
  PtrTrack.Volatile                = FALSE;
  Status = FindVariableEx (VariableName, VendorGuid, TRUE, &PtrTrack);
  if (EFI_ERROR (Status) && Status != EFI_NOT_FOUND) {
    return Status;
  } else if (Status == EFI_NOT_FOUND) {
    PtrTrack.CurrPtr                 = NULL;
    PtrTrack.InDeletedTransitionPtr  = NULL;
  }
  Status = UpdateVariableInternal (
             VariableName,
             VendorGuid,
             Data,
             DataSize,
             Attributes,
             KeyIndex,
             MonotonicCount,
             &PtrTrack,
             TimeStamp,
             TRUE,
             &TempCommandInProgress,
             &TempInProgressInstanceGuid
             );
  DEBUG ((DEBUG_VERBOSE, "  Variable Driver: UpdateVariable status = %r.\n", Status));
  //
  // CommandInProgress should never be TRUE since we are only doing an NV cache update
  //
  ASSERT (!TempCommandInProgress);
  if (TempCommandInProgress) {
    return EFI_OUT_OF_RESOURCES;
  }
  return Status;
}

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
  )
{
  VARIABLE_POINTER_TRACK  Variable;
  EFI_STATUS              Status;

  if (mVariableModuleGlobal->VariableGlobal.HobVariableBase == 0) {
    return FALSE;
  }

  Variable.CurrPtr  = NULL;
  Variable.StartPtr = GetStartPointer ((VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.HobVariableBase);
  Variable.EndPtr   = GetEndPointer   ((VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.HobVariableBase);
  Variable.Volatile = FALSE;

  Status = FindVariableEx ((CHAR16 *) VariableName, (EFI_GUID *) VariableGuid, FALSE, &Variable);
  if ((Variable.CurrPtr != NULL) && (!EFI_ERROR (Status))) {
    return TRUE;
  } else {
    if (Status == EFI_NOT_FOUND) {
      return FALSE;
    }
    ASSERT_EFI_ERROR (Status);
    return FALSE;
  }
}

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
  )
{
  VARIABLE_STORE_TYPE     Type;
  VARIABLE_STORE_TYPE     MaxSearch;
  VARIABLE_POINTER_TRACK  Variable;
  VARIABLE_POINTER_TRACK  VariablePtrTrack;
  EFI_STATUS              Status;
  VARIABLE_STORE_HEADER   *VariableStoreHeader[VariableStoreTypeMax];

  *CurrentVariableInMemory = FALSE;
  if (VariableName[0] != 0 && VendorGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // 0: Volatile, 1: HOB
  // The index and attributes mapping must be kept in this order to implement
  // the variable enumeration search algorithm.
  //
  VariableStoreHeader[VariableStoreTypeVolatile] = (VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.VolatileVariableBase;
  VariableStoreHeader[VariableStoreTypeHob]      = (VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.HobVariableBase;
  VariableStoreHeader[VariableStoreTypeNvCache]  = mNvVariableCache;
  if (mNvVariableEmulationMode) {
    MaxSearch = VariableStoreTypeNvCache;
  } else {
    MaxSearch = VariableStoreTypeHob;
  }

  //
  // Find the current variable by walk through HOB and volatile variable store.
  //
  Variable.CurrPtr  = NULL;
  Status            = EFI_NOT_FOUND;
  for (Type = (VARIABLE_STORE_TYPE) 0; Type <= MaxSearch; Type++) {
    if (VariableStoreHeader[Type] == NULL) {
      continue;
    }

    Variable.StartPtr = GetStartPointer (VariableStoreHeader[Type]);
    Variable.EndPtr   = GetEndPointer   (VariableStoreHeader[Type]);
    Variable.Volatile = (BOOLEAN) (Type == VariableStoreTypeVolatile);

    Status = FindVariableEx (VariableName, VendorGuid, FALSE, &Variable);
    if (!EFI_ERROR (Status)) {
      break;
    }
  }
  if (Variable.CurrPtr == NULL || EFI_ERROR (Status)) {
    goto Done;
  }
  *CurrentVariableInMemory = TRUE;

  if (VariableName[0] != 0) {
    //
    // If variable name is not NULL, get next variable.
    //
    Variable.CurrPtr = GetNextVariablePtr (Variable.CurrPtr);
  }

  while (TRUE) {
    //
    // Switch from Volatile to HOB
    //
    while (!IsValidVariableHeader (Variable.CurrPtr, Variable.EndPtr)) {
      //
      // Find current storage index
      //
      for (Type = (VARIABLE_STORE_TYPE) 0; Type <= VariableStoreTypeHob; Type++) {
        if ((VariableStoreHeader[Type] != NULL) && (Variable.StartPtr == GetStartPointer (VariableStoreHeader[Type]))) {
          break;
        }
      }
      if (!mNvVariableEmulationMode) {
        ASSERT (Type <= VariableStoreTypeHob);
      }
      //
      // Switch to next storage
      //
      for (Type++; Type < VariableStoreTypeMax; Type++) {
        if (VariableStoreHeader[Type] != NULL) {
          break;
        }
      }
      //
      // Capture the case that
      // 1. current storage is the last one, or
      // 2. no further storage
      //
      if (Type > MaxSearch) {
        Status = EFI_NOT_FOUND;
        goto Done;
      }
      Variable.StartPtr = GetStartPointer (VariableStoreHeader[Type]);
      Variable.EndPtr   = GetEndPointer   (VariableStoreHeader[Type]);
      Variable.CurrPtr  = Variable.StartPtr;
    }

    //
    // Variable is found
    //
    if (Variable.CurrPtr->State == VAR_ADDED || Variable.CurrPtr->State == (VAR_IN_DELETED_TRANSITION & VAR_ADDED)) {
      if (!AtRuntime () || ((Variable.CurrPtr->Attributes & EFI_VARIABLE_RUNTIME_ACCESS) != 0)) {
        if (Variable.CurrPtr->State == (VAR_IN_DELETED_TRANSITION & VAR_ADDED)) {
          //
          // If it is a IN_DELETED_TRANSITION variable,
          // and there is also a same ADDED one at the same time,
          // don't return it.
          //
          VariablePtrTrack.StartPtr = Variable.StartPtr;
          VariablePtrTrack.EndPtr = Variable.EndPtr;
          Status = FindVariableEx (
                     GetVariableNamePtr (Variable.CurrPtr),
                     GetVendorGuidPtr (Variable.CurrPtr),
                     FALSE,
                     &VariablePtrTrack
                     );
          if (!EFI_ERROR (Status) && VariablePtrTrack.CurrPtr->State == VAR_ADDED) {
            Variable.CurrPtr = GetNextVariablePtr (Variable.CurrPtr);
            continue;
          }
        }

        *VariablePtr = Variable.CurrPtr;
        Status = EFI_SUCCESS;
        goto Done;
      }
    }

    Variable.CurrPtr = GetNextVariablePtr (Variable.CurrPtr);
  }

Done:
  return Status;
}