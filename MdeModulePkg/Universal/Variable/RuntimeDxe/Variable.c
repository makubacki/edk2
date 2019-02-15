/** @file
  The common variable operation routines shared by DXE_RUNTIME variable
  module and DXE_SMM variable module.

  Caution: This module requires additional review when modified.
  This driver will have external input - variable data. They may be input in SMM mode.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

  VariableServiceGetNextVariableName () and VariableServiceQueryVariableInfo() are external API.
  They need check input parameter.

  VariableServiceGetVariable() and VariableServiceSetVariable() are external API
  to receive datasize and data buffer. The size should be checked carefully.

  VariableServiceSetVariable() should also check authenticate data to avoid buffer overflow,
  integer overflow. It should also check attribute to avoid authentication bypass.

Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
(C) Copyright 2015-2018 Hewlett Packard Enterprise Development LP<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Variable.h"
#include "VariableHelpers.h"
#include "VariableNonVolatile.h"
#include "VariableVolatile.h"
#include "VariableVolatileCommon.h"
#include "VariableStorage.h"

VARIABLE_MODULE_GLOBAL  *mVariableModuleGlobal;

///
/// A flag which indicates whether all variables should be treated as volatile
///
BOOLEAN                mNvVariableEmulationMode;

///
/// Define a memory cache that improves the search performance for a variable.
///
VARIABLE_STORE_HEADER  *mNvVariableCache        = NULL;

///
/// The buffer size used for reclaim.
///
VOID                   *mReclaimBuffer          = NULL;
UINT32                 mReclaimBufferSize       = 0;

///
/// The memory entry used for variable statistics data.
///
VARIABLE_INFO_ENTRY    *gVariableInfo           = NULL;

///
/// A flag to indicate whether the Variable Storage driver requires
/// asyncronous I/O using OS runtime drivers to complete the current command.
/// This flag is only used with the SMM version of UEFI variable services.
/// If this flag is TRUE, then the driver must exit SMM and use the
/// corresponding EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL in Runtime DXE to
/// complete the request.  This allows the EDKII_VARIABLE_STORAGE_PROTOCOL to
/// interact with platform specific OS drivers to execute the I/O, which would
/// not be possible in SMM since the OS is halted. If the request originates
/// from the EFI_SMM_VARIABLE_PROTOCOL/EDKII_SMM_VARIABLE_PROTOCOL, then the
/// Variable Storage driver will need to set up a SMI mechanism since it is
/// impossible for the EDKII_VARIABLE_STORAGE_IO_COMPLETION_PROTOCOL to be called.
///
BOOLEAN                mCommandInProgress       = FALSE;

///
/// If mCommandInProgress is TRUE, this will contain the instance GUID of the
/// Variable Storage driver that is performing the asyncronous I/O
///
EFI_GUID               mInProgressInstanceGuid  = { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }};

///
/// A flag to indicate whether the platform has left the DXE phase of execution.
///
BOOLEAN                mEndOfDxe                = FALSE;

///
/// Indicates if the source of the variable services call is SMM or DXE
///
BOOLEAN                mFromSmm                 = FALSE;

///
/// Indicates if the authenticated variable checks are ignored during a set variable operation.
/// This should only be TRUE during the HOB variable flush.
///
BOOLEAN                mIgnoreAuthCheck         = FALSE;

///
/// Buffer for temporary storage of variable contents when copying to NV cache
///
VOID                  *mVariableDataBuffer      = NULL;

///
/// It indicates the var check request source.
/// In the implementation, DXE is regarded as untrusted, and SMM is trusted.
///
VAR_CHECK_REQUEST_SOURCE mRequestSource       = VarCheckFromUntrusted;
CHAR16                   mVariableNameBuffer[MAX_VARIABLE_NAME_SIZE];

//
// It will record the current boot error flag before EndOfDxe.
//
VAR_ERROR_FLAG          mCurrentBootVarErrFlag = VAR_ERROR_FLAG_NO_ERROR;

VARIABLE_ENTRY_PROPERTY mVariableEntryProperty[] = {
  {
    &gEdkiiVarErrorFlagGuid,
    VAR_ERROR_FLAG_NAME,
    {
      VAR_CHECK_VARIABLE_PROPERTY_REVISION,
      VAR_CHECK_VARIABLE_PROPERTY_READ_ONLY,
      VARIABLE_ATTRIBUTE_NV_BS_RT,
      sizeof (VAR_ERROR_FLAG),
      sizeof (VAR_ERROR_FLAG)
    }
  },
};

AUTH_VAR_LIB_CONTEXT_IN mAuthContextIn = {
  AUTH_VAR_LIB_CONTEXT_IN_STRUCT_VERSION,
  //
  // StructSize, TO BE FILLED
  //
  0,
  //
  // MaxAuthVariableSize, TO BE FILLED
  //
  0,
  VariableExLibFindVariable,
  VariableExLibFindNextVariable,
  VariableExLibUpdateVariable,
  VariableExLibGetScratchBuffer,
  VariableExLibCheckRemainingSpaceForConsistency,
  VariableExLibAtRuntime,
};

AUTH_VAR_LIB_CONTEXT_OUT mAuthContextOut;

/**
  Routine used to track statistical information about variable usage.
  The data is stored in the EFI system table so it can be accessed later.
  VariableInfo.efi can dump out the table. Only Boot Services variable
  accesses are tracked by this code. The PcdVariableCollectStatistics
  build flag controls if this feature is enabled.

  A read that hits in the cache will have Read and Cache true for
  the transaction. Data is allocated by this routine, but never
  freed.

  @param[in] VariableName   Name of the Variable to track.
  @param[in] VendorGuid     Guid of the Variable to track.
  @param[in] Volatile       TRUE if volatile FALSE if non-volatile.
  @param[in] Read           TRUE if GetVariable() was called.
  @param[in] Write          TRUE if SetVariable() was called.
  @param[in] Delete         TRUE if deleted via SetVariable().
  @param[in] Cache          TRUE for a cache hit.

**/
VOID
UpdateVariableInfo (
  IN  CHAR16                  *VariableName,
  IN  EFI_GUID                *VendorGuid,
  IN  BOOLEAN                 Volatile,
  IN  BOOLEAN                 Read,
  IN  BOOLEAN                 Write,
  IN  BOOLEAN                 Delete,
  IN  BOOLEAN                 Cache
  )
{
  VARIABLE_INFO_ENTRY   *Entry;

  if (FeaturePcdGet (PcdVariableCollectStatistics)) {

    if (AtRuntime ()) {
      // Don't collect statistics at runtime.
      return;
    }

    if (gVariableInfo == NULL) {
      //
      // On the first call allocate a entry and place a pointer to it in
      // the EFI System Table.
      //
      gVariableInfo = AllocateZeroPool (sizeof (VARIABLE_INFO_ENTRY));

      if (gVariableInfo == NULL) {
        ASSERT (gVariableInfo != NULL);
        return;
      }

      CopyGuid (&gVariableInfo->VendorGuid, VendorGuid);
      gVariableInfo->Name = AllocateZeroPool (StrSize (VariableName));

      if (gVariableInfo->Name == NULL) {
        ASSERT (gVariableInfo->Name != NULL);
        return;
      }

      StrCpyS (gVariableInfo->Name, StrSize (VariableName) / sizeof (CHAR16), VariableName);
      gVariableInfo->Volatile = Volatile;
    }


    for (Entry = gVariableInfo; Entry != NULL; Entry = Entry->Next) {
      if (CompareGuid (VendorGuid, &Entry->VendorGuid)) {
        if (StrCmp (VariableName, Entry->Name) == 0) {
          if (Read) {
            Entry->ReadCount++;
          }
          if (Write) {
            Entry->WriteCount++;
          }
          if (Delete) {
            Entry->DeleteCount++;
          }
          if (Cache) {
            Entry->CacheCount++;
          }

          return;
        }
      }

      if (Entry->Next == NULL) {
        //
        // If the entry is not in the table add it.
        // Next iteration of the loop will fill in the data.
        //
        Entry->Next = AllocateZeroPool (sizeof (VARIABLE_INFO_ENTRY));

        if (Entry->Next == NULL) {
          ASSERT (Entry->Next != NULL);
          return;
        }

        CopyGuid (&Entry->Next->VendorGuid, VendorGuid);
        Entry->Next->Name = AllocateZeroPool (StrSize (VariableName));
        ASSERT (Entry->Next->Name != NULL);
        StrCpyS (Entry->Next->Name, StrSize(VariableName)/sizeof(CHAR16), VariableName);
        Entry->Next->Volatile = Volatile;
      }

    }
  }
}

/**
  Record variable error flag.

  @param[in]  Flag                    Variable error flag to record.
  @param[in]  VariableName            Name of variable.
  @param[in]  VendorGuid              Guid of variable.
  @param[in]  Attributes              Attributes of the variable.
  @param[in]  VariableSize            Size of the variable.
  @param[out] CommandInProgress       TRUE if the command requires asyncronous I/O and has not completed yet.
                                      Asyncronous I/O should only be required during OS runtime phase, this
                                      return value will be FALSE during all Pre-OS stages.
  @param[out] InProgressInstanceGuid  If CommandInProgress is TRUE, this will contain the instance GUID of the Variable
                                      Storage driver that is performing the asyncronous I/O

**/
VOID
RecordVarErrorFlag (
  IN  VAR_ERROR_FLAG        Flag,
  IN  CHAR16                *VariableName,
  IN  EFI_GUID              *VendorGuid,
  IN  UINT32                Attributes,
  IN  UINTN                 VariableSize,
  OUT BOOLEAN               *CommandInProgress,
  OUT EFI_GUID              *InProgressInstanceGuid
  )
{
  EFI_STATUS                        Status;
  VARIABLE_POINTER_TRACK            Variable;
  VAR_ERROR_FLAG                    *VarErrFlag;
  VAR_ERROR_FLAG                    TempFlag;
  EDKII_VARIABLE_STORAGE_PROTOCOL   *VariableStorageProtocol;

  DEBUG_CODE (
    DEBUG ((EFI_D_ERROR, "  Variable Driver: RecordVarErrorFlag (0x%02x) %s:%g - 0x%08x - 0x%x\n", Flag, VariableName, VendorGuid, Attributes, VariableSize));
    if (Flag == VAR_ERROR_FLAG_SYSTEM_ERROR) {
      if (AtRuntime ()) {
        DEBUG ((EFI_D_ERROR, "  Variable Driver: CommonRuntimeVariableSpace = 0x%x - CommonVariableTotalSize = 0x%x\n", mVariableModuleGlobal->CommonRuntimeVariableSpace, mVariableModuleGlobal->CommonVariableTotalSize));
      } else {
        DEBUG ((EFI_D_ERROR, "  Variable Driver: CommonVariableSpace = 0x%x - CommonVariableTotalSize = 0x%x\n", mVariableModuleGlobal->CommonVariableSpace, mVariableModuleGlobal->CommonVariableTotalSize));
      }
    } else {
      DEBUG ((EFI_D_ERROR, "  Variable Driver: CommonMaxUserVariableSpace = 0x%x - CommonUserVariableTotalSize = 0x%x\n", mVariableModuleGlobal->CommonMaxUserVariableSpace, mVariableModuleGlobal->CommonUserVariableTotalSize));
    }
  );

  *CommandInProgress = FALSE;
  if (!mEndOfDxe) {
    //
    // Before EndOfDxe, just record the current boot variable error flag to local variable,
    // and leave the variable error flag in NV flash as the last boot variable error flag.
    // After EndOfDxe in InitializeVarErrorFlag (), the variable error flag in NV flash
    // will be initialized to this local current boot variable error flag.
    //
    mCurrentBootVarErrFlag &= Flag;
    return;
  }

  //
  // Record error flag (it should be initialized).
  //
  Status = FindVariable (
             VAR_ERROR_FLAG_NAME,
             &gEdkiiVarErrorFlagGuid,
             &Variable,
             &mVariableModuleGlobal->VariableGlobal,
             FALSE,
             CommandInProgress,
             InProgressInstanceGuid
             );
  //
  // VarErrorFlag should always be in the NV cache
  //
  ASSERT (!(*CommandInProgress));
  if (*CommandInProgress) {
    return;
  }
  if (!EFI_ERROR (Status)) {
    VarErrFlag = (VAR_ERROR_FLAG *) GetVariableDataPtr (Variable.CurrPtr);
    TempFlag = *VarErrFlag;
    TempFlag &= Flag;
    if (TempFlag == *VarErrFlag) {
      return;
    }
    VariableStorageProtocol = NULL;
    Status = GetVariableStorageProtocol (
               VAR_ERROR_FLAG_NAME,
               &gEdkiiVarErrorFlagGuid,
               &VariableStorageProtocol
               );
    if (!EFI_ERROR (Status) && VariableStorageProtocol != NULL) {
      //
      // Update the data in NV
      //
      if (VariableStorageProtocol->WriteServiceIsReady (
                                        VariableStorageProtocol)) {
        Status = VariableStorageProtocol->SetVariable (
                                            VariableStorageProtocol,
                                            AtRuntime (),
                                            mFromSmm,
                                            VAR_ERROR_FLAG_NAME,
                                            &gEdkiiVarErrorFlagGuid,
                                            VARIABLE_ATTRIBUTE_NV_BS_RT,
                                            sizeof (TempFlag),
                                            &TempFlag,
                                            0,
                                            0,
                                            NULL,
                                            CommandInProgress
                                            );
        if (!EFI_ERROR (Status)) {
          if (*CommandInProgress) {
            VariableStorageProtocol->GetId (VariableStorageProtocol, InProgressInstanceGuid);
          }
          //
          // Update the data in NV cache.
          //
          *VarErrFlag = TempFlag;
        }
      }
    }
  }
}

/**
  Initialize variable error flag.

  Before EndOfDxe, the variable indicates the last boot variable error flag,
  then it means the last boot variable error flag must be got before EndOfDxe.
  After EndOfDxe, the variable indicates the current boot variable error flag,
  then it means the current boot variable error flag must be got after EndOfDxe.

**/
VOID
InitializeVarErrorFlag (
  VOID
  )
{
  EFI_STATUS                Status;
  VARIABLE_POINTER_TRACK    Variable;
  VAR_ERROR_FLAG            Flag;
  VAR_ERROR_FLAG            VarErrFlag;
  BOOLEAN                   CommandInProgress;
  EFI_GUID                  InProgressInstanceGuid;

  if (!mEndOfDxe) {
    return;
  }

  Flag = mCurrentBootVarErrFlag;
  DEBUG ((EFI_D_INFO, "  Variable Driver: Initialize variable error flag (%02x)\n", Flag));

  Status = FindVariable (
             VAR_ERROR_FLAG_NAME,
             &gEdkiiVarErrorFlagGuid,
             &Variable,
             &mVariableModuleGlobal->VariableGlobal,
             FALSE,
             &CommandInProgress,
             &InProgressInstanceGuid
             );
  if (!EFI_ERROR (Status)) {
    VarErrFlag = *((VAR_ERROR_FLAG *) GetVariableDataPtr (Variable.CurrPtr));
    if (VarErrFlag == Flag) {
      return;
    }
  }

  UpdateVariable (
    VAR_ERROR_FLAG_NAME,
    &gEdkiiVarErrorFlagGuid,
    &Flag,
    sizeof (Flag),
    VARIABLE_ATTRIBUTE_NV_BS_RT,
    0,
    0,
    &Variable,
    NULL
    );
}

/**
  Is user variable?

  @param[in] Variable   Pointer to variable header.

  @retval TRUE          User variable.
  @retval FALSE         System variable.

**/
BOOLEAN
IsUserVariable (
  IN VARIABLE_HEADER    *Variable
  )
{
  VAR_CHECK_VARIABLE_PROPERTY   Property;

  //
  // Only after End Of Dxe, the variables belong to system variable are fixed.
  // If PcdMaxUserNvStorageVariableSize is 0, it means user variable share the same NV storage with system variable,
  // then no need to check if the variable is user variable or not specially.
  //
  if (mEndOfDxe && (mVariableModuleGlobal->CommonMaxUserVariableSpace != mVariableModuleGlobal->CommonVariableSpace)) {
    if (VarCheckLibVariablePropertyGet (GetVariableNamePtr (Variable), GetVendorGuidPtr (Variable), &Property) == EFI_NOT_FOUND) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
  Calculate common user variable total size.

**/
VOID
CalculateCommonUserVariableTotalSize (
  VOID
  )
{
  VARIABLE_HEADER               *Variable;
  VARIABLE_HEADER               *NextVariable;
  UINTN                         VariableSize;
  VAR_CHECK_VARIABLE_PROPERTY   Property;

  //
  // Only after End Of Dxe, the variables belong to system variable are fixed.
  // If PcdMaxUserNvStorageVariableSize is 0, it means user variable share the same NV storage with system variable,
  // then no need to calculate the common user variable total size specially.
  //
  if (mEndOfDxe && (mVariableModuleGlobal->CommonMaxUserVariableSpace != mVariableModuleGlobal->CommonVariableSpace)) {
    Variable = GetStartPointer (mNvVariableCache);
    while (IsValidVariableHeader (Variable, GetEndPointer (mNvVariableCache))) {
      NextVariable = GetNextVariablePtr (Variable);
      VariableSize = (UINTN) NextVariable - (UINTN) Variable;
      if ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
        if (VarCheckLibVariablePropertyGet (GetVariableNamePtr (Variable), GetVendorGuidPtr (Variable), &Property) == EFI_NOT_FOUND) {
          //
          // No property, it is user variable.
          //
          mVariableModuleGlobal->CommonUserVariableTotalSize += VariableSize;
        }
      }

      Variable = NextVariable;
    }
  }
}

/**
  Initialize variable quota.

**/
VOID
InitializeVariableQuota (
  VOID
  )
{
  if (!mEndOfDxe) {
    return;
  }

  InitializeVarErrorFlag ();
  CalculateCommonUserVariableTotalSize ();
}

/**

  Variable store garbage collection and reclaim operation.

  @param[in]      VariableBase            Base address of variable store.
  @param[out]     LastVariableOffset      Offset of last variable.
  @param[in]      IsVolatile              The variable store is volatile or not;
                                          if it is non-volatile, need FTW.
  @param[in, out] UpdatingPtrTrack        Pointer to updating variable pointer track structure.
  @param[in]      NewVariable             Pointer to new variable.
  @param[in]      NewVariableSize         New variable size.

  @return EFI_SUCCESS                  Reclaim operation has finished successfully.
  @return EFI_OUT_OF_RESOURCES         No enough memory resources or variable space.
  @return Others                       Unexpect error happened during reclaim operation.

**/
EFI_STATUS
Reclaim (
  IN     EFI_PHYSICAL_ADDRESS         VariableBase,
  OUT    UINTN                        *LastVariableOffset,
  IN     BOOLEAN                      IsVolatile,
  IN OUT VARIABLE_POINTER_TRACK       *UpdatingPtrTrack,
  IN     VARIABLE_HEADER              *NewVariable,
  IN     UINTN                        NewVariableSize,
  OUT    BOOLEAN                      *CommandInProgress,
  OUT    EFI_GUID                     *InProgressInstanceGuid
  )
{
  VARIABLE_HEADER                 *Variable;
  VARIABLE_HEADER                 *AddedVariable;
  VARIABLE_HEADER                 *NextVariable;
  VARIABLE_HEADER                 *NextAddedVariable;
  VARIABLE_STORE_HEADER           *VariableStoreHeader;
  UINT8                           *ValidBuffer;
  UINTN                           MaximumBufferSize;
  UINTN                           VariableSize;
  UINTN                           NameSize;
  UINTN                           Index;
  UINT8                           *CurrPtr;
  VOID                            *Point0;
  VOID                            *Point1;
  BOOLEAN                         FoundAdded;
  EFI_STATUS                      Status;
  UINTN                           CommonVariableTotalSize;
  UINTN                           CommonUserVariableTotalSize;
  UINTN                           HwErrVariableTotalSize;
  VARIABLE_HEADER                 *UpdatingVariable;
  VARIABLE_HEADER                 *UpdatingInDeletedTransition;
  AUTHENTICATED_VARIABLE_HEADER   *AuthVariable;
  EDKII_VARIABLE_STORAGE_PROTOCOL *VariableStorageProtocol;

  Status = EFI_SUCCESS;

  UpdatingVariable            = NULL;
  UpdatingInDeletedTransition = NULL;

  if (UpdatingPtrTrack != NULL) {
    UpdatingVariable = UpdatingPtrTrack->CurrPtr;
    UpdatingInDeletedTransition = UpdatingPtrTrack->InDeletedTransitionPtr;
  }

  VariableStoreHeader = (VARIABLE_STORE_HEADER *) ((UINTN) VariableBase);

  CommonVariableTotalSize     = 0;
  CommonUserVariableTotalSize = 0;
  HwErrVariableTotalSize      = 0;

  //
  // Start Pointers for the variable.
  //
  Variable          = GetStartPointer (VariableStoreHeader);
  MaximumBufferSize = sizeof (VARIABLE_STORE_HEADER);

  while (IsValidVariableHeader (Variable, GetEndPointer (VariableStoreHeader))) {
    NextVariable = GetNextVariablePtr (Variable);
    if ((Variable->State == VAR_ADDED || Variable->State == (VAR_IN_DELETED_TRANSITION & VAR_ADDED)) &&
        Variable != UpdatingVariable &&
        Variable != UpdatingInDeletedTransition
       ) {
      VariableSize = (UINTN) NextVariable - (UINTN) Variable;
      MaximumBufferSize += VariableSize;
    }

    Variable = NextVariable;
  }

  if (NewVariable != NULL) {
    //
    // Add the new variable size.
    //
    MaximumBufferSize += NewVariableSize;
  }

  //
  // Reserve the 1 Bytes with Oxff to identify the
  // end of the variable buffer.
  //
  MaximumBufferSize += 1;
  if (MaximumBufferSize > mReclaimBufferSize) {
    DEBUG ((DEBUG_ERROR, "Required reclaim memory exceeds the reclaim buffer size\n"));
    ASSERT (FALSE); //This should never happen
    return EFI_OUT_OF_RESOURCES;
  }
  SetMem32 (mReclaimBuffer, mReclaimBufferSize, (UINT32) 0xFFFFFFFF);
  ValidBuffer = mReclaimBuffer;

  //
  // Copy variable store header.
  //
  CopyMem (ValidBuffer, VariableStoreHeader, sizeof (VARIABLE_STORE_HEADER));
  CurrPtr = (UINT8 *) GetStartPointer ((VARIABLE_STORE_HEADER *) ValidBuffer);

  //
  // Reinstall all ADDED variables as long as they are not identical to Updating Variable.
  //
  Variable = GetStartPointer (VariableStoreHeader);
  while (IsValidVariableHeader (Variable, GetEndPointer (VariableStoreHeader))) {
    NextVariable = GetNextVariablePtr (Variable);
    if (Variable != UpdatingVariable && Variable->State == VAR_ADDED) {
      VariableSize = (UINTN) NextVariable - (UINTN) Variable;
      CopyMem (CurrPtr, (UINT8 *) Variable, VariableSize);
      CurrPtr += VariableSize;
      if ((!IsVolatile) && ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD)) {
        HwErrVariableTotalSize += VariableSize;
      } else if ((!IsVolatile) && ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != EFI_VARIABLE_HARDWARE_ERROR_RECORD)) {
        CommonVariableTotalSize += VariableSize;
        if (IsUserVariable (Variable)) {
          CommonUserVariableTotalSize += VariableSize;
        }
      }
    }
    Variable = NextVariable;
  }

  //
  // Reinstall all in delete transition variables.
  //
  Variable = GetStartPointer (VariableStoreHeader);
  while (IsValidVariableHeader (Variable, GetEndPointer (VariableStoreHeader))) {
    NextVariable = GetNextVariablePtr (Variable);
    if (Variable != UpdatingVariable && Variable != UpdatingInDeletedTransition && Variable->State == (VAR_IN_DELETED_TRANSITION & VAR_ADDED)) {

      //
      // Buffer has cached all ADDED variable.
      // Per IN_DELETED variable, we have to guarantee that
      // no ADDED one in previous buffer.
      //

      FoundAdded = FALSE;
      AddedVariable = GetStartPointer ((VARIABLE_STORE_HEADER *) ValidBuffer);
      while (IsValidVariableHeader (AddedVariable, GetEndPointer ((VARIABLE_STORE_HEADER *) ValidBuffer))) {
        NextAddedVariable = GetNextVariablePtr (AddedVariable);
        NameSize = NameSizeOfVariable (AddedVariable);
        if (CompareGuid (GetVendorGuidPtr (AddedVariable), GetVendorGuidPtr (Variable)) &&
            NameSize == NameSizeOfVariable (Variable)
          ) {
          Point0 = (VOID *) GetVariableNamePtr (AddedVariable);
          Point1 = (VOID *) GetVariableNamePtr (Variable);
          if (CompareMem (Point0, Point1, NameSize) == 0) {
            FoundAdded = TRUE;
            break;
          }
        }
        AddedVariable = NextAddedVariable;
      }
      if (!FoundAdded) {
        //
        // Promote VAR_IN_DELETED_TRANSITION to VAR_ADDED.
        //
        VariableSize = (UINTN) NextVariable - (UINTN) Variable;
        CopyMem (CurrPtr, (UINT8 *) Variable, VariableSize);
        ((VARIABLE_HEADER *) CurrPtr)->State = VAR_ADDED;
        CurrPtr += VariableSize;
        if ((!IsVolatile) && ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD)) {
          HwErrVariableTotalSize += VariableSize;
        } else if ((!IsVolatile) && ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != EFI_VARIABLE_HARDWARE_ERROR_RECORD)) {
          CommonVariableTotalSize += VariableSize;
          if (IsUserVariable (Variable)) {
            CommonUserVariableTotalSize += VariableSize;
          }
        }
      }
    }

    Variable = NextVariable;
  }

  if (!IsVolatile && !AtRuntime ()) {
    //
    // Perform Garbage Collection operation on the EDKII_VARIABLE_STORAGE_PROTOCOLs
    //
    for ( Index = 0;
          Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
          Index++) {
      VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
      if (VariableStorageProtocol->WriteServiceIsReady (VariableStorageProtocol)) {
        Status = VariableStorageProtocol->GarbageCollect (VariableStorageProtocol);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Error in Variable Storage Garbage Collection: %r\n", Status));
          goto Done;
        }
      }
    }
  }

  //
  // Install the new variable if it is not NULL.
  //
  if (NewVariable != NULL) {
    if (((UINTN) CurrPtr - (UINTN) ValidBuffer) + NewVariableSize > VariableStoreHeader->Size) {
      //
      // No enough space to store the new variable.
      //
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }
    if (!IsVolatile) {
      if ((NewVariable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
        HwErrVariableTotalSize += NewVariableSize;
      } else if ((NewVariable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
        CommonVariableTotalSize += NewVariableSize;
        if (IsUserVariable (NewVariable)) {
          CommonUserVariableTotalSize += NewVariableSize;
        }
      }
      if ((HwErrVariableTotalSize > PcdGet32 (PcdHwErrStorageSize)) ||
          (CommonVariableTotalSize > mVariableModuleGlobal->CommonVariableSpace) ||
          (CommonUserVariableTotalSize > mVariableModuleGlobal->CommonMaxUserVariableSpace)) {
        //
        // No enough space to store the new variable by NV or NV+HR attribute.
        //
        Status = EFI_OUT_OF_RESOURCES;
        goto Done;
      }
      //
      // Update the data in NV
      //
      if (!mNvVariableEmulationMode) {
        VariableStorageProtocol = NULL;
        Status = GetVariableStorageProtocol (
                  GetVariableNamePtr (NewVariable),
                  GetVendorGuidPtr (NewVariable),
                  &VariableStorageProtocol
                  );
        if (!EFI_ERROR (Status) && VariableStorageProtocol != NULL) {
          if (VariableStorageProtocol->WriteServiceIsReady (VariableStorageProtocol)) {
            if (mVariableModuleGlobal->VariableGlobal.AuthFormat) {
              AuthVariable = (AUTHENTICATED_VARIABLE_HEADER *) NewVariable;
              Status = VariableStorageProtocol->SetVariable (
                                                  VariableStorageProtocol,
                                                  AtRuntime (),
                                                  mFromSmm,
                                                  GetVariableNamePtr (NewVariable),
                                                  GetVendorGuidPtr (NewVariable),
                                                  NewVariable->Attributes,
                                                  DataSizeOfVariable (NewVariable),
                                                  GetVariableDataPtr (NewVariable),
                                                  AuthVariable->PubKeyIndex,
                                                  ReadUnaligned64 (&(AuthVariable->MonotonicCount)),
                                                  &AuthVariable->TimeStamp,
                                                  CommandInProgress
                                                  );
            } else {
              Status = VariableStorageProtocol->SetVariable (
                                                  VariableStorageProtocol,
                                                  AtRuntime (),
                                                  mFromSmm,
                                                  GetVariableNamePtr (NewVariable),
                                                  GetVendorGuidPtr (NewVariable),
                                                  NewVariable->Attributes,
                                                  DataSizeOfVariable (NewVariable),
                                                  GetVariableDataPtr (NewVariable),
                                                  0,
                                                  0,
                                                  NULL,
                                                  CommandInProgress
                                                  );
            }
            if (EFI_ERROR (Status)) {
              goto Done;
            } else if (*CommandInProgress) {
              VariableStorageProtocol->GetId (VariableStorageProtocol, InProgressInstanceGuid);
            }
          } else {
            Status = EFI_NOT_AVAILABLE_YET;
            goto Done;
          }
        } else {
          if (!EFI_ERROR (Status)) {
            Status = EFI_NOT_FOUND;
          }
          goto Done;
        }
      }
    }

    CopyMem (CurrPtr, (UINT8 *) NewVariable, NewVariableSize);
    ((VARIABLE_HEADER *) CurrPtr)->State = VAR_ADDED;
    if (UpdatingVariable != NULL) {
      UpdatingPtrTrack->CurrPtr = (VARIABLE_HEADER *)((UINTN)UpdatingPtrTrack->StartPtr + ((UINTN)CurrPtr - (UINTN)GetStartPointer ((VARIABLE_STORE_HEADER *) ValidBuffer)));
      UpdatingPtrTrack->InDeletedTransitionPtr = NULL;
    }
    CurrPtr += NewVariableSize;
  }

  *LastVariableOffset = (UINTN) (CurrPtr - ValidBuffer);
  //
  // Copy the reclaimed variable store back to the original buffer.
  //
  SetMem ((UINT8 *) (UINTN) VariableBase, VariableStoreHeader->Size, 0xff);
  CopyMem ((UINT8 *) (UINTN) VariableBase, ValidBuffer, (UINTN) (CurrPtr - ValidBuffer));
  if (!IsVolatile) {
    //
    // If non-volatile variable store, update NV storage usage
    //
    mVariableModuleGlobal->HwErrVariableTotalSize       = HwErrVariableTotalSize;
    mVariableModuleGlobal->CommonVariableTotalSize      = CommonVariableTotalSize;
    mVariableModuleGlobal->CommonUserVariableTotalSize  = CommonUserVariableTotalSize;

    Status =  SynchronizeRuntimeVariableCache (
                &mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache,
                0,
                (UINTN) (CurrPtr - ValidBuffer)
                );
    ASSERT_EFI_ERROR (Status);
  } else {
    Status =  SynchronizeRuntimeVariableCache (
                &mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache,
                0,
                (UINTN) (CurrPtr - ValidBuffer)
                );
    ASSERT_EFI_ERROR (Status);
  }

Done:
  return Status;
}

/**
  Get index from supported language codes according to language string.

  This code is used to get corresponding index in supported language codes. It can handle
  RFC4646 and ISO639 language tags.
  In ISO639 language tags, take 3-characters as a delimitation to find matched string and calculate the index.
  In RFC4646 language tags, take semicolon as a delimitation to find matched string and calculate the index.

  For example:
    SupportedLang  = "engfraengfra"
    Lang           = "eng"
    Iso639Language = TRUE
  The return value is "0".
  Another example:
    SupportedLang  = "en;fr;en-US;fr-FR"
    Lang           = "fr-FR"
    Iso639Language = FALSE
  The return value is "3".

  @param  SupportedLang               Platform supported language codes.
  @param  Lang                        Configured language.
  @param  Iso639Language              A bool value to signify if the handler is operated on ISO639 or RFC4646.

  @retval The index of language in the language codes.

**/
UINTN
GetIndexFromSupportedLangCodes (
  IN  CHAR8            *SupportedLang,
  IN  CHAR8            *Lang,
  IN  BOOLEAN          Iso639Language
  )
{
  UINTN    Index;
  UINTN    CompareLength;
  UINTN    LanguageLength;

  if (Iso639Language) {
    CompareLength = ISO_639_2_ENTRY_SIZE;
    for (Index = 0; Index < AsciiStrLen (SupportedLang); Index += CompareLength) {
      if (AsciiStrnCmp (Lang, SupportedLang + Index, CompareLength) == 0) {
        //
        // Successfully find the index of Lang string in SupportedLang string.
        //
        Index = Index / CompareLength;
        return Index;
      }
    }
    ASSERT (FALSE);
    return 0;
  } else {
    //
    // Compare RFC4646 language code
    //
    Index = 0;
    for (LanguageLength = 0; Lang[LanguageLength] != '\0'; LanguageLength++);

    for (Index = 0; *SupportedLang != '\0'; Index++, SupportedLang += CompareLength) {
      //
      // Skip ';' characters in SupportedLang
      //
      for (; *SupportedLang != '\0' && *SupportedLang == ';'; SupportedLang++);
      //
      // Determine the length of the next language code in SupportedLang
      //
      for (CompareLength = 0; SupportedLang[CompareLength] != '\0' && SupportedLang[CompareLength] != ';'; CompareLength++);

      if ((CompareLength == LanguageLength) &&
          (AsciiStrnCmp (Lang, SupportedLang, CompareLength) == 0)) {
        //
        // Successfully find the index of Lang string in SupportedLang string.
        //
        return Index;
      }
    }
    ASSERT (FALSE);
    return 0;
  }
}

/**
  Get language string from supported language codes according to index.

  This code is used to get corresponding language strings in supported language codes. It can handle
  RFC4646 and ISO639 language tags.
  In ISO639 language tags, take 3-characters as a delimitation. Find language string according to the index.
  In RFC4646 language tags, take semicolon as a delimitation. Find language string according to the index.

  For example:
    SupportedLang  = "engfraengfra"
    Index          = "1"
    Iso639Language = TRUE
  The return value is "fra".
  Another example:
    SupportedLang  = "en;fr;en-US;fr-FR"
    Index          = "1"
    Iso639Language = FALSE
  The return value is "fr".

  @param  SupportedLang               Platform supported language codes.
  @param  Index                       The index in supported language codes.
  @param  Iso639Language              A bool value to signify if the handler is operated on ISO639 or RFC4646.

  @retval The language string in the language codes.

**/
CHAR8 *
GetLangFromSupportedLangCodes (
  IN  CHAR8            *SupportedLang,
  IN  UINTN            Index,
  IN  BOOLEAN          Iso639Language
)
{
  UINTN    SubIndex;
  UINTN    CompareLength;
  CHAR8    *Supported;

  SubIndex  = 0;
  Supported = SupportedLang;
  if (Iso639Language) {
    //
    // According to the index of Lang string in SupportedLang string to get the language.
    // This code will be invoked in RUNTIME, therefore there is not a memory allocate/free operation.
    // In driver entry, it pre-allocates a runtime attribute memory to accommodate this string.
    //
    CompareLength = ISO_639_2_ENTRY_SIZE;
    mVariableModuleGlobal->Lang[CompareLength] = '\0';
    return CopyMem (mVariableModuleGlobal->Lang, SupportedLang + Index * CompareLength, CompareLength);

  } else {
    while (TRUE) {
      //
      // Take semicolon as delimitation, sequentially traverse supported language codes.
      //
      for (CompareLength = 0; *Supported != ';' && *Supported != '\0'; CompareLength++) {
        Supported++;
      }
      if ((*Supported == '\0') && (SubIndex != Index)) {
        //
        // Have completed the traverse, but not find corrsponding string.
        // This case is not allowed to happen.
        //
        ASSERT(FALSE);
        return NULL;
      }
      if (SubIndex == Index) {
        //
        // According to the index of Lang string in SupportedLang string to get the language.
        // As this code will be invoked in RUNTIME, therefore there is not memory allocate/free operation.
        // In driver entry, it pre-allocates a runtime attribute memory to accommodate this string.
        //
        mVariableModuleGlobal->PlatformLang[CompareLength] = '\0';
        return CopyMem (mVariableModuleGlobal->PlatformLang, Supported - CompareLength, CompareLength);
      }
      SubIndex++;

      //
      // Skip ';' characters in Supported
      //
      for (; *Supported != '\0' && *Supported == ';'; Supported++);
    }
  }
}

/**
  Returns a pointer to an allocated buffer that contains the best matching language
  from a set of supported languages.

  This function supports both ISO 639-2 and RFC 4646 language codes, but language
  code types may not be mixed in a single call to this function. This function
  supports a variable argument list that allows the caller to pass in a prioritized
  list of language codes to test against all the language codes in SupportedLanguages.

  If SupportedLanguages is NULL, then ASSERT().

  @param[in]  SupportedLanguages  A pointer to a Null-terminated ASCII string that
                                  contains a set of language codes in the format
                                  specified by Iso639Language.
  @param[in]  Iso639Language      If not zero, then all language codes are assumed to be
                                  in ISO 639-2 format.  If zero, then all language
                                  codes are assumed to be in RFC 4646 language format
  @param[in]  ...                 A variable argument list that contains pointers to
                                  Null-terminated ASCII strings that contain one or more
                                  language codes in the format specified by Iso639Language.
                                  The first language code from each of these language
                                  code lists is used to determine if it is an exact or
                                  close match to any of the language codes in
                                  SupportedLanguages.  Close matches only apply to RFC 4646
                                  language codes, and the matching algorithm from RFC 4647
                                  is used to determine if a close match is present.  If
                                  an exact or close match is found, then the matching
                                  language code from SupportedLanguages is returned.  If
                                  no matches are found, then the next variable argument
                                  parameter is evaluated.  The variable argument list
                                  is terminated by a NULL.

  @retval NULL   The best matching language could not be found in SupportedLanguages.
  @retval NULL   There are not enough resources available to return the best matching
                 language.
  @retval Other  A pointer to a Null-terminated ASCII string that is the best matching
                 language in SupportedLanguages.

**/
CHAR8 *
EFIAPI
VariableGetBestLanguage (
  IN CONST CHAR8  *SupportedLanguages,
  IN UINTN        Iso639Language,
  ...
  )
{
  VA_LIST      Args;
  CHAR8        *Language;
  UINTN        CompareLength;
  UINTN        LanguageLength;
  CONST CHAR8  *Supported;
  CHAR8        *Buffer;

  if (SupportedLanguages == NULL) {
    return NULL;
  }

  VA_START (Args, Iso639Language);
  while ((Language = VA_ARG (Args, CHAR8 *)) != NULL) {
    //
    // Default to ISO 639-2 mode
    //
    CompareLength  = 3;
    LanguageLength = MIN (3, AsciiStrLen (Language));

    //
    // If in RFC 4646 mode, then determine the length of the first RFC 4646 language code in Language
    //
    if (Iso639Language == 0) {
      for (LanguageLength = 0; Language[LanguageLength] != 0 && Language[LanguageLength] != ';'; LanguageLength++);
    }

    //
    // Trim back the length of Language used until it is empty
    //
    while (LanguageLength > 0) {
      //
      // Loop through all language codes in SupportedLanguages
      //
      for (Supported = SupportedLanguages; *Supported != '\0'; Supported += CompareLength) {
        //
        // In RFC 4646 mode, then Loop through all language codes in SupportedLanguages
        //
        if (Iso639Language == 0) {
          //
          // Skip ';' characters in Supported
          //
          for (; *Supported != '\0' && *Supported == ';'; Supported++);
          //
          // Determine the length of the next language code in Supported
          //
          for (CompareLength = 0; Supported[CompareLength] != 0 && Supported[CompareLength] != ';'; CompareLength++);
          //
          // If Language is longer than the Supported, then skip to the next language
          //
          if (LanguageLength > CompareLength) {
            continue;
          }
        }
        //
        // See if the first LanguageLength characters in Supported match Language
        //
        if (AsciiStrnCmp (Supported, Language, LanguageLength) == 0) {
          VA_END (Args);

          Buffer = (Iso639Language != 0) ? mVariableModuleGlobal->Lang : mVariableModuleGlobal->PlatformLang;
          Buffer[CompareLength] = '\0';
          return CopyMem (Buffer, Supported, CompareLength);
        }
      }

      if (Iso639Language != 0) {
        //
        // If ISO 639 mode, then each language can only be tested once
        //
        LanguageLength = 0;
      } else {
        //
        // If RFC 4646 mode, then trim Language from the right to the next '-' character
        //
        for (LanguageLength--; LanguageLength > 0 && Language[LanguageLength] != '-'; LanguageLength--);
      }
    }
  }
  VA_END (Args);

  //
  // No matches were found
  //
  return NULL;
}

/**
  This function is to check if the remaining variable space is enough to set
  all Variables from argument list successfully. The purpose of the check
  is to keep the consistency of the Variables to be in variable storage.

  Note: Variables are assumed to be in same storage.
  The set sequence of Variables will be same with the sequence of VariableEntry from argument list,
  so follow the argument sequence to check the Variables.

  @param[in] Attributes         Variable attributes for Variable entries.
  @param[in] Marker             VA_LIST style variable argument list.
                                The variable argument list with type VARIABLE_ENTRY_CONSISTENCY *.
                                A NULL terminates the list. The VariableSize of
                                VARIABLE_ENTRY_CONSISTENCY is the variable data size as input.
                                It will be changed to variable total size as output.

  @retval TRUE                  Have enough variable space to set the Variables successfully.
  @retval FALSE                 No enough variable space to set the Variables successfully.

**/
BOOLEAN
EFIAPI
CheckRemainingSpaceForConsistencyInternal (
  IN UINT32                     Attributes,
  IN VA_LIST                    Marker
  )
{
  EFI_STATUS                    Status;
  VA_LIST                       Args;
  VARIABLE_ENTRY_CONSISTENCY    *VariableEntry;
  UINT64                        MaximumVariableStorageSize;
  UINT64                        RemainingVariableStorageSize;
  UINT64                        MaximumVariableSize;
  UINTN                         TotalNeededSize;
  UINTN                         OriginalVarSize;
  VARIABLE_STORE_HEADER         *VariableStoreHeader;
  VARIABLE_POINTER_TRACK        VariablePtrTrack;
  VARIABLE_HEADER               *NextVariable;
  UINTN                         VarNameSize;
  UINTN                         VarDataSize;

  //
  // Non-Volatile related.
  //
  VariableStoreHeader = mNvVariableCache;

  Status = VariableServiceQueryVariableInfoInternal (
             Attributes,
             &MaximumVariableStorageSize,
             &RemainingVariableStorageSize,
             &MaximumVariableSize
             );
  ASSERT_EFI_ERROR (Status);

  TotalNeededSize = 0;
  VA_COPY (Args, Marker);
  VariableEntry = VA_ARG (Args, VARIABLE_ENTRY_CONSISTENCY *);
  while (VariableEntry != NULL) {
    //
    // Calculate variable total size.
    //
    VarNameSize  = StrSize (VariableEntry->Name);
    VarNameSize += GET_PAD_SIZE (VarNameSize);
    VarDataSize  = VariableEntry->VariableSize;
    VarDataSize += GET_PAD_SIZE (VarDataSize);
    VariableEntry->VariableSize = HEADER_ALIGN (GetVariableHeaderSize () + VarNameSize + VarDataSize);

    TotalNeededSize += VariableEntry->VariableSize;
    VariableEntry = VA_ARG (Args, VARIABLE_ENTRY_CONSISTENCY *);
  }
  VA_END  (Args);

  if (RemainingVariableStorageSize >= TotalNeededSize) {
    //
    // Already have enough space.
    //
    return TRUE;
  } else if (AtRuntime ()) {
    //
    // At runtime, no reclaim.
    // The original variable space of Variables can't be reused.
    //
    return FALSE;
  }

  VA_COPY (Args, Marker);
  VariableEntry = VA_ARG (Args, VARIABLE_ENTRY_CONSISTENCY *);
  while (VariableEntry != NULL) {
    //
    // Check if Variable[Index] has been present and get its size.
    //
    OriginalVarSize = 0;
    VariablePtrTrack.StartPtr = GetStartPointer (VariableStoreHeader);
    VariablePtrTrack.EndPtr   = GetEndPointer   (VariableStoreHeader);
    Status = FindVariableEx (
               VariableEntry->Name,
               VariableEntry->Guid,
               FALSE,
               &VariablePtrTrack
               );
    if (!EFI_ERROR (Status)) {
      //
      // Get size of Variable[Index].
      //
      NextVariable = GetNextVariablePtr (VariablePtrTrack.CurrPtr);
      OriginalVarSize = (UINTN) NextVariable - (UINTN) VariablePtrTrack.CurrPtr;
      //
      // Add the original size of Variable[Index] to remaining variable storage size.
      //
      RemainingVariableStorageSize += OriginalVarSize;
    }
    if (VariableEntry->VariableSize > RemainingVariableStorageSize) {
      //
      // No enough space for Variable[Index].
      //
      VA_END (Args);
      return FALSE;
    }
    //
    // Sub the (new) size of Variable[Index] from remaining variable storage size.
    //
    RemainingVariableStorageSize -= VariableEntry->VariableSize;
    VariableEntry = VA_ARG (Args, VARIABLE_ENTRY_CONSISTENCY *);
  }
  VA_END  (Args);

  return TRUE;
}

/**
  This function is to check if the remaining variable space is enough to set
  all Variables from argument list successfully. The purpose of the check
  is to keep the consistency of the Variables to be in variable storage.

  Note: Variables are assumed to be in same storage.
  The set sequence of Variables will be same with the sequence of VariableEntry from argument list,
  so follow the argument sequence to check the Variables.

  @param[in] Attributes         Variable attributes for Variable entries.
  @param ...                    The variable argument list with type VARIABLE_ENTRY_CONSISTENCY *.
                                A NULL terminates the list. The VariableSize of
                                VARIABLE_ENTRY_CONSISTENCY is the variable data size as input.
                                It will be changed to variable total size as output.

  @retval TRUE                  Have enough variable space to set the Variables successfully.
  @retval FALSE                 No enough variable space to set the Variables successfully.

**/
BOOLEAN
EFIAPI
CheckRemainingSpaceForConsistency (
  IN UINT32                     Attributes,
  ...
  )
{
  VA_LIST Marker;
  BOOLEAN Return;

  VA_START (Marker, Attributes);

  Return = CheckRemainingSpaceForConsistencyInternal (Attributes, Marker);

  VA_END (Marker);

  return Return;
}

/**
  Hook the operations in PlatformLangCodes, LangCodes, PlatformLang and Lang.

  When setting Lang/LangCodes, simultaneously update PlatformLang/PlatformLangCodes.

  According to UEFI spec, PlatformLangCodes/LangCodes are only set once in firmware initialization,
  and are read-only. Therefore, in variable driver, only store the original value for other use.

  @param[in] VariableName       Name of variable.

  @param[in] Data               Variable data.

  @param[in] DataSize           Size of data. 0 means delete.

  @retval EFI_SUCCESS           The update operation is successful or ignored.
  @retval EFI_WRITE_PROTECTED   Update PlatformLangCodes/LangCodes at runtime.
  @retval EFI_OUT_OF_RESOURCES  No enough variable space to do the update operation.
  @retval Others                Other errors happened during the update operation.

**/
EFI_STATUS
AutoUpdateLangVariable (
  IN  CHAR16             *VariableName,
  IN  VOID               *Data,
  IN  UINTN              DataSize
  )
{
  EFI_STATUS                  Status;
  UINTN                       Index;
  UINT32                      Attributes;
  VARIABLE_POINTER_TRACK      Variable;
  BOOLEAN                     SetLanguageCodes;
  VARIABLE_ENTRY_CONSISTENCY  VariableEntry[2];
  CHAR8                       *BestLang;
  CHAR8                       *BestPlatformLang;
  BOOLEAN                     CommandInProgress;
  EFI_GUID                    InProgressInstanceGuid;

  BestLang         = NULL;
  BestPlatformLang = NULL;

  //
  // Don't do updates for delete operation
  //
  if (DataSize == 0) {
    return EFI_SUCCESS;
  }

  SetLanguageCodes = FALSE;

  if (StrCmp (VariableName, EFI_PLATFORM_LANG_CODES_VARIABLE_NAME) == 0) {
    //
    // PlatformLangCodes is a volatile variable, so it can not be updated at runtime.
    //
    if (AtRuntime ()) {
      return EFI_WRITE_PROTECTED;
    }

    SetLanguageCodes = TRUE;

    //
    // According to UEFI spec, PlatformLangCodes is only set once in firmware initialization, and is read-only
    // Therefore, in variable driver, only store the original value for other use.
    //
    if (mVariableModuleGlobal->PlatformLangCodes != NULL) {
      FreePool (mVariableModuleGlobal->PlatformLangCodes);
    }
    mVariableModuleGlobal->PlatformLangCodes = AllocateRuntimeCopyPool (DataSize, Data);
    ASSERT (mVariableModuleGlobal->PlatformLangCodes != NULL);

    //
    // PlatformLang holds a single language from PlatformLangCodes,
    // so the size of PlatformLangCodes is enough for the PlatformLang.
    //
    if (mVariableModuleGlobal->PlatformLang != NULL) {
      FreePool (mVariableModuleGlobal->PlatformLang);
    }
    mVariableModuleGlobal->PlatformLang = AllocateRuntimePool (DataSize);
    ASSERT (mVariableModuleGlobal->PlatformLang != NULL);

  } else if (StrCmp (VariableName, EFI_LANG_CODES_VARIABLE_NAME) == 0) {
    //
    // LangCodes is a volatile variable, so it can not be updated at runtime.
    //
    if (AtRuntime ()) {
      return EFI_WRITE_PROTECTED;
    }

    SetLanguageCodes = TRUE;

    //
    // According to UEFI spec, LangCodes is only set once in firmware initialization, and is read-only
    // Therefore, in variable driver, only store the original value for other use.
    //
    if (mVariableModuleGlobal->LangCodes != NULL) {
      FreePool (mVariableModuleGlobal->LangCodes);
    }
    mVariableModuleGlobal->LangCodes = AllocateRuntimeCopyPool (DataSize, Data);
    ASSERT (mVariableModuleGlobal->LangCodes != NULL);
  }

  if (SetLanguageCodes
      && (mVariableModuleGlobal->PlatformLangCodes != NULL)
      && (mVariableModuleGlobal->LangCodes != NULL)) {
    //
    // Update Lang if PlatformLang is already set
    // Update PlatformLang if Lang is already set
    //
    Status = FindVariable (
               EFI_PLATFORM_LANG_VARIABLE_NAME,
               &gEfiGlobalVariableGuid,
               &Variable,
               &mVariableModuleGlobal->VariableGlobal,
               FALSE,
               &CommandInProgress,
               &InProgressInstanceGuid
               );
    if (!EFI_ERROR (Status)) {
      //
      // Update Lang
      //
      VariableName = EFI_PLATFORM_LANG_VARIABLE_NAME;
      Data         = GetVariableDataPtr (Variable.CurrPtr);
      DataSize     = DataSizeOfVariable (Variable.CurrPtr);
    } else {
      Status = FindVariable (
                 EFI_LANG_VARIABLE_NAME,
                 &gEfiGlobalVariableGuid,
                 &Variable,
                 &mVariableModuleGlobal->VariableGlobal,
                 FALSE,
                 &CommandInProgress,
                 &InProgressInstanceGuid
                 );
      if (!EFI_ERROR (Status)) {
        //
        // Update PlatformLang
        //
        VariableName = EFI_LANG_VARIABLE_NAME;
        Data         = GetVariableDataPtr (Variable.CurrPtr);
        DataSize     = DataSizeOfVariable (Variable.CurrPtr);
      } else {
        //
        // Neither PlatformLang nor Lang is set, directly return
        //
        return EFI_SUCCESS;
      }
    }
  }

  Status = EFI_SUCCESS;

  //
  // According to UEFI spec, "Lang" and "PlatformLang" is NV|BS|RT attributions.
  //
  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;

  if (StrCmp (VariableName, EFI_PLATFORM_LANG_VARIABLE_NAME) == 0) {
    //
    // Update Lang when PlatformLangCodes/LangCodes were set.
    //
    if ((mVariableModuleGlobal->PlatformLangCodes != NULL) && (mVariableModuleGlobal->LangCodes != NULL)) {
      //
      // When setting PlatformLang, firstly get most matched language string from supported language codes.
      //
      BestPlatformLang = VariableGetBestLanguage (mVariableModuleGlobal->PlatformLangCodes, FALSE, Data, NULL);
      if (BestPlatformLang != NULL) {
        //
        // Get the corresponding index in language codes.
        //
        Index = GetIndexFromSupportedLangCodes (mVariableModuleGlobal->PlatformLangCodes, BestPlatformLang, FALSE);

        //
        // Get the corresponding ISO639 language tag according to RFC4646 language tag.
        //
        BestLang = GetLangFromSupportedLangCodes (mVariableModuleGlobal->LangCodes, Index, TRUE);

        //
        // Check the variable space for both Lang and PlatformLang variable.
        //
        VariableEntry[0].VariableSize = ISO_639_2_ENTRY_SIZE + 1;
        VariableEntry[0].Guid = &gEfiGlobalVariableGuid;
        VariableEntry[0].Name = EFI_LANG_VARIABLE_NAME;

        VariableEntry[1].VariableSize = AsciiStrSize (BestPlatformLang);
        VariableEntry[1].Guid = &gEfiGlobalVariableGuid;
        VariableEntry[1].Name = EFI_PLATFORM_LANG_VARIABLE_NAME;
        if (!CheckRemainingSpaceForConsistency (VARIABLE_ATTRIBUTE_NV_BS_RT, &VariableEntry[0], &VariableEntry[1], NULL)) {
          //
          // No enough variable space to set both Lang and PlatformLang successfully.
          //
          Status = EFI_OUT_OF_RESOURCES;
        } else {
          //
          // Successfully convert PlatformLang to Lang, and set the BestLang value into Lang variable simultaneously.
          //
          FindVariable (
            EFI_LANG_VARIABLE_NAME,
            &gEfiGlobalVariableGuid,
            &Variable,
            &mVariableModuleGlobal->VariableGlobal,
            FALSE,
            &CommandInProgress,
            &InProgressInstanceGuid
            );

          Status = UpdateVariable (
                     EFI_LANG_VARIABLE_NAME,
                     &gEfiGlobalVariableGuid,
                     BestLang,
                     ISO_639_2_ENTRY_SIZE + 1,
                     Attributes,
                     0,
                     0,
                     &Variable,
                     NULL
                     );
        }

        DEBUG ((EFI_D_INFO, "  Variable Driver: Auto Update PlatformLang, PlatformLang:%a, Lang:%a Status: %r\n", BestPlatformLang, BestLang, Status));
      }
    }

  } else if (StrCmp (VariableName, EFI_LANG_VARIABLE_NAME) == 0) {
    //
    // Update PlatformLang when PlatformLangCodes/LangCodes were set.
    //
    if ((mVariableModuleGlobal->PlatformLangCodes != NULL) && (mVariableModuleGlobal->LangCodes != NULL)) {
      //
      // When setting Lang, firstly get most matched language string from supported language codes.
      //
      BestLang = VariableGetBestLanguage (mVariableModuleGlobal->LangCodes, TRUE, Data, NULL);
      if (BestLang != NULL) {
        //
        // Get the corresponding index in language codes.
        //
        Index = GetIndexFromSupportedLangCodes (mVariableModuleGlobal->LangCodes, BestLang, TRUE);

        //
        // Get the corresponding RFC4646 language tag according to ISO639 language tag.
        //
        BestPlatformLang = GetLangFromSupportedLangCodes (mVariableModuleGlobal->PlatformLangCodes, Index, FALSE);

        if (BestPlatformLang == NULL) {
          ASSERT (BestPlatformLang != NULL);
          return EFI_OUT_OF_RESOURCES;
        }

        //
        // Check the variable space for both PlatformLang and Lang variable.
        //
        VariableEntry[0].VariableSize = AsciiStrSize (BestPlatformLang);
        VariableEntry[0].Guid = &gEfiGlobalVariableGuid;
        VariableEntry[0].Name = EFI_PLATFORM_LANG_VARIABLE_NAME;

        VariableEntry[1].VariableSize = ISO_639_2_ENTRY_SIZE + 1;
        VariableEntry[1].Guid = &gEfiGlobalVariableGuid;
        VariableEntry[1].Name = EFI_LANG_VARIABLE_NAME;
        if (!CheckRemainingSpaceForConsistency (VARIABLE_ATTRIBUTE_NV_BS_RT, &VariableEntry[0], &VariableEntry[1], NULL)) {
          //
          // No enough variable space to set both PlatformLang and Lang successfully.
          //
          Status = EFI_OUT_OF_RESOURCES;
        } else {
          //
          // Successfully convert Lang to PlatformLang, and set the BestPlatformLang value into PlatformLang variable simultaneously.
          //
          FindVariable (
            EFI_PLATFORM_LANG_VARIABLE_NAME,
            &gEfiGlobalVariableGuid,
            &Variable,
            &mVariableModuleGlobal->VariableGlobal,
            FALSE,
            &CommandInProgress,
            &InProgressInstanceGuid
            );

          Status = UpdateVariable (
                     EFI_PLATFORM_LANG_VARIABLE_NAME,
                     &gEfiGlobalVariableGuid,
                     BestPlatformLang,
                     AsciiStrSize (BestPlatformLang),
                     Attributes,
                     0,
                     0,
                     &Variable,
                     NULL
                     );
        }

        DEBUG ((EFI_D_INFO, "  Variable Driver: Auto Update Lang, Lang:%a, PlatformLang:%a Status: %r\n", BestLang, BestPlatformLang, Status));
      }
    }
  }

  if (SetLanguageCodes) {
    //
    // Continue to set PlatformLangCodes or LangCodes.
    //
    return EFI_SUCCESS;
  } else {
    return Status;
  }
}

/**
  Update the variable region with Variable information. If EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS is set,
  index of associated public key is needed.

  @param[in]  VariableName       Name of variable.
  @param[in]  VendorGuid         Guid of variable.
  @param[in]  Data               Variable data.
  @param[in]  DataSize           Size of data. 0 means delete.
  @param[in]  Attributes         Attributes of the variable.
  @param[in]  KeyIndex           Index of associated public key.
  @param[in]  MonotonicCount     Value of associated monotonic count.
  @param[in, out] CacheVariable The variable information which is used to keep track of variable usage.
  @param[in]  TimeStamp          Value of associated TimeStamp.
  @param[in]  OnlyUpdateNvCache  TRUE if only the NV cache should be written to, not the EDKII_VARIABLE_STORAGE_PROTOCOLs
  @param[out] CommandInProgress  TRUE if the command requires asyncronous I/O and has not completed yet.
                                 If this parameter is TRUE, then CacheVariable will not be updated and will
                                 not contain valid data.  Asyncronous I/O should only be required during
                                 OS runtime phase, this return value will be FALSE during all Pre-OS stages.
                                 If CommandInProgress is returned TRUE, then this function will return EFI_SUCCESS

  @retval EFI_SUCCESS           The update operation is success.
  @retval EFI_OUT_OF_RESOURCES  Variable region is full, can not write other data into this region.

**/
EFI_STATUS
UpdateVariableInternal (
  IN      CHAR16                      *VariableName,
  IN      EFI_GUID                    *VendorGuid,
  IN      VOID                        *Data,
  IN      UINTN                       DataSize,
  IN      UINT32                      Attributes      OPTIONAL,
  IN      UINT32                      KeyIndex        OPTIONAL,
  IN      UINT64                      MonotonicCount  OPTIONAL,
  IN OUT  VARIABLE_POINTER_TRACK      *CacheVariable,
  IN      EFI_TIME                    *TimeStamp      OPTIONAL,
  IN      BOOLEAN                     OnlyUpdateNvCache,
  OUT     BOOLEAN                     *CommandInProgress,
  OUT     EFI_GUID                    *InProgressInstanceGuid
  )
{
  EFI_STATUS                          Status;
  EFI_STATUS                          Status2;
  VARIABLE_HEADER                     *NextVariable;
  UINTN                               ScratchSize;
  UINTN                               MaxDataSize;
  UINTN                               VarNameOffset;
  UINTN                               VarDataOffset;
  UINTN                               VarNameSize;
  UINTN                               VarSize;
  BOOLEAN                             Volatile;
  UINT8                               State;
  VARIABLE_POINTER_TRACK              *Variable;
  VARIABLE_HEADER                     *LastVariable;
  VARIABLE_HEADER                     *OldVariable;
  VARIABLE_RUNTIME_CACHE              *VolatileCacheInstance;
  UINTN                               OldVariableSize;
  UINTN                               CacheOffset;
  UINT8                               *BufferForMerge;
  UINTN                               MergedBufSize;
  BOOLEAN                             DataReady;
  UINTN                               DataOffset;
  BOOLEAN                             IsCommonVariable;
  BOOLEAN                             IsCommonUserVariable;
  AUTHENTICATED_VARIABLE_HEADER       *AuthVariable;
  EFI_PHYSICAL_ADDRESS                HobVariableBase;
  EDKII_VARIABLE_STORAGE_PROTOCOL     *VariableStorageProtocol;

  *CommandInProgress = FALSE;
  if (!mVariableModuleGlobal->WriteServiceReady && !OnlyUpdateNvCache) {
    //
    // NV Variable writes are not ready, so the EFI_VARIABLE_WRITE_ARCH_PROTOCOL is not installed.
    //
    if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
      //
      // Trying to update NV variable prior to the installation of EFI_VARIABLE_WRITE_ARCH_PROTOCOL
      //
      DEBUG ((EFI_D_ERROR, "  Variable Driver: Update NV variable before EFI_VARIABLE_WRITE_ARCH_PROTOCOL ready - %r\n", EFI_NOT_AVAILABLE_YET));
      return EFI_NOT_AVAILABLE_YET;
    } else if ((Attributes & VARIABLE_ATTRIBUTE_AT_AW) != 0) {
      //
      // Trying to update volatile authenticated variable prior to the installation of EFI_VARIABLE_WRITE_ARCH_PROTOCOL
      // The authenticated variable perhaps is not initialized, just return here.
      //
      DEBUG ((EFI_D_ERROR, "  Variable Driver: Update AUTH variable before EFI_VARIABLE_WRITE_ARCH_PROTOCOL ready - %r\n", EFI_NOT_AVAILABLE_YET));
      return EFI_NOT_AVAILABLE_YET;
    }
  }

  DEBUG ((DEBUG_INFO, "+-+-> Variable Driver: UpdateVariable.\n  Variable Name: %s.\n  Guid:  %g.\n", VariableName, VendorGuid));

  //
  // Check if CacheVariable points to the variable in variable HOB.
  // If yes, let CacheVariable points to the variable in NV variable cache.
  //
  if ((CacheVariable->CurrPtr != NULL) &&
      (mVariableModuleGlobal->VariableGlobal.HobVariableBase != 0) &&
      (CacheVariable->StartPtr == GetStartPointer ((VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.HobVariableBase))
     ) {
    HobVariableBase                                       = mVariableModuleGlobal->VariableGlobal.HobVariableBase;
    mVariableModuleGlobal->VariableGlobal.HobVariableBase = 0;
    Status = FindVariable (
               VariableName,
               VendorGuid,
               CacheVariable,
               &mVariableModuleGlobal->VariableGlobal,
               FALSE,
               CommandInProgress,
               InProgressInstanceGuid
               );
    mVariableModuleGlobal->VariableGlobal.HobVariableBase = HobVariableBase;
    //
    // The existing variable should be loaded in to NV cache at this point
    //
    ASSERT (!(*CommandInProgress));
    if (*CommandInProgress) {
      return EFI_OUT_OF_RESOURCES;
    }
    if (CacheVariable->CurrPtr == NULL || EFI_ERROR (Status)) {
      //
      // There is no matched variable in NV variable cache.
      //
      if ((((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0) && (DataSize == 0)) || (Attributes == 0)) {
        //
        // It is to delete variable,
        // go to delete this variable in variable HOB and
        // try to flush other variables from HOB to storage.
        //
        UpdateVariableInfo (VariableName, VendorGuid, FALSE, FALSE, FALSE, TRUE, FALSE);
        FlushHobVariableToStorage (VariableName, VendorGuid, NULL);
        return EFI_SUCCESS;
      }
    }
  }

  Variable = CacheVariable;
  Variable->EndPtr   = (VARIABLE_HEADER *) ((UINTN) Variable->StartPtr + ((UINTN) CacheVariable->EndPtr - (UINTN) CacheVariable->StartPtr));

  //
  // Tricky part: Use scratch data area at the end of volatile variable store
  // as a temporary storage.
  //
  NextVariable = GetEndPointer ((VARIABLE_STORE_HEADER *) ((UINTN) mVariableModuleGlobal->VariableGlobal.VolatileVariableBase));
  ScratchSize = mVariableModuleGlobal->ScratchBufferSize;
  SetMem (NextVariable, ScratchSize, 0xff);
  DataReady = FALSE;

  if (Variable->CurrPtr != NULL) {
    DEBUG ((DEBUG_INFO, "  Variable Driver: Updating an existing variable (found in the cache).\n"));
    //
    // Update/Delete existing variable.
    //
    if (AtRuntime ()) {
      //
      // If AtRuntime and the variable is Volatile and Runtime Access,
      // the volatile is ReadOnly, and SetVariable should be aborted and
      // return EFI_WRITE_PROTECTED.
      //
      if (Variable->Volatile) {
        Status = EFI_WRITE_PROTECTED;
        goto Done;
      }
      //
      // Only variable that have NV attributes can be updated/deleted in Runtime.
      //
      if ((CacheVariable->CurrPtr->Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) {
        Status = EFI_INVALID_PARAMETER;
        goto Done;
      }

      //
      // Only variable that have RT attributes can be updated/deleted in Runtime.
      //
      if ((CacheVariable->CurrPtr->Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0) {
        Status = EFI_INVALID_PARAMETER;
        goto Done;
      }
    }

    //
    // Special handling for VarErrorFlag
    //
    if (CompareGuid (VendorGuid, &gEdkiiVarErrorFlagGuid) &&
        (StrCmp (VariableName, VAR_ERROR_FLAG_NAME) == 0) &&
        (DataSize == sizeof (VAR_ERROR_FLAG)) && !OnlyUpdateNvCache) {
      RecordVarErrorFlag (
        *((VAR_ERROR_FLAG *) Data),
        VariableName,
        VendorGuid,
        Attributes,
        DataSize,
        CommandInProgress,
        InProgressInstanceGuid
        );
      return EFI_SUCCESS;
    }

    //
    // Setting a data variable with no access, or zero DataSize attributes
    // causes it to be deleted.
    // When the EFI_VARIABLE_APPEND_WRITE attribute is set, DataSize of zero will
    // not delete the variable.
    //
    if ((((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0) && (DataSize == 0)) ||
         ((Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)) == 0)) {
      DEBUG ((DEBUG_INFO, "  Variable Driver: Variable is being deleted.\n"));
      if (Variable->InDeletedTransitionPtr != NULL) {
        //
        // Both ADDED and IN_DELETED_TRANSITION variable are present,
        // set IN_DELETED_TRANSITION one to DELETED state first.
        //
        ASSERT (CacheVariable->InDeletedTransitionPtr != NULL);
        CacheVariable->InDeletedTransitionPtr->State &= VAR_DELETED;
      }
      Variable->CurrPtr->State &= VAR_DELETED;
      Status                    = EFI_SUCCESS;
      if (!Variable->Volatile && !OnlyUpdateNvCache) {
        DEBUG ((DEBUG_INFO, "  Variable Driver: Variable is being deleted from NV storage.\n"));
        //
        // Delete the variable from the NV storage
        //
        Status = GetVariableStorageProtocol (
                  VariableName,
                  VendorGuid,
                  &VariableStorageProtocol
                  );
        if (!EFI_ERROR (Status)) {
          if (VariableStorageProtocol == NULL) {
            ASSERT (VariableStorageProtocol != NULL);
            return EFI_NOT_FOUND;
          }
          Status = VariableStorageProtocol->SetVariable (
                                                VariableStorageProtocol,
                                                AtRuntime (),
                                                mFromSmm,
                                                VariableName,
                                                VendorGuid,
                                                Attributes,
                                                0,
                                                NULL,
                                                0,
                                                0,
                                                TimeStamp,
                                                CommandInProgress
                                                );
          DEBUG ((DEBUG_INFO, "  Variable Driver: Value returned from storage protocol = %r.\n", Status));
          if (*CommandInProgress) {
            DEBUG ((DEBUG_INFO, "  Variable Driver: SetVariable returned CommandInProgress\n", Status));
            VariableStorageProtocol->GetId (VariableStorageProtocol, InProgressInstanceGuid);
          }
        }
        OldVariable = GetNextVariablePtr (Variable->CurrPtr);
        OldVariableSize = (UINTN) OldVariable - (UINTN) Variable->CurrPtr;
        State = CacheVariable->CurrPtr->State;
        if ((Variable->CurrPtr->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
          mVariableModuleGlobal->HwErrVariableTotalSize -= OldVariableSize;
        } else if ((Variable->CurrPtr->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
          mVariableModuleGlobal->CommonVariableTotalSize -= OldVariableSize;
          if (IsUserVariable (Variable->CurrPtr)) {
            mVariableModuleGlobal->CommonUserVariableTotalSize -= OldVariableSize;
          }
        }
      }
      if (!EFI_ERROR (Status)) {
        UpdateVariableInfo (VariableName, VendorGuid, Variable->Volatile, FALSE, FALSE, TRUE, FALSE);
        FlushHobVariableToStorage (VariableName, VendorGuid, NULL);
      }
      goto Done;
    }
    //
    // If the variable is marked valid, and the same data has been passed in,
    // then return to the caller immediately.
    //
    if (DataSizeOfVariable (CacheVariable->CurrPtr) == DataSize &&
        (CompareMem (Data, GetVariableDataPtr (CacheVariable->CurrPtr), DataSize) == 0) &&
        ((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0) &&
        (TimeStamp == NULL)) {
      //
      // Variable content unchanged and no need to update timestamp, just return.
      //
      UpdateVariableInfo (VariableName, VendorGuid, Variable->Volatile, FALSE, TRUE, FALSE, FALSE);
      Status = EFI_SUCCESS;
      goto Done;
    } else if ((CacheVariable->CurrPtr->State == VAR_ADDED) ||
               (CacheVariable->CurrPtr->State == (VAR_ADDED & VAR_IN_DELETED_TRANSITION))) {

      //
      // EFI_VARIABLE_APPEND_WRITE attribute only effects for existing variable.
      //
      if ((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0) {
        //
        // NOTE: From 0 to DataOffset of NextVariable is reserved for Variable Header and Name.
        // From DataOffset of NextVariable is to save the existing variable data.
        //
        DataOffset = GetVariableDataOffset (CacheVariable->CurrPtr);
        BufferForMerge = (UINT8 *) ((UINTN) NextVariable + DataOffset);
        CopyMem (BufferForMerge, (UINT8 *) ((UINTN) CacheVariable->CurrPtr + DataOffset), DataSizeOfVariable (CacheVariable->CurrPtr));

        //
        // Set Max Common/Auth Variable Data Size as default MaxDataSize.
        // Max Harware error record variable data size is different from common/auth variable.
        //
        if ((Attributes & VARIABLE_ATTRIBUTE_AT_AW) != 0) {
          MaxDataSize = mVariableModuleGlobal->MaxAuthVariableSize - DataOffset;
        } else if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
          MaxDataSize = mVariableModuleGlobal->MaxVariableSize - DataOffset;
        } else {
          MaxDataSize = mVariableModuleGlobal->MaxVolatileVariableSize - DataOffset;
        }
        if ((Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
          MaxDataSize = PcdGet32 (PcdMaxHardwareErrorVariableSize) - DataOffset;
        }

        if (DataSizeOfVariable (CacheVariable->CurrPtr) + DataSize > MaxDataSize) {
          //
          // Existing data size + new data size exceed maximum variable size limitation.
          //
          Status = EFI_INVALID_PARAMETER;
          goto Done;
        }
        //
        // Append the new data to the end of existing data.
        //
        CopyMem ((UINT8*) ((UINTN) BufferForMerge + DataSizeOfVariable (CacheVariable->CurrPtr)), Data, DataSize);
        MergedBufSize = DataSizeOfVariable (CacheVariable->CurrPtr) + DataSize;

        //
        // BufferForMerge(from DataOffset of NextVariable) has included the merged existing and new data.
        //
        Data      = BufferForMerge;
        DataSize  = MergedBufSize;
        DataReady = TRUE;
      }

      //
      // Mark the old variable as in delete transition.
      //
      State = CacheVariable->CurrPtr->State;
      Variable->CurrPtr->State &= VAR_IN_DELETED_TRANSITION;
    }
  } else {
    DEBUG ((DEBUG_INFO, "  Variable Driver: New variable being written.\n"));
    //
    // Not found existing variable. Create a new variable.
    //

    if ((DataSize == 0) && ((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0)) {
      Status = EFI_SUCCESS;
      goto Done;
    }

    //
    // Make sure we are trying to create a new variable.
    // Setting a data variable with zero DataSize or no access attributes means to delete it.
    //
    if (DataSize == 0 || (Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)) == 0) {
      Status = EFI_NOT_FOUND;
      goto Done;
    }

    //
    // Only variable have NV|RT attribute can be created in Runtime.
    //
    if (AtRuntime () &&
        (((Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0) || ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0))) {
      Status = EFI_INVALID_PARAMETER;
      goto Done;
    }
  }

  //
  // Function part - create a new variable and copy the data.
  // Both update a variable and create a variable will come here.
  //
  NextVariable->StartId     = VARIABLE_DATA;
  //
  // NextVariable->State = VAR_ADDED;
  //
  NextVariable->Reserved        = 0;
  if (mVariableModuleGlobal->VariableGlobal.AuthFormat) {
    AuthVariable = (AUTHENTICATED_VARIABLE_HEADER *) NextVariable;
    AuthVariable->PubKeyIndex    = KeyIndex;
    AuthVariable->MonotonicCount = MonotonicCount;
    ZeroMem (&AuthVariable->TimeStamp, sizeof (EFI_TIME));

    if (((Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) != 0) &&
        (TimeStamp != NULL)) {
      if ((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0) {
        CopyMem (&AuthVariable->TimeStamp, TimeStamp, sizeof (EFI_TIME));
      } else {
        //
        // In the case when the EFI_VARIABLE_APPEND_WRITE attribute is set, only
        // when the new TimeStamp value is later than the current timestamp associated
        // with the variable, we need associate the new timestamp with the updated value.
        //
        if (Variable->CurrPtr != NULL) {
          if (VariableCompareTimeStampInternal (&(((AUTHENTICATED_VARIABLE_HEADER *) CacheVariable->CurrPtr)->TimeStamp), TimeStamp)) {
            CopyMem (&AuthVariable->TimeStamp, TimeStamp, sizeof (EFI_TIME));
          } else {
            CopyMem (&AuthVariable->TimeStamp, &(((AUTHENTICATED_VARIABLE_HEADER *) CacheVariable->CurrPtr)->TimeStamp), sizeof (EFI_TIME));
          }
        }
      }
    }
  }

  //
  // The EFI_VARIABLE_APPEND_WRITE attribute will never be set in the returned
  // Attributes bitmask parameter of a GetVariable() call.
  //
  NextVariable->Attributes  = Attributes & (~EFI_VARIABLE_APPEND_WRITE);

  VarNameOffset                 = GetVariableHeaderSize ();
  VarNameSize                   = StrSize (VariableName);
  CopyMem (
    (UINT8 *) ((UINTN) NextVariable + VarNameOffset),
    VariableName,
    VarNameSize
    );
  VarDataOffset = VarNameOffset + VarNameSize + GET_PAD_SIZE (VarNameSize);

  //
  // If DataReady is TRUE, it means the variable data has been saved into
  // NextVariable during EFI_VARIABLE_APPEND_WRITE operation preparation.
  //
  if (!DataReady) {
    CopyMem (
      (UINT8 *) ((UINTN) NextVariable + VarDataOffset),
      Data,
      DataSize
      );
  }

  CopyMem (GetVendorGuidPtr (NextVariable), VendorGuid, sizeof (EFI_GUID));
  //
  // There will be pad bytes after Data, the NextVariable->NameSize and
  // NextVariable->DataSize should not include pad size so that variable
  // service can get actual size in GetVariable.
  //
  SetNameSizeOfVariable (NextVariable, VarNameSize);
  SetDataSizeOfVariable (NextVariable, DataSize);

  //
  // The actual size of the variable that stores in storage should
  // include pad size.
  //
  VarSize = VarDataOffset + DataSize + GET_PAD_SIZE (DataSize);
  if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
    //
    // Create a nonvolatile variable.
    //
    Volatile = FALSE;

    IsCommonVariable = FALSE;
    IsCommonUserVariable = FALSE;
    LastVariable  = GetStartPointer (mNvVariableCache);
    while (IsValidVariableHeader (LastVariable, GetEndPointer (mNvVariableCache))) {
      LastVariable = GetNextVariablePtr (LastVariable);
    }
    CacheOffset = (UINTN) LastVariable - (UINTN) mNvVariableCache;
    if ((Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == 0) {
      IsCommonVariable = TRUE;
      IsCommonUserVariable = IsUserVariable (NextVariable);
    }
    if ((((Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != 0)
      && ((VarSize + mVariableModuleGlobal->HwErrVariableTotalSize) > PcdGet32 (PcdHwErrStorageSize)))
      || (IsCommonVariable && ((VarSize + mVariableModuleGlobal->CommonVariableTotalSize) > mVariableModuleGlobal->CommonVariableSpace))
      || (IsCommonVariable && AtRuntime () && ((VarSize + mVariableModuleGlobal->CommonVariableTotalSize) > mVariableModuleGlobal->CommonRuntimeVariableSpace))
      || (IsCommonUserVariable && ((VarSize + mVariableModuleGlobal->CommonUserVariableTotalSize) > mVariableModuleGlobal->CommonMaxUserVariableSpace))
      || (UINT32) (VarSize + CacheOffset) > mNvVariableCache->Size) {
      if (AtRuntime ()) {
        if (IsCommonUserVariable && ((VarSize + mVariableModuleGlobal->CommonUserVariableTotalSize) > mVariableModuleGlobal->CommonMaxUserVariableSpace)) {
          RecordVarErrorFlag (
            VAR_ERROR_FLAG_USER_ERROR,
            VariableName,
            VendorGuid,
            Attributes,
            VarSize,
            CommandInProgress,
            InProgressInstanceGuid
            );
        }
        if (IsCommonVariable && ((VarSize + mVariableModuleGlobal->CommonVariableTotalSize) > mVariableModuleGlobal->CommonRuntimeVariableSpace)) {
          RecordVarErrorFlag (
            VAR_ERROR_FLAG_SYSTEM_ERROR,
            VariableName,
            VendorGuid,
            Attributes,
            VarSize,
            CommandInProgress,
            InProgressInstanceGuid
            );
        }
        Status = EFI_OUT_OF_RESOURCES;
        goto Done;
      }
      //
      // Perform garbage collection & reclaim operation, and integrate the new variable at the same time.
      //
      Status = Reclaim (
                 (EFI_PHYSICAL_ADDRESS) (UINTN) mNvVariableCache,
                 &CacheOffset,
                 FALSE,
                 Variable,
                 NextVariable,
                 HEADER_ALIGN (VarSize),
                 CommandInProgress,
                 InProgressInstanceGuid
                 );
      if (!EFI_ERROR (Status)) {
        //
        // The new variable has been integrated successfully during reclaiming.
        //
        if (Variable->CurrPtr != NULL) {
          CacheVariable->CurrPtr = (VARIABLE_HEADER *)((UINTN) CacheVariable->StartPtr + ((UINTN) Variable->CurrPtr - (UINTN) Variable->StartPtr));
          CacheVariable->InDeletedTransitionPtr = NULL;
        }
        UpdateVariableInfo (VariableName, VendorGuid, FALSE, FALSE, TRUE, FALSE, FALSE);
        FlushHobVariableToStorage (VariableName, VendorGuid, NULL);
      } else {
        if (IsCommonUserVariable && ((VarSize + mVariableModuleGlobal->CommonUserVariableTotalSize) > mVariableModuleGlobal->CommonMaxUserVariableSpace)) {
          RecordVarErrorFlag (
            VAR_ERROR_FLAG_USER_ERROR,
            VariableName,
            VendorGuid,
            Attributes,
            VarSize,
            CommandInProgress,
            InProgressInstanceGuid
            );
        }
        if (IsCommonVariable && ((VarSize + mVariableModuleGlobal->CommonVariableTotalSize) > mVariableModuleGlobal->CommonVariableSpace)) {
          RecordVarErrorFlag (
            VAR_ERROR_FLAG_SYSTEM_ERROR,
            VariableName,
            VendorGuid,
            Attributes,
            VarSize,
            CommandInProgress,
            InProgressInstanceGuid
            );
        }
      }
      goto Done;
    }
    //
    // Write the variable to NV
    //
    if (!OnlyUpdateNvCache) {
      Status = GetVariableStorageProtocol (
                VariableName,
                VendorGuid,
                &VariableStorageProtocol
                );
      if (EFI_ERROR (Status) || VariableStorageProtocol == NULL) {
        goto Done;
      }
      Status = VariableStorageProtocol->SetVariable (
                                          VariableStorageProtocol,
                                          AtRuntime (),
                                          mFromSmm,
                                          VariableName,
                                          VendorGuid,
                                          Attributes,
                                          DataSize,
                                          Data,
                                          KeyIndex,
                                          MonotonicCount,
                                          TimeStamp,
                                          CommandInProgress
                                          );
      DEBUG ((DEBUG_INFO, "  Variable Driver: Variable was written to NV via the storage protocol. Status = %r.\n", Status));
      if (*CommandInProgress) {
        DEBUG ((DEBUG_INFO, "  Variable Driver: SetVariable returned CommandInProgress\n", Status));
        VariableStorageProtocol->GetId (VariableStorageProtocol, InProgressInstanceGuid);
      }
      if (EFI_ERROR (Status)) {
        goto Done;
      }
      if ((Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != 0) {
        mVariableModuleGlobal->HwErrVariableTotalSize += HEADER_ALIGN (VarSize);
      } else {
        mVariableModuleGlobal->CommonVariableTotalSize += HEADER_ALIGN (VarSize);
        if (IsCommonUserVariable) {
          mVariableModuleGlobal->CommonUserVariableTotalSize += HEADER_ALIGN (VarSize);
        }
      }
    }
    //
    // Update the NV Cache
    //
    NextVariable->State = VAR_ADDED;
    Status2 = UpdateVolatileVariableStore (
                &mVariableModuleGlobal->VariableGlobal,
                TRUE,
                CacheOffset,
                (UINT32) VarSize,
                (UINT8 *) NextVariable,
                mNvVariableCache
                );
    DEBUG ((DEBUG_VERBOSE, "  Variable Driver: Wrote the variable to the NV cache in UpdateVariable().\n"));
    if (EFI_ERROR (Status2)) {
      Status = Status2;
      DEBUG ((DEBUG_ERROR, "  Error updating NV cache. Status = %r.\n", Status));
      goto Done;
    }
    Status =  SynchronizeRuntimeVariableCache (
                &mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache,
                CacheOffset,
                VarSize
                );
    ASSERT_EFI_ERROR (Status);
  } else {
    //
    // Create a volatile variable.
    //
    Volatile = TRUE;

    DEBUG ((DEBUG_INFO, "  Variable Driver: Creating a volatile variable.\n"));

    if ((UINT32) (VarSize + mVariableModuleGlobal->VolatileLastVariableOffset) >
        ((VARIABLE_STORE_HEADER *) ((UINTN) (mVariableModuleGlobal->VariableGlobal.VolatileVariableBase)))->Size) {
      //
      // Perform garbage collection & reclaim operation, and integrate the new variable at the same time.
      //
      Status = Reclaim (
                 mVariableModuleGlobal->VariableGlobal.VolatileVariableBase,
                 &mVariableModuleGlobal->VolatileLastVariableOffset,
                 TRUE,
                 Variable,
                 NextVariable,
                 HEADER_ALIGN (VarSize),
                 CommandInProgress,
                 InProgressInstanceGuid
                 );
      if (*CommandInProgress) {
        DEBUG ((DEBUG_ERROR, "  Variable Driver: CommandInProgress should never be set on volatile variable update\n"));
        ASSERT (FALSE);
        Status = EFI_OUT_OF_RESOURCES;
        goto Done;
      }
      if (!EFI_ERROR (Status)) {
        //
        // The new variable has been integrated successfully during reclaiming.
        //
        if (Variable->CurrPtr != NULL) {
          CacheVariable->CurrPtr = (VARIABLE_HEADER *)((UINTN) CacheVariable->StartPtr + ((UINTN) Variable->CurrPtr - (UINTN) Variable->StartPtr));
          CacheVariable->InDeletedTransitionPtr = NULL;
        }
        UpdateVariableInfo (VariableName, VendorGuid, TRUE, FALSE, TRUE, FALSE, FALSE);
      }
      goto Done;
    }

    NextVariable->State = VAR_ADDED;
    Status = UpdateVolatileVariableStore (
                &mVariableModuleGlobal->VariableGlobal,
                TRUE,
                mVariableModuleGlobal->VolatileLastVariableOffset,
                (UINT32) VarSize,
                (UINT8 *) NextVariable,
                (VARIABLE_STORE_HEADER *) ((UINTN) mVariableModuleGlobal->VariableGlobal.VolatileVariableBase)
                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "  Error updating NV cache. Status = %r.\n", Status));
      goto Done;
    }
    Status =  SynchronizeRuntimeVariableCache (
                &mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache,
                mVariableModuleGlobal->VolatileLastVariableOffset,
                VarSize
                );
    ASSERT_EFI_ERROR (Status);
    mVariableModuleGlobal->VolatileLastVariableOffset += HEADER_ALIGN (VarSize);
  }

  //
  // Mark the old variable as deleted.
  //
  if (!EFI_ERROR (Status) && Variable->CurrPtr != NULL) {
    //
    // Reduce the current NV storage usage counts by the old variable's size
    //
    if (!OnlyUpdateNvCache) {
      OldVariable = GetNextVariablePtr (Variable->CurrPtr);
      OldVariableSize = (UINTN) OldVariable - (UINTN) Variable->CurrPtr;
      if ((Variable->CurrPtr->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
        mVariableModuleGlobal->HwErrVariableTotalSize -= OldVariableSize;
      } else if ((Variable->CurrPtr->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
        mVariableModuleGlobal->CommonVariableTotalSize -= OldVariableSize;
        if (IsUserVariable (Variable->CurrPtr)) {
          mVariableModuleGlobal->CommonUserVariableTotalSize -= OldVariableSize;
        }
      }
    }
    if (Variable->Volatile) {
      VolatileCacheInstance = &(mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeVolatileCache);
    } else {
      VolatileCacheInstance = &(mVariableModuleGlobal->VariableGlobal.VariableRuntimeCacheContext.VariableRuntimeNvCache);
    }
    if (Variable->InDeletedTransitionPtr != NULL) {
      //
      // Both ADDED and IN_DELETED_TRANSITION old variable are present,
      // set IN_DELETED_TRANSITION one to DELETED state first.
      //
      ASSERT (CacheVariable->InDeletedTransitionPtr != NULL);
      State = CacheVariable->InDeletedTransitionPtr->State;
      State &= VAR_DELETED;
      Status =  UpdateVolatileVariableStore (
                  &mVariableModuleGlobal->VariableGlobal,
                  FALSE,
                  (UINTN) &Variable->InDeletedTransitionPtr->State,
                  sizeof (UINT8),
                  &State,
                  (
                    (Variable->Volatile) ?
                    ((VARIABLE_STORE_HEADER *) ((UINTN) mVariableModuleGlobal->VariableGlobal.VolatileVariableBase)) :
                    mNvVariableCache
                    )
                  );
      if (EFI_ERROR (Status)) {
        goto Done;
      }
      Status =  SynchronizeRuntimeVariableCache (
                  VolatileCacheInstance,
                  (
                    ((UINTN) &Variable->InDeletedTransitionPtr->State) -
                    (((UINTN) Variable->StartPtr) - HEADER_ALIGN (sizeof (VARIABLE_STORE_HEADER)))
                    ),
                  sizeof (UINT8)
                  );
      ASSERT_EFI_ERROR (Status);
    }

    State = Variable->CurrPtr->State;
    State &= VAR_DELETED;

    Status =  UpdateVolatileVariableStore (
                &mVariableModuleGlobal->VariableGlobal,
                FALSE,
                (UINTN) &Variable->CurrPtr->State,
                sizeof (UINT8),
                &State,
                (
                  (Variable->Volatile) ?
                  ((VARIABLE_STORE_HEADER *) ((UINTN) mVariableModuleGlobal->VariableGlobal.VolatileVariableBase)) : mNvVariableCache
                  )
                );
    if (!EFI_ERROR (Status)) {
      Status =  SynchronizeRuntimeVariableCache (
                  VolatileCacheInstance,
                  (
                    ((UINTN) &Variable->CurrPtr->State) -
                    (((UINTN) Variable->StartPtr) - HEADER_ALIGN (sizeof (VARIABLE_STORE_HEADER)))
                    ),
                  sizeof (UINT8)
                  );
      ASSERT_EFI_ERROR (Status);
    }
  }

  if (!EFI_ERROR (Status)) {
    UpdateVariableInfo (VariableName, VendorGuid, Volatile, FALSE, TRUE, FALSE, FALSE);
    if (!Volatile) {
      FlushHobVariableToStorage (VariableName, VendorGuid, NULL);
    }
  }

Done:
  return Status;
}

/**
  Update the variable region with Variable information. If EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS is set,
  index of associated public key is needed.

  @param[in] VariableName       Name of variable.
  @param[in] VendorGuid         Guid of variable.
  @param[in] Data               Variable data.
  @param[in] DataSize           Size of data. 0 means delete.
  @param[in] Attributes         Attributes of the variable.
  @param[in] KeyIndex           Index of associated public key.
  @param[in] MonotonicCount     Value of associated monotonic count.
  @param[in, out] CacheVariable The variable information which is used to keep track of variable usage.
  @param[in] TimeStamp          Value of associated TimeStamp.

  @retval EFI_SUCCESS           The update operation is success.
  @retval EFI_OUT_OF_RESOURCES  Variable region is full, can not write other data into this region.

**/
EFI_STATUS
UpdateVariable (
  IN      CHAR16                      *VariableName,
  IN      EFI_GUID                    *VendorGuid,
  IN      VOID                        *Data,
  IN      UINTN                       DataSize,
  IN      UINT32                      Attributes      OPTIONAL,
  IN      UINT32                      KeyIndex        OPTIONAL,
  IN      UINT64                      MonotonicCount  OPTIONAL,
  IN OUT  VARIABLE_POINTER_TRACK      *CacheVariable,
  IN      EFI_TIME                    *TimeStamp      OPTIONAL
  )
{
  EFI_STATUS                Status;
  BOOLEAN                   CommandInProgress;
  EFI_GUID                  InProgressInstanceGuid;

  CommandInProgress = FALSE;
  Status = UpdateVariableInternal (
             VariableName,
             VendorGuid,
             Data,
             DataSize,
             Attributes,
             KeyIndex,
             MonotonicCount,
             CacheVariable,
             TimeStamp,
             mNvVariableEmulationMode,
             &CommandInProgress,
             &InProgressInstanceGuid
             );
  if (CommandInProgress) {
    if (mCommandInProgress) {
      if (!CompareGuid (&InProgressInstanceGuid, &mInProgressInstanceGuid)) {
        DEBUG ((DEBUG_ERROR, "Two different EDKII_VARIABLE_STORAGE_PROTOCOLs can not perform asyncronous I/O at once\n"));
        ASSERT (FALSE);
        return EFI_DEVICE_ERROR;
      }
    }
    mCommandInProgress = TRUE;
    CopyMem (&mInProgressInstanceGuid, &InProgressInstanceGuid, sizeof (EFI_GUID));
  }
  return Status;
}

/**
  Finds variable in storage blocks of volatile and non-volatile storage areas.

  This code finds variable in storage blocks of volatile and non-volatile storage areas.
  If VariableName is an empty string, then we just return the first
  qualified variable without comparing VariableName and VendorGuid.
  If IgnoreRtCheck is TRUE, then we ignore the EFI_VARIABLE_RUNTIME_ACCESS attribute check
  at runtime when searching existing variable, only VariableName and VendorGuid are compared.
  Otherwise, variables without EFI_VARIABLE_RUNTIME_ACCESS are not visible at runtime.

  @param[in]   VariableName           Name of the variable to be found.
  @param[in]   VendorGuid             Vendor GUID to be found.
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
FindVariable (
  IN  CHAR16                  *VariableName,
  IN  EFI_GUID                *VendorGuid,
  OUT VARIABLE_POINTER_TRACK  *PtrTrack,
  IN  VARIABLE_GLOBAL         *Global,
  IN  BOOLEAN                 IgnoreRtCheck,
  OUT BOOLEAN                 *CommandInProgress,
  OUT EFI_GUID                *InProgressInstanceGuid
  )
{
  EFI_STATUS                                Status;
  EFI_STATUS                                Status2;
  VARIABLE_STORE_HEADER                     *VariableStoreHeader[VariableStoreTypeMax];
  EFI_GUID                                  VariableStorageId;
  EFI_GUID                                  InstanceGuid;
  EDKII_VARIABLE_STORAGE_PROTOCOL           *VariableStorageProtocol;
  EDKII_VARIABLE_STORAGE_PROTOCOL           *CorrectVariableStorageProtocol;
  EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL  *VariableStorageSelectorProtocol;
  UINTN                                     Index;
  VARIABLE_STORE_TYPE                       Type;
  EFI_TIME                                  TimeStamp;
  UINT64                                    MonotonicCount;
  UINTN                                     DataSize;
  UINT32                                    Attributes;
  UINT32                                    KeyIndex;
  BOOLEAN                                   FailedRtCheck;
  BOOLEAN                                   DataIsReady;
  BOOLEAN                                   TempCommandInProgress;
  EFI_GUID                                  TempInProgressInstanceGuid;

  *CommandInProgress  = FALSE;
  FailedRtCheck       = FALSE;
  DataIsReady         = TRUE;
  if (VariableName[0] != 0 && VendorGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // 0: Volatile, 1: HOB, 2: Non-Volatile Cache.
  // The index and attributes mapping must be kept in this order as RuntimeServiceGetNextVariableName
  // make use of this mapping to implement search algorithm.
  //
  VariableStoreHeader[VariableStoreTypeVolatile] = (VARIABLE_STORE_HEADER *) (UINTN) Global->VolatileVariableBase;
  VariableStoreHeader[VariableStoreTypeHob]      = (VARIABLE_STORE_HEADER *) (UINTN) Global->HobVariableBase;
  VariableStoreHeader[VariableStoreTypeNvCache]  = mNvVariableCache;

  DEBUG ((DEBUG_INFO, "+-+-> Variable Driver: FindVariable.\n  Variable Name: %s.\n  Guid:  %g.\n", VariableName, VendorGuid));

  //
  // Find the variable by walk through HOB, volatile and non-volatile variable store.
  //
  for (Type = (VARIABLE_STORE_TYPE) 0; Type < VariableStoreTypeMax; Type++) {
    if (VariableStoreHeader[Type] == NULL) {
      continue;
    }

    PtrTrack->StartPtr = GetStartPointer (VariableStoreHeader[Type]);
    PtrTrack->EndPtr   = GetEndPointer   (VariableStoreHeader[Type]);
    PtrTrack->Volatile = (BOOLEAN) (Type == VariableStoreTypeVolatile);

    Status = FindVariableEx (VariableName, VendorGuid, TRUE, PtrTrack);
    if (!EFI_ERROR (Status)) {
      FailedRtCheck = FALSE;
      if (!IgnoreRtCheck &&
          ((PtrTrack->CurrPtr->Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0) &&
          AtRuntime ()) {
        FailedRtCheck = TRUE;
        continue;
      }
      DEBUG ((DEBUG_INFO, "Variable Driver: Found the variable in store type %d before going to protocols.\n", Type));
      return Status;
    }
  }
  if (FailedRtCheck) {
    PtrTrack->CurrPtr                 = NULL;
    PtrTrack->InDeletedTransitionPtr  = NULL;
    return EFI_NOT_FOUND;
  }
  if (mNvVariableEmulationMode) {
    return EFI_NOT_FOUND;
  }

  //
  // If VariableName is an empty string get the first variable from the first NV storage
  //
  if (VariableName[0] == 0) {
    return FindFirstNvVariable (PtrTrack, Global, IgnoreRtCheck, CommandInProgress, InProgressInstanceGuid);
  }
  //
  // Search the EDKII_VARIABLE_STORAGE_PROTOCOLs
  // First, try the protocol instance which the VariableStorageSelectorProtocol suggests
  //
  ASSERT (mVariableDataBuffer != NULL);
  if (mVariableDataBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  CorrectVariableStorageProtocol = NULL;
  ZeroMem ((VOID *) &VariableStorageId, sizeof (EFI_GUID));

  VariableStorageSelectorProtocol = mVariableModuleGlobal->VariableGlobal.VariableStorageSelectorProtocol;
  if (VariableStorageSelectorProtocol == NULL) {
    ASSERT (mNvVariableEmulationMode == TRUE);
    return EFI_NOT_FOUND;
  }

  Status = VariableStorageSelectorProtocol->GetId (VariableName, VendorGuid, &VariableStorageId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
    ZeroMem ((VOID *) &InstanceGuid, sizeof (EFI_GUID));
    Status = VariableStorageProtocol->GetId (VariableStorageProtocol, &InstanceGuid);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    if (CompareGuid (&VariableStorageId, &InstanceGuid)) {
      CorrectVariableStorageProtocol = VariableStorageProtocol;
      DataSize = mVariableModuleGlobal->ScratchBufferSize;
      ZeroMem (mVariableDataBuffer, DataSize);
      Status = VariableStorageProtocol->GetAuthenticatedVariable (
                                          VariableStorageProtocol,
                                          AtRuntime (),
                                          mFromSmm,
                                          VariableName,
                                          VendorGuid,
                                          &Attributes,
                                          &DataSize,
                                          mVariableDataBuffer,
                                          &KeyIndex,
                                          &MonotonicCount,
                                          &TimeStamp,
                                          CommandInProgress
                                          );
      DEBUG ((DEBUG_INFO, "  Variable Driver: Variable storage protocol GetAuthenticatedVariable status = %r.\n", Status));
      if (!EFI_ERROR (Status) && (*CommandInProgress)) {
        CopyMem (InProgressInstanceGuid, &InstanceGuid, sizeof (EFI_GUID));
        DataIsReady = FALSE;
      }

      if (!EFI_ERROR (Status)) {
        goto UpdateNvCache;
      }
      ASSERT (Status != EFI_BUFFER_TOO_SMALL);
      if (Status != EFI_NOT_FOUND) {
        return Status;
      }
      break;
    }
  }
  //
  // As a fallback, try searching the remaining EDKII_VARIABLE_STORAGE_PROTOCOLs even
  // though the variable shouldn't actually be stored in there
  //
  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
    ZeroMem ((VOID *) &InstanceGuid, sizeof (EFI_GUID));
    Status = VariableStorageProtocol->GetId (VariableStorageProtocol, &InstanceGuid);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    if (!CompareGuid (&VariableStorageId, &InstanceGuid)) {
      DataSize = mVariableModuleGlobal->ScratchBufferSize;
      ZeroMem (mVariableDataBuffer, DataSize);
      Status = VariableStorageProtocol->GetAuthenticatedVariable (
                                          VariableStorageProtocol,
                                          AtRuntime (),
                                          mFromSmm,
                                          VariableName,
                                          VendorGuid,
                                          &Attributes,
                                          &DataSize,
                                          mVariableDataBuffer,
                                          &KeyIndex,
                                          &MonotonicCount,
                                          &TimeStamp,
                                          CommandInProgress
                                          );
      if (!EFI_ERROR (Status) && (*CommandInProgress)) {
        CopyMem (InProgressInstanceGuid, &InstanceGuid, sizeof (EFI_GUID));
        DataIsReady = FALSE;
      }
      if (!EFI_ERROR (Status)) {
        //
        // The variable isn't being stored in the correct VariableStorage,
        // attempt to move it to the correct VariableStorage
        //
        DEBUG ((
          DEBUG_INFO,
          "Variable %s is not being stored in the correct NV storage!\n",
          VariableName
          ));
        DEBUG ((
            DEBUG_INFO,
            "Expected StorageId = %g, Actual StorageId = %g\n",
            &VariableStorageId,
            &InstanceGuid
            ));
        if ((CorrectVariableStorageProtocol != NULL) && !(*CommandInProgress)) {
          if (CorrectVariableStorageProtocol->WriteServiceIsReady (
                                                CorrectVariableStorageProtocol) &&
              VariableStorageProtocol->WriteServiceIsReady (
                                        VariableStorageProtocol)) {
            Status2 = CorrectVariableStorageProtocol->SetVariable (
                                                        CorrectVariableStorageProtocol,
                                                        AtRuntime (),
                                                        mFromSmm,
                                                        VariableName,
                                                        VendorGuid,
                                                        Attributes,
                                                        DataSize,
                                                        mVariableDataBuffer,
                                                        KeyIndex,
                                                        MonotonicCount,
                                                        &TimeStamp,
                                                        CommandInProgress
                                                        );
            if (!EFI_ERROR (Status2) && (*CommandInProgress)) {
              CopyMem (InProgressInstanceGuid, &VariableStorageId, sizeof (EFI_GUID));
              goto UpdateNvCache;
            }
            if (EFI_ERROR (Status2) || (*CommandInProgress)) {
              DEBUG ((DEBUG_INFO, "Failed to copy variable to correct VariableStorage!\n"));
              goto UpdateNvCache;
            }
            //
            // Delete the redundant copy that is incorrectly stored
            //
            Status2 = VariableStorageProtocol->SetVariable (
                                                 VariableStorageProtocol,
                                                 AtRuntime (),
                                                 mFromSmm,
                                                 VariableName,
                                                 VendorGuid,
                                                 Attributes,
                                                 0,
                                                 NULL,
                                                 0,
                                                 0,
                                                 &TimeStamp,
                                                 CommandInProgress
                                                 );
            if (!EFI_ERROR (Status2) && (*CommandInProgress)) {
              CopyMem (InProgressInstanceGuid, &InstanceGuid, sizeof (EFI_GUID));
            }
            if (EFI_ERROR (Status2)) {
              DEBUG ((DEBUG_INFO, "  Variable Driver: Failed to delete redundant copy of variable in the incorrect VariableStorage!\n"));
            }
            DEBUG ((DEBUG_INFO, "  Variable Driver: Variable has been moved to the correct VariableStorage.\n"));
          } else {
            DEBUG ((DEBUG_INFO, "  Variable Driver: VariableStorage is not ready to write, unable to move variable.\n"));
          }
        } else {
          DEBUG ((DEBUG_INFO, "  Variable Driver: Expected VariableStorage does not exist or async I/O is pending!\n"));
        }
        goto UpdateNvCache;
      }
      ASSERT (Status != EFI_BUFFER_TOO_SMALL);
      if (Status != EFI_NOT_FOUND) {
        return Status;
      }
    }
  }
  return EFI_NOT_FOUND;
UpdateNvCache:
  if (DataIsReady) {
    DEBUG ((DEBUG_INFO, "  Variable Driver: Updating the cache for this variable.\n"));

    PtrTrack->CurrPtr                 = NULL;
    PtrTrack->InDeletedTransitionPtr  = NULL;
    PtrTrack->StartPtr                = GetStartPointer (mNvVariableCache);
    PtrTrack->EndPtr                  = GetEndPointer   (mNvVariableCache);
    PtrTrack->Volatile                = FALSE;
    Status = UpdateVariableInternal (
               VariableName,
               VendorGuid,
               mVariableDataBuffer,
               DataSize,
               Attributes,
               KeyIndex,
               MonotonicCount,
               PtrTrack,
               &TimeStamp,
               TRUE,
               &TempCommandInProgress,
               &TempInProgressInstanceGuid
               );
    DEBUG ((DEBUG_INFO, "  Variable Driver: UpdateVariable status = %r.\n", Status));
    //
    // CommandInProgress should never be TRUE since we are only doing an NV cache update
    //
    ASSERT (!TempCommandInProgress);
    if (TempCommandInProgress) {
      return EFI_OUT_OF_RESOURCES;
    }
    if (!EFI_ERROR (Status)) {
      PtrTrack->StartPtr = GetStartPointer (mNvVariableCache);
      PtrTrack->EndPtr   = GetEndPointer   (mNvVariableCache);
      PtrTrack->Volatile = FALSE;
      Status = FindVariableEx (VariableName, VendorGuid, IgnoreRtCheck, PtrTrack);
    }
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
  @param Data                       The buffer to return the contents of the variable. May be NULL
                                    with a zero DataSize in order to determine the size buffer needed.
  @param CommandInProgress          TRUE if the command requires asyncronous I/O and has not completed yet
                                    be updated and do not contain valid data.  Asyncronous I/O should only
                                    FALSE otherwise.  This parameter is only be used by the SMM Variable
                                    Services implementation.  The Runtime DXE implementation always returns FALSE.
  @param InProgressInstanceGuid     If CommandInProgress is TRUE, this will contain the instance GUID of the Variable
                                    Storage driver that is performing the asyncronous I/O

  @return EFI_INVALID_PARAMETER     Invalid parameter.
  @return EFI_SUCCESS               Find the specified variable.
  @return EFI_NOT_FOUND             Not found.
  @return EFI_BUFFER_TO_SMALL       DataSize is too small for the result.

**/
EFI_STATUS
EFIAPI
VariableServiceGetVariable (
  IN      CHAR16            *VariableName,
  IN      EFI_GUID          *VendorGuid,
  OUT     UINT32            *Attributes OPTIONAL,
  IN OUT  UINTN             *DataSize,
  OUT     VOID              *Data OPTIONAL,
  OUT     BOOLEAN           *CommandInProgress,
  OUT     EFI_GUID          *InProgressInstanceGuid
  )
{
  EFI_STATUS              Status;
  VARIABLE_POINTER_TRACK  Variable;
  UINTN                   VarDataSize;

  *CommandInProgress = FALSE;
  if (VariableName == NULL || VendorGuid == NULL || DataSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (VariableName[0] == 0) {
    return EFI_NOT_FOUND;
  }

  AcquireLockOnlyAtBootTime(&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  Status = FindVariable (
             VariableName,
             VendorGuid,
             &Variable,
             &mVariableModuleGlobal->VariableGlobal,
             FALSE,
             CommandInProgress,
             InProgressInstanceGuid
             );
  if (Variable.CurrPtr == NULL || EFI_ERROR (Status) || (*CommandInProgress)) {
    goto Done;
  }

  //
  // Get data size
  //
  VarDataSize = DataSizeOfVariable (Variable.CurrPtr);
  ASSERT (VarDataSize != 0);

  if (*DataSize >= VarDataSize) {
    if (Data == NULL) {
      Status = EFI_INVALID_PARAMETER;
      goto Done;
    }

    CopyMem (Data, GetVariableDataPtr (Variable.CurrPtr), VarDataSize);
    if (Attributes != NULL) {
      *Attributes = Variable.CurrPtr->Attributes;
    }

    *DataSize = VarDataSize;
    UpdateVariableInfo (VariableName, VendorGuid, Variable.Volatile, TRUE, FALSE, FALSE, FALSE);

    Status = EFI_SUCCESS;
    goto Done;
  } else {
    *DataSize = VarDataSize;
    Status = EFI_BUFFER_TOO_SMALL;
    goto Done;
  }

Done:
  ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
  return Status;
}

/**
  This code Finds the Next available variable.

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode. This function will do basic validation, before parse the data.

  @param VariableNameSize   The size of the VariableName buffer. The size must be large.
  @param VariableName       Pointer to variable name.
  @param VendorGuid         Variable Vendor Guid.

  @return EFI_SUCCESS       Find the specified variable.
  @return EFI_NOT_FOUND     Not found.

**/
EFI_STATUS
EFIAPI
VariableServiceGetNextVariableInternal (
  IN OUT UINTN              *VariableNameSize,
  IN OUT CHAR16             *VariableName,
  IN OUT EFI_GUID           *VariableGuid
  )
{
  EFI_GUID                    VariableStorageId;
  EFI_GUID                    InstanceGuid;
  VARIABLE_HEADER             *VariablePtr;
  UINTN                       Index;
  UINTN                       NextIndex;
  UINTN                       VarNameSize;
  UINT32                      VarAttributes;
  UINTN                       CallerVariableNameBufferSize;
  EFI_STATUS                  Status;
  BOOLEAN                     SearchComplete;
  BOOLEAN                     CurrentVariableInMemory;

  EDKII_VARIABLE_STORAGE_PROTOCOL           *VariableStorage;
  EDKII_VARIABLE_STORAGE_SELECTOR_PROTOCOL  *VariableStorageSelectorProtocol;

  CallerVariableNameBufferSize =  *VariableNameSize;

  //
  // Check the volatile and HOB variables first
  //
  Status = VariableServiceGetNextInMemoryVariableInternal (
             VariableName,
             VariableGuid,
             &VariablePtr,
             &CurrentVariableInMemory
             );
  if (!EFI_ERROR (Status)) {
    VarNameSize = NameSizeOfVariable (VariablePtr);
    ASSERT (VarNameSize != 0);
    if (VarNameSize <= *VariableNameSize) {
      CopyMem (VariableName, GetVariableNamePtr (VariablePtr), VarNameSize);
      CopyMem (VariableGuid, GetVendorGuidPtr (VariablePtr), sizeof (EFI_GUID));
      Status = EFI_SUCCESS;
    } else {
      Status = EFI_BUFFER_TOO_SMALL;
    }

    *VariableNameSize = VarNameSize;
    return Status;
  } else if (Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "VariableServiceGetNextInMemoryVariableInternal status %r\n", Status));
    return Status;
  }

  if (mNvVariableEmulationMode) {
    return EFI_NOT_FOUND;
  }

  //
  // If VariableName is an empty string or we reached the end of the volatile
  // and HOB variables, get the first variable from the first NV storage
  //
  if (VariableName[0] == 0 || (Status == EFI_NOT_FOUND && CurrentVariableInMemory)) {
    if (mVariableModuleGlobal->VariableGlobal.VariableStoresCount <= 0) {
      return EFI_NOT_FOUND;
    }
    ZeroMem ((VOID *) VariableName, *VariableNameSize);
    ZeroMem ((VOID *) VariableGuid, sizeof (EFI_GUID));
    VariableStorage = mVariableModuleGlobal->VariableGlobal.VariableStores[0];
    Status = VariableStorage->GetNextVariableName (
                                VariableStorage,
                                VariableNameSize,
                                VariableName,
                                VariableGuid,
                                &VarAttributes
                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "VariableStorageProtocol->GetNextVariableName status %r\n", Status));
      return Status;
    } else {
      //
      // Don't return this variable if we are at runtime and the variable's attributes do not include
      // the EFI_VARIABLE_RUNTIME_ACCESS flag. If this is true, advance to the next variable
      //
      if (((VarAttributes & EFI_VARIABLE_RUNTIME_ACCESS) != 0) || !AtRuntime ()) {
        return Status;
      }
    }
  }

  VariableStorageSelectorProtocol = mVariableModuleGlobal->VariableGlobal.VariableStorageSelectorProtocol;
  if (VariableStorageSelectorProtocol == NULL) {
    ASSERT (mNvVariableEmulationMode == TRUE);
    return EFI_NOT_FOUND;
  }

  SearchComplete = FALSE;
  while (!SearchComplete) {
    Status = VariableStorageSelectorProtocol->GetId (VariableName, VariableGuid, &VariableStorageId);
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    VarNameSize = CallerVariableNameBufferSize;

    for ( Index = 0;
          Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
          Index++) {
      VariableStorage = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
      ZeroMem ((VOID *) &InstanceGuid, sizeof (EFI_GUID));
      Status = VariableStorage->GetId (VariableStorage, &InstanceGuid);
      if (EFI_ERROR (Status)) {
        return Status;
      }
      if (CompareGuid (&VariableStorageId, &InstanceGuid)) {
        Status = VariableStorage->GetNextVariableName (
                                    VariableStorage,
                                    &VarNameSize,
                                    VariableName,
                                    VariableGuid,
                                    &VarAttributes
                                    );
        if (!EFI_ERROR (Status)) {
          if (VariableExistsInHob (VariableName, VariableGuid)) {
            //
            // Don't return this variable if there is a HOB variable that overrides it
            // advance to the next variable
            //
            break;
          }
          if (((VarAttributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0) && AtRuntime ()) {
            //
            // Don't return this variable if we are at runtime and the variable's attributes do not include
            // the EFI_VARIABLE_RUNTIME_ACCESS flag. If this is true, advance to the next variable
            //
            break;
          }
          goto Done;
        } else if (Status == EFI_NOT_FOUND) {
          //
          // If we reached the end of the variables in the current NV storage
          // get the first variable in the next NV storage
          //
          SearchComplete = TRUE;
          for ( NextIndex = (Index + 1);
                NextIndex < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
                NextIndex++) {
            VarNameSize = CallerVariableNameBufferSize;
            VariableStorage = mVariableModuleGlobal->VariableGlobal.VariableStores[NextIndex];

            ZeroMem ((VOID *) VariableGuid, sizeof (VariableGuid));
            Status = VariableStorage->GetNextVariableName (
                                        VariableStorage,
                                        &VarNameSize,
                                        VariableName,
                                        VariableGuid,
                                        &VarAttributes
                                        );
            if (!EFI_ERROR (Status)) {
              SearchComplete = FALSE;
              if (VariableExistsInHob (VariableName, VariableGuid)) {
                //
                // Don't return this variable if there is a HOB variable that overrides it
                // advance to the next variable
                //
                break;
              }
              if (((VarAttributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0) && AtRuntime ()) {
                //
                // Don't return this variable if we are at runtime and the variable's attributes do not include
                // the EFI_VARIABLE_RUNTIME_ACCESS flag. If this is true, advance to the next variable
                //
                break;
              }
            } else if (Status == EFI_NOT_FOUND) {
              //
              // This variable store is completely empty, attempt the next one
              //
              continue;
            } else {
              DEBUG ((DEBUG_ERROR, "VariableStorageProtocol->GetNextVariableName status %r\n", Status));
              return Status;
            }
            DEBUG ((DEBUG_INFO, "Variable.c: Status returned from the variable storage protocol is %r\n", Status));
            goto Done;
          }
          //
          // Either this is the last variable
          // or we found a variable that is duplicated in the HOB
          // or we found a variable that isn't runtime accessible when we are at runtime
          //
          // depending on the value of SearchComplete, we will either try the next variable after the one we just found
          // or there are no variables left, and we exit the while loop and return EFI_NOT_FOUND
          //
          break;
        } else {
          DEBUG ((DEBUG_ERROR, "VariableStorageProtocol->GetNextVariableName status %r\n", Status));
          return Status;
        }
      }
    }
  }

  return EFI_NOT_FOUND;

Done:
  *VariableNameSize = VarNameSize;

  if (CallerVariableNameBufferSize < VarNameSize) {
    return EFI_BUFFER_TOO_SMALL;
  }

  return EFI_SUCCESS;
}

/**

  This code Finds the Next available variable.

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode. This function will do basic validation, before parse the data.

  @param VariableNameSize           The size of the VariableName buffer. The size must be large
                                    enough to fit input string supplied in VariableName buffer.
  @param VariableName               Pointer to variable name.
  @param VendorGuid                 Variable Vendor Guid.

  @retval EFI_SUCCESS               The function completed successfully.
  @retval EFI_NOT_FOUND             The next variable was not found.
  @retval EFI_BUFFER_TOO_SMALL      The VariableNameSize is too small for the result.
                                    VariableNameSize has been updated with the size needed to complete the request.
  @retval EFI_INVALID_PARAMETER     VariableNameSize is NULL.
  @retval EFI_INVALID_PARAMETER     VariableName is NULL.
  @retval EFI_INVALID_PARAMETER     VendorGuid is NULL.
  @retval EFI_INVALID_PARAMETER     The input values of VariableName and VendorGuid are not a name and
                                    GUID of an existing variable.
  @retval EFI_INVALID_PARAMETER     Null-terminator is not found in the first VariableNameSize bytes of
                                    the input VariableName buffer.

**/
EFI_STATUS
EFIAPI
VariableServiceGetNextVariableName (
  IN OUT  UINTN             *VariableNameSize,
  IN OUT  CHAR16            *VariableName,
  IN OUT  EFI_GUID          *VendorGuid
  )
{
  UINTN                   VariableNameBufferSize;
  EFI_STATUS              Status;
  UINTN                   MaxLen;

  if (VariableNameSize == NULL || VariableName == NULL || VendorGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Calculate the possible maximum length of name string, including the Null terminator.
  //
  MaxLen = *VariableNameSize / sizeof (CHAR16);
  if ((MaxLen == 0) || (StrnLenS (VariableName, MaxLen) == MaxLen)) {
    //
    // Null-terminator is not found in the first VariableNameSize bytes of the input VariableName buffer,
    // follow spec to return EFI_INVALID_PARAMETER.
    //
    return EFI_INVALID_PARAMETER;
  }

  AcquireLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  VariableNameBufferSize = sizeof (mVariableNameBuffer);
  ZeroMem ((VOID *) &mVariableNameBuffer[0], VariableNameBufferSize);
  StrCpyS (&mVariableNameBuffer[0], VariableNameBufferSize, VariableName);

  Status = VariableServiceGetNextVariableInternal (&VariableNameBufferSize, &mVariableNameBuffer[0], VendorGuid);
  ASSERT (Status != EFI_BUFFER_TOO_SMALL);

  if (!EFI_ERROR (Status)) {
    if (VariableNameBufferSize > *VariableNameSize) {
      *VariableNameSize = VariableNameBufferSize;
      Status = EFI_BUFFER_TOO_SMALL;
    } else {
      StrCpyS (VariableName, *VariableNameSize, &mVariableNameBuffer[0]);
      *VariableNameSize = VariableNameBufferSize;
    }
  }

  ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  return Status;
}

/**
  Load all variables in to the NV cache from the EDKII_VARIABLE_STORAGE_PROTOCOLs

  @param[in]   Global                 Pointer to VARIABLE_GLOBAL structure, including
                                      base of volatile variable storage area, base of
                                      NV variable storage area, and a lock.
  @param[out]  CommandInProgress      TRUE if the command requires asyncronous I/O and has not completed yet.
                                      If this parameter is TRUE, then PtrTrack will not be updated and will
                                      not contain valid data.  Asyncronous I/O should only be required during
                                      OS runtime phase, this return value will be FALSE during all Pre-OS stages.
                                      If CommandInProgress is returned TRUE, then this function will return EFI_SUCCESS
  @param[out]  InProgressInstanceGuid If CommandInProgress is TRUE, this will contain the instance GUID of the Variable
                                      Storage driver that is performing the asyncronous I/O

  @retval EFI_INVALID_PARAMETER If CommandInProgress is NULL or InProgressInstanceGuid is NULL.
  @retval EFI_OUT_OF_RESOURCES  NV Variable cache is full

**/
EFI_STATUS
LoadAllNvVariablesInToCache (
  IN  VARIABLE_GLOBAL         *Global,
  OUT BOOLEAN                 *CommandInProgress,
  OUT EFI_GUID                *InProgressInstanceGuid
  )
{
  VARIABLE_POINTER_TRACK  PtrTrack;
  EFI_GUID                VendorGuid;
  EFI_STATUS              Status;
  UINTN                   VariableNameBufferSize;
  BOOLEAN                 EnumerationComplete;

  ZeroMem ((VOID *) &mVariableNameBuffer[0], sizeof (mVariableNameBuffer));
  ZeroMem ((VOID *) &VendorGuid, sizeof (VendorGuid));
  EnumerationComplete = FALSE;
  AcquireLockOnlyAtBootTime (&Global->VariableServicesLock);
  Status = EFI_SUCCESS;
  while (!EnumerationComplete) {
    VariableNameBufferSize = sizeof (mVariableNameBuffer);
    Status = VariableServiceGetNextVariableInternal (&VariableNameBufferSize, &mVariableNameBuffer[0], &VendorGuid);
    if (!EFI_ERROR (Status)) {
      ZeroMem ((VOID *) &PtrTrack, sizeof (PtrTrack));
      Status = FindVariable (
                 &mVariableNameBuffer[0],
                 &VendorGuid,
                 &PtrTrack,
                 Global,
                 TRUE,
                 CommandInProgress,
                 InProgressInstanceGuid
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "  Variable Driver: Error loading NV cache FindVariable status %r\n", Status));
        goto Done;
      }
      if (*CommandInProgress) {
        Status = EFI_SUCCESS;
        goto Done;
      }
    } else if (Status == EFI_NOT_FOUND) {
      EnumerationComplete = TRUE;
      Status = EFI_SUCCESS;
    } else {
      DEBUG ((DEBUG_INFO, "  Variable Driver: Error loading NV cache GetNextVariable status %r\n", Status));
      goto Done;
    }
  }

Done:
  ReleaseLockOnlyAtBootTime (&Global->VariableServicesLock);
  return Status;
}

/**

  This code sets variable in storage blocks (Volatile or Non-Volatile).

  Caution: This function may receive untrusted input.
  This function may be invoked in SMM mode, and datasize and data are external input.
  This function will do basic validation, before parse the data.
  This function will parse the authentication carefully to avoid security issues, like
  buffer overflow, integer overflow.
  This function will check attribute carefully to avoid authentication bypass.

  @param VariableName                     Name of Variable to be found.
  @param VendorGuid                       Variable vendor GUID.
  @param Attributes                       Attribute value of the variable found
  @param DataSize                         Size of Data found. If size is less than the
                                          data, this value contains the required size.
  @param Data                             Data pointer.
  @param CommandInProgress                TRUE if the command requires asyncronous I/O and has not completed yet.
                                          Asyncronous I/O should only be required during OS runtime phase.  This
                                          parameter is only be used by the SMM Variable Services implementation.
                                          The Runtime DXE implementation always returns FALSE.
  @param InProgressInstanceGuid           If CommandInProgress is TRUE, this will contain the instance GUID of the Variable
                                          Storage driver that is performing the asyncronous I/O

  @return EFI_INVALID_PARAMETER           Invalid parameter.
  @return EFI_SUCCESS                     Set successfully.
  @return EFI_OUT_OF_RESOURCES            Resource not enough to set variable.
  @return EFI_NOT_FOUND                   Not found.
  @return EFI_WRITE_PROTECTED             Variable is read-only.

**/
EFI_STATUS
EFIAPI
VariableServiceSetVariable (
  IN  CHAR16                  *VariableName,
  IN  EFI_GUID                *VendorGuid,
  IN  UINT32                  Attributes,
  IN  UINTN                   DataSize,
  IN  VOID                    *Data,
  OUT BOOLEAN                 *CommandInProgress,
  OUT EFI_GUID                *InProgressInstanceGuid,
  OUT BOOLEAN                 *ReenterFunction
  )
{
  VARIABLE_POINTER_TRACK              Variable;
  EFI_STATUS                          Status;
  UINTN                               PayloadSize;
  EFI_PHYSICAL_ADDRESS                HobVariableBase;
  BOOLEAN                             LoadAllNvVariablesToCache;

  *ReenterFunction = FALSE;
  //
  // Check input parameters.
  //
  if (VariableName == NULL || VariableName[0] == 0 || VendorGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (DataSize != 0 && Data == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check for the reserverd bit in the variable attributes.
  // EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS is deprecated but we still allow
  // the delete operation of common authenticated variable at user physical presence.
  // So leave EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS attribute check to AuthVariableLib
  //
  if ((Attributes & (~(EFI_VARIABLE_ATTRIBUTES_MASK | EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS))) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Make sure that if the runtime bit is set, the boot service bit is also set.
  //
  if ((Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)) == EFI_VARIABLE_RUNTIME_ACCESS) {
    if ((Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) != 0) {
      return EFI_UNSUPPORTED;
    } else {
      return EFI_INVALID_PARAMETER;
    }
  } else if ((Attributes & VARIABLE_ATTRIBUTE_AT_AW) != 0) {
    if (!mVariableModuleGlobal->VariableGlobal.AuthSupport && !mIgnoreAuthCheck) {
      //
      // Authenticated variable writes are not supported.
      //
      return EFI_INVALID_PARAMETER;
    }
  } else if ((Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != 0) {
    if (PcdGet32 (PcdHwErrStorageSize) == 0) {
      //
      // Harware error record variables are not supported on this platform.
      //
      return EFI_INVALID_PARAMETER;
    }
  }

  //
  // EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS and EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS
  // cannot both be set.
  //
  if (((Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS)
     && ((Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)) {
    return EFI_UNSUPPORTED;
  }

  if ((Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS && !mIgnoreAuthCheck) {
    //
    //  If DataSize == AUTHINFO_SIZE and then PayloadSize is 0.
    //  Maybe it's the delete operation of common authenticated variable at user physical presence.
    //
    if (DataSize != AUTHINFO_SIZE) {
      return EFI_UNSUPPORTED;
    }
    PayloadSize = DataSize - AUTHINFO_SIZE;
  } else if ((Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS && !mIgnoreAuthCheck) {
    //
    // Sanity check for EFI_VARIABLE_AUTHENTICATION_2 descriptor.
    //
    if (DataSize < OFFSET_OF_AUTHINFO2_CERT_DATA ||
      ((EFI_VARIABLE_AUTHENTICATION_2 *) Data)->AuthInfo.Hdr.dwLength > DataSize - (OFFSET_OF (EFI_VARIABLE_AUTHENTICATION_2, AuthInfo)) ||
      ((EFI_VARIABLE_AUTHENTICATION_2 *) Data)->AuthInfo.Hdr.dwLength < OFFSET_OF (WIN_CERTIFICATE_UEFI_GUID, CertData)) {
      return EFI_SECURITY_VIOLATION;
    }
    //
    // The MemoryLoadFence() call here is to ensure the above sanity check
    // for the EFI_VARIABLE_AUTHENTICATION_2 descriptor has been completed
    // before the execution of subsequent codes.
    //
    MemoryLoadFence ();
    PayloadSize = DataSize - AUTHINFO2_SIZE (Data);
  } else {
    PayloadSize = DataSize;
  }

  if ((UINTN)(~0) - PayloadSize < StrSize(VariableName)){
    //
    // Prevent whole variable size overflow
    //
    return EFI_INVALID_PARAMETER;
  }

  //
  //  The size of the VariableName, including the Unicode Null in bytes plus
  //  the DataSize is limited to maximum size of PcdGet32 (PcdMaxHardwareErrorVariableSize)
  //  bytes for HwErrRec#### variable.
  //
  if ((Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
    if (StrSize (VariableName) + PayloadSize > PcdGet32 (PcdMaxHardwareErrorVariableSize) - GetVariableHeaderSize ()) {
      return EFI_INVALID_PARAMETER;
    }
  } else {
    //
    //  The size of the VariableName, including the Unicode Null in bytes plus
    //  the DataSize is limited to maximum size of Max(Auth|Volatile)VariableSize bytes.
    //
    if ((Attributes & VARIABLE_ATTRIBUTE_AT_AW) != 0) {
      if (StrSize (VariableName) + PayloadSize > mVariableModuleGlobal->MaxAuthVariableSize - GetVariableHeaderSize ()) {
        DEBUG ((DEBUG_ERROR,
          "%a: Failed to set variable '%s' with Guid %g\n",
          __FUNCTION__, VariableName, VendorGuid));
        DEBUG ((DEBUG_ERROR,
          "NameSize(0x%x) + PayloadSize(0x%x) > "
          "MaxAuthVariableSize(0x%x) - HeaderSize(0x%x)\n",
          StrSize (VariableName), PayloadSize,
          mVariableModuleGlobal->MaxAuthVariableSize,
          GetVariableHeaderSize ()
          ));
        return EFI_INVALID_PARAMETER;
      }
    } else if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
      if (StrSize (VariableName) + PayloadSize > mVariableModuleGlobal->MaxVariableSize - GetVariableHeaderSize ()) {
        DEBUG ((DEBUG_ERROR,
          "%a: Failed to set variable '%s' with Guid %g\n",
          __FUNCTION__, VariableName, VendorGuid));
        DEBUG ((DEBUG_ERROR,
          "NameSize(0x%x) + PayloadSize(0x%x) > "
          "MaxVariableSize(0x%x) - HeaderSize(0x%x)\n",
          StrSize (VariableName), PayloadSize,
          mVariableModuleGlobal->MaxVariableSize,
          GetVariableHeaderSize ()
          ));
        return EFI_INVALID_PARAMETER;
      }
    } else {
      if (StrSize (VariableName) + PayloadSize > mVariableModuleGlobal->MaxVolatileVariableSize - GetVariableHeaderSize ()) {
        DEBUG ((DEBUG_ERROR,
          "%a: Failed to set variable '%s' with Guid %g\n",
          __FUNCTION__, VariableName, VendorGuid));
        DEBUG ((DEBUG_ERROR,
          "NameSize(0x%x) + PayloadSize(0x%x) > "
          "MaxVolatileVariableSize(0x%x) - HeaderSize(0x%x)\n",
          StrSize (VariableName), PayloadSize,
          mVariableModuleGlobal->MaxVolatileVariableSize,
          GetVariableHeaderSize ()
          ));
        return EFI_INVALID_PARAMETER;
      }
    }
  }

  //
  // Check if the variable already exists.
  //
  LoadAllNvVariablesToCache = FALSE;
  if (((Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) ||
      ((Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)) {
    LoadAllNvVariablesToCache = TRUE;
  }
  AcquireLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
  Status = FindVariable (
             VariableName,
             VendorGuid,
             &Variable,
             &mVariableModuleGlobal->VariableGlobal,
             TRUE,
             CommandInProgress,
             InProgressInstanceGuid
             );
  ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
  if (!EFI_ERROR (Status)) {
    if (*CommandInProgress) {
      //
      // Exit, allow async I/O to read the variable and put it in to NV cache
      //
      *ReenterFunction = TRUE;
      return EFI_SUCCESS;
    }
    if (((Variable.CurrPtr->Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0) && AtRuntime ()) {
      return EFI_WRITE_PROTECTED;
    }
    if (Attributes != 0 && (Attributes & (~EFI_VARIABLE_APPEND_WRITE)) != Variable.CurrPtr->Attributes) {
      //
      // If a preexisting variable is rewritten with different attributes, SetVariable() shall not
      // modify the variable and shall return EFI_INVALID_PARAMETER. Two exceptions to this rule:
      // 1. No access attributes specified
      // 2. The only attribute differing is EFI_VARIABLE_APPEND_WRITE
      //
      DEBUG ((EFI_D_INFO, "  Variable Driver: Rewritten a preexisting variable(0x%08x) with different attributes(0x%08x) - %g:%s\n", Variable.CurrPtr->Attributes, Attributes, VendorGuid, VariableName));
      return EFI_INVALID_PARAMETER;
    }
    if (((Variable.CurrPtr->Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) ||
        ((Variable.CurrPtr->Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) == EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)) {
      LoadAllNvVariablesToCache = TRUE;
    }
  } else if (Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "  Variable Driver: Error loading Variable to NV cache: %r\n", Status));
    return Status;
  }
  if (IsAuthenticatedVariable (VariableName, VendorGuid)) {
    LoadAllNvVariablesToCache = TRUE;
  }
  //
  // If FindVariable returns a HOB variable, make sure that if a variable
  // exists in NV that the NV copy of the variable is loaded in to the NV cache
  //
  if ((Variable.CurrPtr != NULL) &&
      (mVariableModuleGlobal->VariableGlobal.HobVariableBase != 0) &&
      (Variable.StartPtr == GetStartPointer ((VARIABLE_STORE_HEADER *) (UINTN) mVariableModuleGlobal->VariableGlobal.HobVariableBase))
     ) {
    HobVariableBase                                       = mVariableModuleGlobal->VariableGlobal.HobVariableBase;
    mVariableModuleGlobal->VariableGlobal.HobVariableBase = 0;
    AcquireLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
    Status = FindVariable (
               VariableName,
               VendorGuid,
               &Variable,
               &mVariableModuleGlobal->VariableGlobal,
               TRUE,
               CommandInProgress,
               InProgressInstanceGuid
               );
    ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
    mVariableModuleGlobal->VariableGlobal.HobVariableBase = HobVariableBase;
    if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
      DEBUG ((DEBUG_INFO, "  Variable Driver: Error loading Variable to NV cache: %r\n", Status));
      return Status;
    }
    if (!EFI_ERROR (Status) && *CommandInProgress) {
      //
      // Exit, allow async I/O to read the variable and put it in to NV cache
      //
      *ReenterFunction = TRUE;
      return EFI_SUCCESS;
    }
  }
  //
  // If any variable authentication flows are involved, and we cannot do
  // syncronous I/O, then we need to make sure that all variables are already
  // loaded in to memory before proceeding with the authentication flow
  //
  if (VariableStorageAnyAsyncIoRequired () && LoadAllNvVariablesToCache && !mIgnoreAuthCheck) {
    DEBUG ((DEBUG_INFO, "  Variable Driver: Async with Authentication detected, loading the entire NV cache\n"));
    Status = LoadAllNvVariablesInToCache (
               &mVariableModuleGlobal->VariableGlobal,
               CommandInProgress,
               InProgressInstanceGuid
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    if (*CommandInProgress) {
      //
      // Exit, allow async I/O to read the variable and put it in to NV cache
      //
      *ReenterFunction = TRUE;
      return EFI_SUCCESS;
    }
  }

  //
  // Special Handling for MOR Lock variable.
  //
  Status = SetVariableCheckHandlerMor (VariableName, VendorGuid, Attributes, PayloadSize, (VOID *) ((UINTN) Data + DataSize - PayloadSize));
  if (Status == EFI_ALREADY_STARTED) {
    //
    // EFI_ALREADY_STARTED means the SetVariable() action is handled inside of SetVariableCheckHandlerMor().
    // Variable driver can just return SUCCESS.
    //
    if (mCommandInProgress) {
      *CommandInProgress = mCommandInProgress;
      CopyMem (InProgressInstanceGuid, &mInProgressInstanceGuid, sizeof (EFI_GUID));
      mCommandInProgress = FALSE;
      ZeroMem (&mInProgressInstanceGuid, sizeof (EFI_GUID));
    }
    return EFI_SUCCESS;
  }
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = VarCheckLibSetVariableCheck (VariableName, VendorGuid, Attributes, PayloadSize, (VOID *) ((UINTN) Data + DataSize - PayloadSize), mRequestSource);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  AcquireLockOnlyAtBootTime(&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  //
  // Consider reentrant in MCA/INIT/NMI. It needs be reupdated.
  //
  InterlockedIncrement (&mVariableModuleGlobal->VariableGlobal.ReentrantState);

  if (!FeaturePcdGet (PcdUefiVariableDefaultLangDeprecate)) {
    //
    // Hook the operation of setting PlatformLangCodes/PlatformLang and LangCodes/Lang.
    //
    Status = AutoUpdateLangVariable (VariableName, Data, DataSize);
    if (EFI_ERROR (Status)) {
      //
      // The auto update operation failed, directly return to avoid inconsistency between PlatformLang and Lang.
      //
      goto Done;
    }
  }

  if (mVariableModuleGlobal->VariableGlobal.AuthSupport && !mIgnoreAuthCheck) {
    Status = AuthVariableLibProcessVariable (VariableName, VendorGuid, Data, DataSize, Attributes);
  } else {
    Status = UpdateVariable (VariableName, VendorGuid, Data, DataSize, Attributes, 0, 0, &Variable, NULL);
  }
  if (mCommandInProgress) {
    *CommandInProgress = mCommandInProgress;
    CopyMem (InProgressInstanceGuid, &mInProgressInstanceGuid, sizeof (EFI_GUID));
    mCommandInProgress = FALSE;
    ZeroMem (&mInProgressInstanceGuid, sizeof (EFI_GUID));
  }

Done:
  InterlockedDecrement (&mVariableModuleGlobal->VariableGlobal.ReentrantState);
  ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  if (!AtRuntime ()) {
    if (!EFI_ERROR (Status)) {
      SecureBootHook (
        VariableName,
        VendorGuid
        );
    }
  }

  return Status;
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
VariableServiceQueryVariableInfoInternal (
  IN  UINT32                 Attributes,
  OUT UINT64                 *MaximumVariableStorageSize,
  OUT UINT64                 *RemainingVariableStorageSize,
  OUT UINT64                 *MaximumVariableSize
  )
{
  VARIABLE_HEADER                   *Variable;
  VARIABLE_HEADER                   *NextVariable;
  UINT64                            VariableSize;
  VARIABLE_STORE_HEADER             *VariableStoreHeader;
  UINT64                            CommonVariableTotalSize;
  UINT64                            HwErrVariableTotalSize;
  EDKII_VARIABLE_STORAGE_PROTOCOL   *VariableStorageProtocol;
  UINTN                             Index;
  UINT32                            VariableStoreSize;
  UINT32                            VspCommonVariablesTotalSize;
  UINT32                            VspHwErrVariablesTotalSize;
  EFI_STATUS                        Status;
  VARIABLE_POINTER_TRACK            VariablePtrTrack;

  CommonVariableTotalSize = 0;
  HwErrVariableTotalSize = 0;

  if ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) {
    //
    // Query is Volatile related.
    //
    VariableStoreHeader = (VARIABLE_STORE_HEADER *) ((UINTN) mVariableModuleGlobal->VariableGlobal.VolatileVariableBase);
  } else {
    //
    // Query is Non-Volatile related.
    //
    VariableStoreHeader = mNvVariableCache;
  }

  //
  // Now let's fill *MaximumVariableStorageSize *RemainingVariableStorageSize
  // with the storage size (excluding the storage header size).
  //
  *MaximumVariableStorageSize   = VariableStoreHeader->Size - sizeof (VARIABLE_STORE_HEADER);

  //
  // Harware error record variable needs larger size.
  //
  if ((Attributes & (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_HARDWARE_ERROR_RECORD)) == (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_HARDWARE_ERROR_RECORD)) {
    *MaximumVariableStorageSize = PcdGet32 (PcdHwErrStorageSize);
    *MaximumVariableSize = PcdGet32 (PcdMaxHardwareErrorVariableSize) - GetVariableHeaderSize ();
  } else {
    if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
      if (AtRuntime ()) {
        *MaximumVariableStorageSize = mVariableModuleGlobal->CommonRuntimeVariableSpace;
      } else {
        *MaximumVariableStorageSize = mVariableModuleGlobal->CommonVariableSpace;
      }
    }

    //
    // Let *MaximumVariableSize be Max(Auth|Volatile)VariableSize with the exception of the variable header size.
    //
    if ((Attributes & VARIABLE_ATTRIBUTE_AT_AW) != 0) {
      *MaximumVariableSize = mVariableModuleGlobal->MaxAuthVariableSize - GetVariableHeaderSize ();
    } else if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0) {
      *MaximumVariableSize = mVariableModuleGlobal->MaxVariableSize - GetVariableHeaderSize ();
    } else {
      *MaximumVariableSize = mVariableModuleGlobal->MaxVolatileVariableSize - GetVariableHeaderSize ();
    }
  }

  //
  // Point to the starting address of the variables.
  //
  Variable = GetStartPointer (VariableStoreHeader);

  if ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) {
    //
    // For Volatile related, walk through the variable store.
    //
    while (IsValidVariableHeader (Variable, GetEndPointer (VariableStoreHeader))) {
      NextVariable = GetNextVariablePtr (Variable);
      VariableSize = (UINT64) (UINTN) NextVariable - (UINT64) (UINTN) Variable;

      if (AtRuntime ()) {
        //
        // We don't take the state of the variables in mind
        // when calculating RemainingVariableStorageSize,
        // since the space occupied by variables not marked with
        // VAR_ADDED is not allowed to be reclaimed in Runtime.
        //
        if ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
          HwErrVariableTotalSize += VariableSize;
        } else {
          CommonVariableTotalSize += VariableSize;
        }
      } else {
        //
        // Only care about Variables with State VAR_ADDED, because
        // the space not marked as VAR_ADDED is reclaimable now.
        //
        if (Variable->State == VAR_ADDED) {
          if ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
            HwErrVariableTotalSize += VariableSize;
          } else {
            CommonVariableTotalSize += VariableSize;
          }
        } else if (Variable->State == (VAR_IN_DELETED_TRANSITION & VAR_ADDED)) {
          //
          // If it is a IN_DELETED_TRANSITION variable,
          // and there is not also a same ADDED one at the same time,
          // this IN_DELETED_TRANSITION variable is valid.
          //
          VariablePtrTrack.StartPtr = GetStartPointer (VariableStoreHeader);
          VariablePtrTrack.EndPtr   = GetEndPointer   (VariableStoreHeader);
          Status = FindVariableEx (
                    GetVariableNamePtr (Variable),
                    GetVendorGuidPtr (Variable),
                    FALSE,
                    &VariablePtrTrack
                    );
          if (!EFI_ERROR (Status) && VariablePtrTrack.CurrPtr->State != VAR_ADDED) {
            if ((Variable->Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
              HwErrVariableTotalSize += VariableSize;
            } else {
              CommonVariableTotalSize += VariableSize;
            }
          }
        }
      }

      //
      // Go to the next one.
      //
      Variable = NextVariable;
    }
  } else {
    //
    // For Non Volatile related, call GetStorageUsage() on the EDKII_VARIABLE_STORAGE_PROTOCOLs
    //
    for ( Index = 0;
          Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
          Index++) {
      VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
      Status = VariableStorageProtocol->GetStorageUsage (
                                          VariableStorageProtocol,
                                          AtRuntime (),
                                          &VariableStoreSize,
                                          &VspCommonVariablesTotalSize,
                                          &VspHwErrVariablesTotalSize
                                          );
      ASSERT_EFI_ERROR (Status);
      if (EFI_ERROR (Status)) {
        return Status;
      }
      CommonVariableTotalSize  += VspCommonVariablesTotalSize;
      HwErrVariableTotalSize   += VspHwErrVariablesTotalSize;
    }
  }

  if ((Attributes  & EFI_VARIABLE_HARDWARE_ERROR_RECORD) == EFI_VARIABLE_HARDWARE_ERROR_RECORD){
    *RemainingVariableStorageSize = *MaximumVariableStorageSize - HwErrVariableTotalSize;
  } else {
    if (*MaximumVariableStorageSize < CommonVariableTotalSize) {
      *RemainingVariableStorageSize = 0;
    } else {
      *RemainingVariableStorageSize = *MaximumVariableStorageSize - CommonVariableTotalSize;
    }
  }

  if (*RemainingVariableStorageSize < GetVariableHeaderSize ()) {
    *MaximumVariableSize = 0;
  } else if ((*RemainingVariableStorageSize - GetVariableHeaderSize ()) < *MaximumVariableSize) {
    *MaximumVariableSize = *RemainingVariableStorageSize - GetVariableHeaderSize ();
  }

  return EFI_SUCCESS;
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

  @return EFI_INVALID_PARAMETER         An invalid combination of attribute bits was supplied.
  @return EFI_SUCCESS                   Query successfully.
  @return EFI_UNSUPPORTED               The attribute is not supported on this platform.

**/
EFI_STATUS
EFIAPI
VariableServiceQueryVariableInfo (
  IN  UINT32                 Attributes,
  OUT UINT64                 *MaximumVariableStorageSize,
  OUT UINT64                 *RemainingVariableStorageSize,
  OUT UINT64                 *MaximumVariableSize
  )
{
  EFI_STATUS             Status;

  if(MaximumVariableStorageSize == NULL || RemainingVariableStorageSize == NULL || MaximumVariableSize == NULL || Attributes == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) != 0) {
    //
    //  Deprecated attribute, make this check as highest priority.
    //
    return EFI_UNSUPPORTED;
  }

  if ((Attributes & EFI_VARIABLE_ATTRIBUTES_MASK) == 0) {
    //
    // Make sure the Attributes combination is supported by the platform.
    //
    return EFI_UNSUPPORTED;
  } else if ((Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)) == EFI_VARIABLE_RUNTIME_ACCESS) {
    //
    // Make sure if runtime bit is set, boot service bit is set also.
    //
    return EFI_INVALID_PARAMETER;
  } else if (AtRuntime () && ((Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0)) {
    //
    // Make sure RT Attribute is set if we are in Runtime phase.
    //
    return EFI_INVALID_PARAMETER;
  } else if ((Attributes & (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_HARDWARE_ERROR_RECORD)) == EFI_VARIABLE_HARDWARE_ERROR_RECORD) {
    //
    // Make sure Hw Attribute is set with NV.
    //
    return EFI_INVALID_PARAMETER;
  } else if ((Attributes & VARIABLE_ATTRIBUTE_AT_AW) != 0) {
    if (!mVariableModuleGlobal->VariableGlobal.AuthSupport) {
      //
      // Not support authenticated variable write.
      //
      return EFI_UNSUPPORTED;
    }
  } else if ((Attributes & EFI_VARIABLE_HARDWARE_ERROR_RECORD) != 0) {
    if (PcdGet32 (PcdHwErrStorageSize) == 0) {
      //
      // Not support harware error record variable variable.
      //
      return EFI_UNSUPPORTED;
    }
  }

  AcquireLockOnlyAtBootTime(&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  Status = VariableServiceQueryVariableInfoInternal (
             Attributes,
             MaximumVariableStorageSize,
             RemainingVariableStorageSize,
             MaximumVariableSize
             );

  ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
  return Status;
}

/**
  This function reclaims variable storage if free size is below the threshold.

  Caution: This function may be invoked at SMM mode.
  Care must be taken to make sure not security issue.

**/
VOID
ReclaimForOS (
  VOID
  )
{
  EFI_STATUS                     Status;
  UINTN                          RemainingCommonRuntimeVariableSpace;
  UINTN                          RemainingHwErrVariableSpace;
  UINTN                          CacheOffset;
  BOOLEAN                        CommandInProgress;
  EFI_GUID                       InProgressInstanceGuid;
  STATIC BOOLEAN                 Reclaimed;

  //
  // This function will be called only once at EndOfDxe or ReadyToBoot event.
  //
  if (Reclaimed) {
    return;
  }
  Reclaimed = TRUE;
  Status    = EFI_SUCCESS;

  if (mVariableModuleGlobal->CommonRuntimeVariableSpace < mVariableModuleGlobal->CommonVariableTotalSize) {
    RemainingCommonRuntimeVariableSpace = 0;
  } else {
    RemainingCommonRuntimeVariableSpace = mVariableModuleGlobal->CommonRuntimeVariableSpace - mVariableModuleGlobal->CommonVariableTotalSize;
  }

  RemainingHwErrVariableSpace = PcdGet32 (PcdHwErrStorageSize) - mVariableModuleGlobal->HwErrVariableTotalSize;

  //
  // Check if the free area is below a threshold.
  //
  if (((RemainingCommonRuntimeVariableSpace < mVariableModuleGlobal->MaxVariableSize) ||
       (RemainingCommonRuntimeVariableSpace < mVariableModuleGlobal->MaxAuthVariableSize)) ||
      ((PcdGet32 (PcdHwErrStorageSize) != 0) &&
       (RemainingHwErrVariableSpace < PcdGet32 (PcdMaxHardwareErrorVariableSize)))) {
    CommandInProgress = FALSE;
    Status = Reclaim (
               (EFI_PHYSICAL_ADDRESS) (UINTN) mNvVariableCache,
               &CacheOffset,
               FALSE,
               NULL,
               NULL,
               0,
               &CommandInProgress,
               &InProgressInstanceGuid
               );
    ASSERT_EFI_ERROR (Status);
    ASSERT (!CommandInProgress);
  }
}

/**
  Notifies the core variable driver that the Variable Storage Driver's WriteServiceIsReady() function
  is now returning TRUE instead of FALSE.

  The Variable Storage Driver is required to call this function as quickly as possible.

**/
VOID
EFIAPI
VariableStorageSupportNotifyWriteServiceReady (
  VOID
  )
{
  EFI_STATUS                        Status;
  UINTN                             Index;
  EDKII_VARIABLE_STORAGE_PROTOCOL   *VariableStorageProtocol;
  BOOLEAN                           WriteServiceReady;

  Status            = EFI_SUCCESS;
  WriteServiceReady = TRUE;
  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];
    if (!VariableStorageProtocol->WriteServiceIsReady (VariableStorageProtocol)) {
      WriteServiceReady = FALSE;
      break;
    }
  }
  if (WriteServiceReady && !mVariableModuleGlobal->WriteServiceReady) {
    mVariableModuleGlobal->WriteServiceReady = TRUE;
    Status = VariableWriteServiceInitialize ();
  }
}

/**
  Get maximum variable size, covering both non-volatile and volatile variables.

  @return Maximum variable size.

**/
UINTN
GetMaxVariableSize (
  VOID
  )
{
  UINTN MaxVariableSize;

  MaxVariableSize = GetNonVolatileMaxVariableSize();
  //
  // The condition below fails implicitly if PcdMaxVolatileVariableSize equals
  // the default zero value.
  //
  if (MaxVariableSize < PcdGet32 (PcdMaxVolatileVariableSize)) {
    MaxVariableSize = PcdGet32 (PcdMaxVolatileVariableSize);
  }
  return MaxVariableSize;
}

/**
  Activities that should execute after the initial HOB flush is attempted.

**/
VOID
EFIAPI
PostHobVariableFlushInitialization (
  VOID
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                Handle;
  UINTN                     Index;
  VARIABLE_ENTRY_PROPERTY   *VariableEntry;

  DEBUG ((EFI_D_INFO, "  Variable Driver: Starting initialization after HOB flush.\n"));
  AcquireLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  Status = EFI_SUCCESS;
  ZeroMem (&mAuthContextOut, sizeof (mAuthContextOut));
  if (mVariableModuleGlobal->VariableGlobal.AuthFormat) {
    //
    // Authenticated variable initialize.
    //
    mAuthContextIn.StructSize = sizeof (AUTH_VAR_LIB_CONTEXT_IN);
    mAuthContextIn.MaxAuthVariableSize = mVariableModuleGlobal->MaxAuthVariableSize - GetVariableHeaderSize ();
    Status = AuthVariableLibInitialize (&mAuthContextIn, &mAuthContextOut);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "  Variable Driver: Variable driver will work with auth variable support!\n"));
      mVariableModuleGlobal->VariableGlobal.AuthSupport = TRUE;
      if (mAuthContextOut.AuthVarEntry != NULL) {
        for (Index = 0; Index < mAuthContextOut.AuthVarEntryCount; Index++) {
          VariableEntry = &mAuthContextOut.AuthVarEntry[Index];
          Status = VarCheckLibVariablePropertySet (
                     VariableEntry->Name,
                     VariableEntry->Guid,
                     &VariableEntry->VariableProperty
                     );
          ASSERT_EFI_ERROR (Status);
        }
      }
    } else if (Status == EFI_UNSUPPORTED) {
      DEBUG ((EFI_D_INFO, "  Variable Driver: NOTICE - AuthVariableLibInitialize() returns %r!\n", Status));
      DEBUG ((EFI_D_INFO, "  Variable Driver: Will continue to work without auth variable support!\n"));
      mVariableModuleGlobal->VariableGlobal.AuthSupport = FALSE;
      Status = EFI_SUCCESS;
    }
  }

  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < ARRAY_SIZE (mVariableEntryProperty); Index++) {
      VariableEntry = &mVariableEntryProperty[Index];
      Status = VarCheckLibVariablePropertySet (VariableEntry->Name, VariableEntry->Guid, &VariableEntry->VariableProperty);
      ASSERT_EFI_ERROR (Status);
    }
  }
  ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  //
  // Initialize MOR Lock variable.
  //
  MorLockInit ();

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "  Variable Driver: Variable write service initialization failed. Status = %r\n", Status));
  } else {
    DEBUG ((EFI_D_INFO, "Sending notification that asynchronous write ready operations have completed.\n"));
    Handle = NULL;
    Status = gBS->InstallProtocolInterface (
                    &Handle,
                    &gEdkiiVariableWriteReadyOperationsCompleteGuid,
                    EFI_NATIVE_INTERFACE,
                    NULL
                    );
  }
}

/**
  Initializes variable write service after all variable storage protocols support write.

  @retval EFI_SUCCESS          Function successfully executed.
  @retval Others               Fail to initialize the variable service.

**/
EFI_STATUS
VariableWriteServiceInitialize (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    Handle;

  if (mVariableModuleGlobal->VariableGlobal.HobVariableBase == 0) {
    DEBUG ((EFI_D_INFO, "  Variable Driver: No HOB variables to flush to storage.\n\n"));
    PostHobVariableFlushInitialization ();

    Handle = NULL;
    Status = gBS->InstallProtocolInterface (
                    &Handle,
                    &gEdkiiVariableWriteReadyOperationsCompleteGuid,
                    EFI_NATIVE_INTERFACE,
                    NULL
                    );
    ASSERT_EFI_ERROR (Status);
  } else {
    AcquireLockOnlyAtBootTime(&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
    FlushHobVariableToStorage (NULL, NULL, PostHobVariableFlushInitialization);
    ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
  }

  //
  // Install the Variable Write Architectural protocol
  //
  InstallVariableWriteReady ();

  return EFI_SUCCESS;
}

/**

  @param[in]  NormalVarStorage  Pointer to the normal variable storage header

  @retval the allocated auth variable storage
**/
VOID *
ConvertNormalVarStorageToAuthVarStorage (
  VARIABLE_STORE_HEADER *NormalVarStorage
  )
{
  VARIABLE_HEADER *StartPtr;
  UINT8           *NextPtr;
  VARIABLE_HEADER *EndPtr;
  UINTN           AuthVarStroageSize;
  AUTHENTICATED_VARIABLE_HEADER *AuthStartPtr;
  VARIABLE_STORE_HEADER         *AuthVarStorage;

  AuthVarStroageSize  = sizeof (VARIABLE_STORE_HEADER);
  //
  // Set AuthFormat as FALSE for normal variable storage
  //
  mVariableModuleGlobal->VariableGlobal.AuthFormat = FALSE;

  //
  // Calculate Auth Variable Storage Size
  //
  StartPtr = GetStartPointer (NormalVarStorage);
  EndPtr   = GetEndPointer (NormalVarStorage);
  while (StartPtr < EndPtr) {
    if (StartPtr->State == VAR_ADDED) {
      AuthVarStroageSize = HEADER_ALIGN (AuthVarStroageSize);
      AuthVarStroageSize += sizeof (AUTHENTICATED_VARIABLE_HEADER);
      AuthVarStroageSize += StartPtr->NameSize + GET_PAD_SIZE (StartPtr->NameSize);
      AuthVarStroageSize += StartPtr->DataSize + GET_PAD_SIZE (StartPtr->DataSize);
    }
    StartPtr  = GetNextVariablePtr (StartPtr);
  }

  //
  // Allocate Runtime memory for Auth Variable Storage
  //
  AuthVarStorage = AllocateRuntimeZeroPool (AuthVarStroageSize);
  ASSERT (AuthVarStorage != NULL);
  if (AuthVarStorage == NULL) {
    return NULL;
  }

  //
  // Copy Variable from Normal storage to Auth storage
  //
  StartPtr = GetStartPointer (NormalVarStorage);
  EndPtr   = GetEndPointer (NormalVarStorage);
  AuthStartPtr = (AUTHENTICATED_VARIABLE_HEADER *) GetStartPointer (AuthVarStorage);
  while (StartPtr < EndPtr) {
    if (StartPtr->State == VAR_ADDED) {
      AuthStartPtr = (AUTHENTICATED_VARIABLE_HEADER *) HEADER_ALIGN (AuthStartPtr);
      //
      // Copy Variable Header
      //
      AuthStartPtr->StartId     = StartPtr->StartId;
      AuthStartPtr->State       = StartPtr->State;
      AuthStartPtr->Attributes  = StartPtr->Attributes;
      AuthStartPtr->NameSize    = StartPtr->NameSize;
      AuthStartPtr->DataSize    = StartPtr->DataSize;
      CopyGuid (&AuthStartPtr->VendorGuid, &StartPtr->VendorGuid);
      //
      // Copy Variable Name
      //
      NextPtr = (UINT8 *) (AuthStartPtr + 1);
      CopyMem (NextPtr, GetVariableNamePtr (StartPtr), AuthStartPtr->NameSize);
      //
      // Copy Variable Data
      //
      NextPtr = NextPtr + AuthStartPtr->NameSize + GET_PAD_SIZE (AuthStartPtr->NameSize);
      CopyMem (NextPtr, GetVariableDataPtr (StartPtr), AuthStartPtr->DataSize);
      //
      // Go to next variable
      //
      AuthStartPtr = (AUTHENTICATED_VARIABLE_HEADER *) (NextPtr + AuthStartPtr->DataSize + GET_PAD_SIZE (AuthStartPtr->DataSize));
    }
    StartPtr = GetNextVariablePtr (StartPtr);
  }
  //
  // Update Auth Storage Header
  //
  AuthVarStorage->Format = NormalVarStorage->Format;
  AuthVarStorage->State  = NormalVarStorage->State;
  AuthVarStorage->Size = (UINT32)((UINTN)AuthStartPtr - (UINTN)AuthVarStorage);
  CopyGuid (&AuthVarStorage->Signature, &gEfiAuthenticatedVariableGuid);
  ASSERT (AuthVarStorage->Size <= AuthVarStroageSize);

  //
  // Restore AuthFormat
  //
  mVariableModuleGlobal->VariableGlobal.AuthFormat = TRUE;
  return AuthVarStorage;
}

/**
  Get HOB variable store.

  @param[in] VariableGuid       NV variable store signature.

  @retval EFI_SUCCESS           Function successfully executed.
  @retval EFI_OUT_OF_RESOURCES  Fail to allocate enough memory resource.

**/
EFI_STATUS
GetHobVariableStore (
  IN EFI_GUID                   *VariableGuid
  )
{
  VARIABLE_STORE_HEADER         *VariableStoreHeader;
  UINT64                        VariableStoreLength;
  EFI_HOB_GUID_TYPE             *GuidHob;
  BOOLEAN                       NeedConvertNormalToAuth;

  //
  // Make sure there is no more than one Variable HOB.
  //
  DEBUG_CODE (
    GuidHob = GetFirstGuidHob (&gEfiAuthenticatedVariableGuid);
    if (GuidHob != NULL) {
      if ((GetNextGuidHob (&gEfiAuthenticatedVariableGuid, GET_NEXT_HOB (GuidHob)) != NULL)) {
        DEBUG ((DEBUG_ERROR, "ERROR: Found two Auth Variable HOBs\n"));
        ASSERT (FALSE);
      } else if (GetFirstGuidHob (&gEfiVariableGuid) != NULL) {
        DEBUG ((DEBUG_ERROR, "ERROR: Found one Auth + one Normal Variable HOBs\n"));
        ASSERT (FALSE);
      }
    } else {
      GuidHob = GetFirstGuidHob (&gEfiVariableGuid);
      if (GuidHob != NULL) {
        if ((GetNextGuidHob (&gEfiVariableGuid, GET_NEXT_HOB (GuidHob)) != NULL)) {
          DEBUG ((DEBUG_ERROR, "ERROR: Found two Normal Variable HOBs\n"));
          ASSERT (FALSE);
        }
      }
    }
  );

  //
  // Combinations supported:
  // 1. Normal NV variable store +
  //    Normal HOB variable store
  // 2. Auth NV variable store +
  //    Auth HOB variable store
  // 3. Auth NV variable store +
  //    Normal HOB variable store (code will convert it to Auth Format)
  //
  NeedConvertNormalToAuth = FALSE;
  GuidHob = GetFirstGuidHob (VariableGuid);
  if (GuidHob == NULL && VariableGuid == &gEfiAuthenticatedVariableGuid) {
    //
    // Try getting it from normal variable HOB
    //
    GuidHob = GetFirstGuidHob (&gEfiVariableGuid);
    NeedConvertNormalToAuth = TRUE;
  }
  if (GuidHob != NULL) {
    VariableStoreHeader = GET_GUID_HOB_DATA (GuidHob);
    VariableStoreLength = GuidHob->Header.HobLength - sizeof (EFI_HOB_GUID_TYPE);
    if (GetVariableStoreStatus (VariableStoreHeader) == EfiValid) {
      if (!NeedConvertNormalToAuth) {
        mVariableModuleGlobal->VariableGlobal.HobVariableBase = (EFI_PHYSICAL_ADDRESS) (UINTN) AllocateRuntimeCopyPool ((UINTN) VariableStoreLength, (VOID *) VariableStoreHeader);
      } else {
        mVariableModuleGlobal->VariableGlobal.HobVariableBase = (EFI_PHYSICAL_ADDRESS) (UINTN) ConvertNormalVarStorageToAuthVarStorage ((VOID *) VariableStoreHeader);
      }
      if (mVariableModuleGlobal->VariableGlobal.HobVariableBase == 0) {
        return EFI_OUT_OF_RESOURCES;
      }
    } else {
      DEBUG ((EFI_D_ERROR, "HOB Variable Store header is corrupted!\n"));
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CheckAuthenticatedVariableSupport (
  VOID
  )
{
  EFI_STATUS                        Status;
  UINTN                             Index;
  BOOLEAN                           AuthSupported;
  EDKII_VARIABLE_STORAGE_PROTOCOL   *VariableStorageProtocol;

  for ( Index = 0;
        Index < mVariableModuleGlobal->VariableGlobal.VariableStoresCount;
        Index++) {
    VariableStorageProtocol = mVariableModuleGlobal->VariableGlobal.VariableStores[Index];

    //
    // Determine authenticated variables support. If all of the
    // EDKII_VARIABLE_STORAGE_PROTOCOLs support authenticated variables, enable it. If
    // any of the EDKII_VARIABLE_STORAGE_PROTOCOLs do not support it disable it globally.
    //
    if (mVariableModuleGlobal->VariableGlobal.AuthFormat) {
      AuthSupported = FALSE;
      Status = VariableStorageProtocol->GetAuthenticatedSupport (
                                          VariableStorageProtocol,
                                          &AuthSupported
                                          );
      ASSERT_EFI_ERROR (Status);
      if (EFI_ERROR (Status)) {
        return Status;
      }
      if (!AuthSupported) {
        mVariableModuleGlobal->VariableGlobal.AuthFormat = FALSE;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Initializes variable store area for non-volatile and volatile variable.

  @retval EFI_SUCCESS           Function successfully executed.
  @retval EFI_OUT_OF_RESOURCES  Fail to allocate enough memory resource.

**/
EFI_STATUS
VariableCommonInitialize (
  VOID
  )
{
  EFI_STATUS                      Status;
  VARIABLE_STORE_HEADER           *VolatileVariableStore;
  UINTN                           ScratchSize;
  UINTN                           TotalNonVolatileVariableStorageSize;
  EFI_GUID                        *VariableGuid;

  mVariableModuleGlobal->CommonVariableTotalSize    = 0;
  mVariableModuleGlobal->HwErrVariableTotalSize     = 0;
  mVariableModuleGlobal->VariableGlobal.AuthFormat  = TRUE;
  mVariableModuleGlobal->WriteServiceReady          = FALSE;

  //
  // Allocate memory for volatile variable store, note that there is a scratch space to store scratch data.
  //
  ScratchSize = GetNonVolatileMaxVariableSize ();
  mVariableModuleGlobal->ScratchBufferSize = ScratchSize;
  VolatileVariableStore = AllocateRuntimePool (PcdGet32 (PcdVariableStoreSize) + ScratchSize);
  if (VolatileVariableStore == NULL) {
    FreePool (mVariableModuleGlobal);
    return EFI_OUT_OF_RESOURCES;
  }
  mVariableDataBuffer = AllocateRuntimePool (ScratchSize);
  if (mVariableDataBuffer == NULL) {
    FreePool (mVariableModuleGlobal);
    FreePool (VolatileVariableStore);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Check if all present variable stores support authenticated variables.
  //
  Status = CheckAuthenticatedVariableSupport ();
  ASSERT_EFI_ERROR (Status);

  Status = InitVariableHelpers (mVariableModuleGlobal->VariableGlobal.AuthFormat);
  ASSERT_EFI_ERROR (Status);

  //
  // Init non-volatile variable store.
  //
  Status = InitNonVolatileVariableStore ();
  if (EFI_ERROR (Status)) {
    FreePool (mVariableModuleGlobal);
    FreePool (mVariableDataBuffer);
    FreePool (VolatileVariableStore);
    return Status;
  }

  mVariableModuleGlobal->MaxVolatileVariableSize = ((PcdGet32 (PcdMaxVolatileVariableSize) != 0) ?
                                                    PcdGet32 (PcdMaxVolatileVariableSize) :
                                                    mVariableModuleGlobal->MaxVariableSize
                                                    );

  //
  // Init non-volatile variable cache.
  //
  Status = GetTotalNonVolatileVariableStorageSize (&TotalNonVolatileVariableStorageSize);
  if (!EFI_ERROR (Status)) {
    Status = InitVariableCache (&mNvVariableCache, &TotalNonVolatileVariableStorageSize);
  }
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // mVariableModuleGlobal->VariableGlobal.AuthFormat
  // has been initialized in InitNonVolatileVariableStore().
  //
  if (mVariableModuleGlobal->VariableGlobal.AuthFormat) {
    DEBUG ((EFI_D_VERBOSE, "  Variable Driver: Will work with auth variable format!\n"));
    VariableGuid = &gEfiAuthenticatedVariableGuid;
  } else {
    DEBUG ((EFI_D_INFO, "  Variable Driver: Will work without auth variable support!\n"));
    VariableGuid = &gEfiVariableGuid;
  }

  //
  // Get HOB variable store.
  //
  Status = GetHobVariableStore (VariableGuid);
  if (EFI_ERROR (Status)) {
    FreePool (mNvVariableCache);
    FreePool (mVariableModuleGlobal);
    FreePool (mVariableDataBuffer);
    FreePool (VolatileVariableStore);
    return Status;
  }

  SetMem (VolatileVariableStore, PcdGet32 (PcdVariableStoreSize) + ScratchSize, 0xff);

  //
  // Initialize Variable Specific Data.
  //
  mVariableModuleGlobal->VariableGlobal.VolatileVariableBase = (EFI_PHYSICAL_ADDRESS) (UINTN) VolatileVariableStore;
  mVariableModuleGlobal->VolatileLastVariableOffset = (UINTN) GetStartPointer (VolatileVariableStore) - (UINTN) VolatileVariableStore;

  CopyGuid (&VolatileVariableStore->Signature, VariableGuid);
  VolatileVariableStore->Size        = PcdGet32 (PcdVariableStoreSize);
  VolatileVariableStore->Format      = VARIABLE_STORE_FORMATTED;
  VolatileVariableStore->State       = VARIABLE_STORE_HEALTHY;
  VolatileVariableStore->Reserved    = 0;
  VolatileVariableStore->Reserved1   = 0;

  //
  // Setup the callback to determine when to enable variable writes and to enable NV cache updates
  //
  if (mNvVariableEmulationMode) {
    mVariableModuleGlobal->WriteServiceReady = TRUE;
    Status = VariableWriteServiceInitialize ();

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "  Variable Driver: Variable write service initialization failed. Status = %r\n", Status));
    } else {
      //
      // Install the Variable Write Architectural protocol
      //
      InstallVariableWriteReady ();
    }
  }

  return EFI_SUCCESS;
}
