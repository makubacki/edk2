/** @file
  Pei Core Migration Support

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PeiMain.h"

PEI_CONVERT_POINTER_PPI mPeiConvertPointerPpi = {
  ConvertMigratedPointer
};

EFI_PEI_PPI_DESCRIPTOR  mPeiConvertPointerPpiList = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gPeiConvertPointerPpiGuid,
  &mPeiConvertPointerPpi
};


/**

  Migrate Pointer from the Temporary RAM to PEI installed memory.

  @param Pointer         Pointer to the Pointer needs to be converted.
  @param TempBottom      Base of old Temporary RAM
  @param TempTop         Top of old Temporary RAM
  @param Offset          Offset of new memory to old Temporary RAM.
  @param OffsetPositive  Positive flag of Offset value.

**/
VOID
ConvertPointer (
  IN OUT VOID              **Pointer,
  IN UINTN                 TempBottom,
  IN UINTN                 TempTop,
  IN UINTN                 Offset,
  IN BOOLEAN               OffsetPositive
  )
{
  if (((UINTN) *Pointer < TempTop) &&
    ((UINTN) *Pointer >= TempBottom)) {
    if (OffsetPositive) {
      *Pointer = (VOID *) ((UINTN) *Pointer + Offset);
    } else {
      *Pointer = (VOID *) ((UINTN) *Pointer - Offset);
    }
  }
}

/**
  This interface provides a service to convert a pointer to an address in a pre-memory FV
  to the corresponding address in the permanent memory FV.

  This service is published by the PEI Foundation if the PEI Foundation migrated pre-memory
  FVs to permanent memory. This service is published before temporary RAM is disabled and
  can be used by PEIMs to convert pointer such as code function pointers to the permanent
  memory address before the code in pre-memory is invalidated.

  @param[in]  PeiServices         The pointer to the PEI Services Table.
  @param[in]  This                The pointer to this instance of the PEI_CONVERT_POINTER_PPI.
  @param[in]  Address             The pointer to a pointer that is to be converted from
                                  a pre-memory address in a FV to the corresponding  permanent memory
                                  address in a FV.

  @retval EFI_SUCCESS             The pointer pointed to by Address was modified successfully.
  @retval EFI_NOT_FOUND           The address at the pointer pointed to by Address was not found
                                  in a pre-memory FV address range.
  @retval EFI_INVALID_PARAMETER   Address is NULL or *Address is NULL.

**/
EFI_STATUS
EFIAPI
ConvertMigratedPointer (
  IN CONST EFI_PEI_SERVICES          **PeiServices,
  IN       PEI_CONVERT_POINTER_PPI   *This,
  IN       VOID                      **Address
  )
{
  PEI_CORE_INSTANCE     *PrivateData;
  UINTN                 PreMemoryAddress;
  UINTN                 Index;
  UINTN                 Offset;
  BOOLEAN               OffsetPositive;

  if (Address == NULL || *Address == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  PrivateData = PEI_CORE_INSTANCE_FROM_PS_THIS (PeiServices);

  PreMemoryAddress = (UINTN) *Address;

  ASSERT (PrivateData->MigrationMap.Count <= PrivateData->FvCount);
  for (Index = 0; Index < PrivateData->MigrationMap.Count; Index++) {
    if (PrivateData->MigrationMap.Entry[Index].PostMemoryBase > PrivateData->MigrationMap.Entry[Index].PreMemoryBase) {
      Offset = (UINTN) (PrivateData->MigrationMap.Entry[Index].PostMemoryBase - PrivateData->MigrationMap.Entry[Index].PreMemoryBase);
      OffsetPositive = TRUE;
    } else {
      Offset = (UINTN) (PrivateData->MigrationMap.Entry[Index].PreMemoryBase - PrivateData->MigrationMap.Entry[Index].PostMemoryBase);
      OffsetPositive = FALSE;
    }

    if ((PreMemoryAddress >= (UINTN) PrivateData->MigrationMap.Entry[Index].PreMemoryBase) &&
        (PreMemoryAddress < (PreMemoryAddress + PrivateData->MigrationMap.Entry[Index].PreMemoryLength))) {
      ConvertPointer (
        Address,
        (UINTN) PrivateData->MigrationMap.Entry[Index].PreMemoryBase,
        (UINTN) PrivateData->MigrationMap.Entry[Index].PreMemoryBase + PrivateData->MigrationMap.Entry[Index].PreMemoryLength,
        Offset,
        OffsetPositive
        );
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**

  Migrate Pointer in ranges of the Temporary RAM to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of Temporary RAM, the stack location and the BFV location.
  @param PrivateData     Pointer to PeiCore's private data structure.
  @param Pointer         Pointer to the Pointer needs to be converted.

**/
VOID
ConvertPointerInRanges (
  IN CONST EFI_SEC_PEI_HAND_OFF  *SecCoreData,
  IN PEI_CORE_INSTANCE           *PrivateData,
  IN OUT VOID                    **Pointer
  )
{
  UINT8                 IndexHole;

  if (PrivateData->MemoryPages.Size != 0) {
    //
    // Convert PPI pointer in old memory pages
    // It needs to be done before Convert PPI pointer in old Heap
    //
    ConvertPointer (
      Pointer,
      (UINTN)PrivateData->MemoryPages.Base,
      (UINTN)PrivateData->MemoryPages.Base + PrivateData->MemoryPages.Size,
      PrivateData->MemoryPages.Offset,
      PrivateData->MemoryPages.OffsetPositive
      );
  }

  //
  // Convert PPI pointer in old Heap
  //
  ConvertPointer (
    Pointer,
    (UINTN)SecCoreData->PeiTemporaryRamBase,
    (UINTN)SecCoreData->PeiTemporaryRamBase + SecCoreData->PeiTemporaryRamSize,
    PrivateData->HeapOffset,
    PrivateData->HeapOffsetPositive
    );

  //
  // Convert PPI pointer in old Stack
  //
  ConvertPointer (
    Pointer,
    (UINTN)SecCoreData->StackBase,
    (UINTN)SecCoreData->StackBase + SecCoreData->StackSize,
    PrivateData->StackOffset,
    PrivateData->StackOffsetPositive
    );

  //
  // Convert PPI pointer in old TempRam Hole
  //
  for (IndexHole = 0; IndexHole < HOLE_MAX_NUMBER; IndexHole ++) {
    if (PrivateData->HoleData[IndexHole].Size == 0) {
      continue;
    }

    ConvertPointer (
      Pointer,
      (UINTN)PrivateData->HoleData[IndexHole].Base,
      (UINTN)PrivateData->HoleData[IndexHole].Base + PrivateData->HoleData[IndexHole].Size,
      PrivateData->HoleData[IndexHole].Offset,
      PrivateData->HoleData[IndexHole].OffsetPositive
      );
  }
}

/**

  Migrate Single PPI Pointer from the Temporary RAM to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of Temporary RAM, the stack location and the BFV location.
  @param PrivateData     Pointer to PeiCore's private data structure.
  @param PpiPointer      Pointer to Ppi

**/
VOID
ConvertSinglePpiPointer (
  IN CONST EFI_SEC_PEI_HAND_OFF  *SecCoreData,
  IN PEI_CORE_INSTANCE           *PrivateData,
  IN PEI_PPI_LIST_POINTERS       *PpiPointer
  )
{
  //
  // 1. Convert the pointer to the PPI descriptor from the old TempRam
  //    to the relocated physical memory.
  // It (for the pointer to the PPI descriptor) needs to be done before 2 (for
  // the pointer to the GUID) and 3 (for the pointer to the PPI interface structure).
  //
  ConvertPointerInRanges (SecCoreData, PrivateData, &PpiPointer->Raw);
  //
  // 2. Convert the pointer to the GUID in the PPI or NOTIFY descriptor
  //    from the old TempRam to the relocated physical memory.
  //
  ConvertPointerInRanges (SecCoreData, PrivateData, (VOID **) &PpiPointer->Ppi->Guid);
  //
  // 3. Convert the pointer to the PPI interface structure in the PPI descriptor
  //    from the old TempRam to the relocated physical memory.
  //
  ConvertPointerInRanges (SecCoreData, PrivateData, (VOID **) &PpiPointer->Ppi->Ppi);
}

/**

  Migrate PPI Pointers from the Temporary RAM to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of Temporary RAM, the stack location and the BFV location.
  @param PrivateData     Pointer to PeiCore's private data structure.

**/
VOID
ConvertPpiPointers (
  IN CONST EFI_SEC_PEI_HAND_OFF  *SecCoreData,
  IN PEI_CORE_INSTANCE           *PrivateData
  )
{
  UINT8                 Index;

  //
  // Convert normal PPIs.
  //
  for (Index = 0; Index < PrivateData->PpiData.PpiList.CurrentCount; Index++) {
    ConvertSinglePpiPointer (
      SecCoreData,
      PrivateData,
      &PrivateData->PpiData.PpiList.PpiPtrs[Index]
      );
  }

  //
  // Convert Callback Notification PPIs.
  //
  for (Index = 0; Index < PrivateData->PpiData.CallbackNotifyList.CurrentCount; Index++) {
    ConvertSinglePpiPointer (
      SecCoreData,
      PrivateData,
      &PrivateData->PpiData.CallbackNotifyList.NotifyPtrs[Index]
      );
  }

  //
  // Convert Dispatch Notification PPIs.
  //
  for (Index = 0; Index < PrivateData->PpiData.DispatchNotifyList.CurrentCount; Index++) {
    ConvertSinglePpiPointer (
      SecCoreData,
      PrivateData,
      &PrivateData->PpiData.DispatchNotifyList.NotifyPtrs[Index]
      );
  }
}

/**

  Migrate memory pages allocated in pre-memory phase.
  Copy memory pages at temporary heap top to permanent heap top.

  @param[in] Private                Pointer to the private data passed in from caller.
  @param[in] TemporaryRamMigrated   Temporary RAM has been migrated to permanent memory.

**/
VOID
MigrateMemoryPages (
  IN PEI_CORE_INSTANCE      *Private,
  IN BOOLEAN                TemporaryRamMigrated
  )
{
  EFI_PHYSICAL_ADDRESS      NewMemPagesBase;
  EFI_PHYSICAL_ADDRESS      MemPagesBase;

  Private->MemoryPages.Size = (UINTN) (Private->HobList.HandoffInformationTable->EfiMemoryTop -
                                       Private->HobList.HandoffInformationTable->EfiFreeMemoryTop);
  if (Private->MemoryPages.Size == 0) {
    //
    // No any memory page allocated in pre-memory phase.
    //
    return;
  }
  Private->MemoryPages.Base = Private->HobList.HandoffInformationTable->EfiFreeMemoryTop;

  ASSERT (Private->MemoryPages.Size <= Private->FreePhysicalMemoryTop);
  NewMemPagesBase = Private->FreePhysicalMemoryTop - Private->MemoryPages.Size;
  NewMemPagesBase &= ~(UINT64)EFI_PAGE_MASK;
  ASSERT (NewMemPagesBase >= Private->PhysicalMemoryBegin);
  //
  // Copy memory pages at temporary heap top to permanent heap top.
  //
  if (TemporaryRamMigrated) {
    //
    // Memory pages at temporary heap top has been migrated to permanent heap,
    // Here still needs to copy them from permanent heap to permanent heap top.
    //
    MemPagesBase = Private->MemoryPages.Base;
    if (Private->HeapOffsetPositive) {
      MemPagesBase += Private->HeapOffset;
    } else {
      MemPagesBase -= Private->HeapOffset;
    }
    CopyMem ((VOID *)(UINTN)NewMemPagesBase, (VOID *)(UINTN)MemPagesBase, Private->MemoryPages.Size);
  } else {
    CopyMem ((VOID *)(UINTN)NewMemPagesBase, (VOID *)(UINTN)Private->MemoryPages.Base, Private->MemoryPages.Size);
  }

  if (NewMemPagesBase >= Private->MemoryPages.Base) {
    Private->MemoryPages.OffsetPositive = TRUE;
    Private->MemoryPages.Offset = (UINTN)(NewMemPagesBase - Private->MemoryPages.Base);
  } else {
    Private->MemoryPages.OffsetPositive = FALSE;
    Private->MemoryPages.Offset = (UINTN)(Private->MemoryPages.Base - NewMemPagesBase);
  }

  DEBUG ((DEBUG_INFO, "Pages Offset = 0x%lX\n", (UINT64) Private->MemoryPages.Offset));

  Private->FreePhysicalMemoryTop = NewMemPagesBase;
}

/**

  Migrate MemoryBaseAddress in memory allocation HOBs
  from the Temporary RAM to PEI installed memory.

  @param[in] PrivateData        Pointer to PeiCore's private data structure.

**/
VOID
ConvertMemoryAllocationHobs (
  IN PEI_CORE_INSTANCE          *PrivateData
  )
{
  EFI_PEI_HOB_POINTERS          Hob;
  EFI_HOB_MEMORY_ALLOCATION     *MemoryAllocationHob;
  EFI_PHYSICAL_ADDRESS          OldMemPagesBase;
  UINTN                         OldMemPagesSize;

  if (PrivateData->MemoryPages.Size == 0) {
    //
    // No any memory page allocated in pre-memory phase.
    //
    return;
  }

  OldMemPagesBase = PrivateData->MemoryPages.Base;
  OldMemPagesSize = PrivateData->MemoryPages.Size;

  MemoryAllocationHob = NULL;
  Hob.Raw = GetFirstHob (EFI_HOB_TYPE_MEMORY_ALLOCATION);
  while (Hob.Raw != NULL) {
    MemoryAllocationHob = (EFI_HOB_MEMORY_ALLOCATION *) Hob.Raw;
    if ((MemoryAllocationHob->AllocDescriptor.MemoryBaseAddress >= OldMemPagesBase) &&
        (MemoryAllocationHob->AllocDescriptor.MemoryBaseAddress < (OldMemPagesBase + OldMemPagesSize))
        ) {
      if (PrivateData->MemoryPages.OffsetPositive) {
        MemoryAllocationHob->AllocDescriptor.MemoryBaseAddress += PrivateData->MemoryPages.Offset;
      } else {
        MemoryAllocationHob->AllocDescriptor.MemoryBaseAddress -= PrivateData->MemoryPages.Offset;
      }
    }

    Hob.Raw = GET_NEXT_HOB (Hob);
    Hob.Raw = GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, Hob.Raw);
  }
}

/**
  Migrate the base address in firmware volume allocation HOBs
  from Temporary RAM to permanent memory.

  @param[in] PrivateData      Pointer to PeiCore's private data structure.
  @param[in] OrgFvHandle      Address of FV Handle in Temporary RAM.
  @param[in] FvHandle         Address of FV Handle in permanent memory.

**/
VOID
ConvertFvHob (
  IN PEI_CORE_INSTANCE          *PrivateData,
  IN UINTN                      OrgFvHandle,
  IN UINTN                      FvHandle
  )
{
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_HOB_FIRMWARE_VOLUME     *FirmwareVolumeHob;
  EFI_HOB_FIRMWARE_VOLUME2    *FirmwareVolume2Hob;
  EFI_HOB_FIRMWARE_VOLUME3    *FirmwareVolume3Hob;

  DEBUG ((DEBUG_VERBOSE, "Converting base addresses in FV HOBs to permanent memory addresses.\n"));

  for (Hob.Raw = GetHobList (); !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV) {
      FirmwareVolumeHob = Hob.FirmwareVolume;
      if (FirmwareVolumeHob->BaseAddress == OrgFvHandle) {
        FirmwareVolumeHob->BaseAddress = FvHandle;
      }
    } else if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV2) {
      FirmwareVolume2Hob = Hob.FirmwareVolume2;
      if (FirmwareVolume2Hob->BaseAddress == OrgFvHandle) {
        FirmwareVolume2Hob->BaseAddress = FvHandle;
      }
    } else if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV3) {
      FirmwareVolume3Hob = Hob.FirmwareVolume3;
      if (FirmwareVolume3Hob->BaseAddress == OrgFvHandle) {
        FirmwareVolume3Hob->BaseAddress = FvHandle;
      }
    }
  }
}

/**
  Removes any FV HOBs whose base address is not in PEI installed memory.

  @param[in] Private          Pointer to PeiCore's private data structure.

**/
VOID
RemoveFvHobsInTemporaryMemory (
  IN PEI_CORE_INSTANCE        *Private
  )
{
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_HOB_FIRMWARE_VOLUME     *FirmwareVolumeHob;

  DEBUG ((DEBUG_VERBOSE, "Removing FVs in FV HOB not already migrated to permanent memory.\n"));

  for (Hob.Raw = GetHobList (); !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV || GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV2 || GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV3) {
      FirmwareVolumeHob = Hob.FirmwareVolume;
      DEBUG ((DEBUG_VERBOSE, "  Found FV HOB.\n"));
      DEBUG ((
          DEBUG_VERBOSE,
          "    BA=%016lx  L=%016lx\n",
          FirmwareVolumeHob->BaseAddress,
          FirmwareVolumeHob->Length
          ));
      if (!(((EFI_PHYSICAL_ADDRESS) (UINTN) FirmwareVolumeHob->BaseAddress >= Private->PhysicalMemoryBegin) &&
          (((EFI_PHYSICAL_ADDRESS) (UINTN) FirmwareVolumeHob->BaseAddress + (FirmwareVolumeHob->Length - 1)) < Private->FreePhysicalMemoryTop))) {
        DEBUG ((DEBUG_VERBOSE, "      Removing FV HOB to an FV in T-RAM (the FV was not migrated).\n"));
        Hob.Header->HobType = EFI_HOB_TYPE_UNUSED;
      }
    }
  }
}

/**
  Migrate PPI related pointers to addresses within pre-memory FVs to the corresponding
  address to the FV in permanent memory.

  The descriptor pointer, GUID pointer, and PPI interface (or notification function for Notifies)
  is updated in that order to the permanent memory address.

  @param[in] PrivateData      Pointer to PeiCore's private data structure.
  @param[in] OrgFvHandle      Address of FV Handle in Temporary RAM.
  @param[in] FvHandle         Address of FV Handle in permanent memory.
  @param[in] FvSize           Size of the FV.

**/
VOID
ConvertPpiPointersFv (
  IN  PEI_CORE_INSTANCE       *PrivateData,
  IN  UINTN                   OrgFvHandle,
  IN  UINTN                   FvHandle,
  IN  UINTN                   FvSize
  )
{
  UINT8                             Index;
  UINTN                             Offset;
  BOOLEAN                           OffsetPositive;
  EFI_PEI_FIRMWARE_VOLUME_INFO_PPI  *FvInfoPpi;
  UINT8                             GuidIndex;
  EFI_GUID                          *Guid;
  EFI_GUID                          *GuidCheckList[2];

  GuidCheckList[0] = &gEfiPeiFirmwareVolumeInfoPpiGuid;
  GuidCheckList[1] = &gEfiPeiFirmwareVolumeInfo2PpiGuid;

  if (FvHandle > OrgFvHandle) {
    OffsetPositive = TRUE;
    Offset = FvHandle - OrgFvHandle;
  } else {
    OffsetPositive = FALSE;
    Offset = OrgFvHandle - FvHandle;
  }

  DEBUG ((DEBUG_VERBOSE, "Converting PPI pointers in FV.\n"));
  DEBUG ((
    DEBUG_VERBOSE,
    "  OrgFvHandle at 0x%08x. FvHandle at 0x%08x. FvSize = 0x%x\n",
    (UINTN) OrgFvHandle,
    (UINTN) FvHandle,
    FvSize
    ));
  DEBUG ((
    DEBUG_VERBOSE,
    "    OrgFvHandle range: 0x%08x - 0x%08x\n",
    OrgFvHandle,
    OrgFvHandle + FvSize
    ));

  for (Index = 0; Index < PrivateData->PpiData.CallbackNotifyList.CurrentCount; Index++) {
      ConvertPointer (
        (VOID **) &PrivateData->PpiData.CallbackNotifyList.NotifyPtrs[Index].Raw,
        OrgFvHandle,
        OrgFvHandle + FvSize,
        Offset,
        OffsetPositive
        );
      ConvertPointer (
        (VOID **) &PrivateData->PpiData.CallbackNotifyList.NotifyPtrs[Index].Notify->Guid,
        OrgFvHandle,
        OrgFvHandle + FvSize,
        Offset,
        OffsetPositive
        );
      ConvertPointer (
        (VOID **) &PrivateData->PpiData.CallbackNotifyList.NotifyPtrs[Index].Notify->Notify,
        OrgFvHandle,
        OrgFvHandle + FvSize,
        Offset,
        OffsetPositive
        );
  }

  for (Index = 0; Index < PrivateData->PpiData.DispatchNotifyList.CurrentCount; Index++) {
    ConvertPointer (
      (VOID **) &PrivateData->PpiData.DispatchNotifyList.NotifyPtrs[Index].Raw,
      OrgFvHandle,
      OrgFvHandle + FvSize,
      Offset,
      OffsetPositive
      );
    ConvertPointer (
      (VOID **) &PrivateData->PpiData.DispatchNotifyList.NotifyPtrs[Index].Notify->Guid,
      OrgFvHandle,
      OrgFvHandle + FvSize,
      Offset,
      OffsetPositive
      );
    ConvertPointer (
      (VOID **) &PrivateData->PpiData.DispatchNotifyList.NotifyPtrs[Index].Notify->Notify,
      OrgFvHandle,
      OrgFvHandle + FvSize,
      Offset,
      OffsetPositive
      );
  }

  for (Index = 0; Index < PrivateData->PpiData.PpiList.CurrentCount; Index++) {
    ConvertPointer (
      (VOID **) &PrivateData->PpiData.PpiList.PpiPtrs[Index].Raw,
      OrgFvHandle,
      OrgFvHandle + FvSize,
      Offset,
      OffsetPositive
      );
    ConvertPointer (
      (VOID **) &PrivateData->PpiData.PpiList.PpiPtrs[Index].Ppi->Guid,
      OrgFvHandle,
      OrgFvHandle + FvSize,
      Offset,
      OffsetPositive
      );
    ConvertPointer (
      (VOID **) &PrivateData->PpiData.PpiList.PpiPtrs[Index].Ppi->Ppi,
      OrgFvHandle,
      OrgFvHandle + FvSize,
      Offset,
      OffsetPositive
      );

    //
    // Update the FvInfo pointer in the FvInfoPpi instance for the migrated FV
    //
    Guid = PrivateData->PpiData.PpiList.PpiPtrs[Index].Ppi->Guid;
    for (GuidIndex = 0; GuidIndex < ARRAY_SIZE (GuidCheckList); ++GuidIndex) {
      //
      // Don't use CompareGuid function here for performance reasons.
      // Instead we compare the GUID as INT32 at a time and branch
      // on the first failed comparison.
      //
      if ((((INT32 *) Guid)[0] == ((INT32 *) GuidCheckList[GuidIndex])[0]) &&
          (((INT32 *) Guid)[1] == ((INT32 *) GuidCheckList[GuidIndex])[1]) &&
          (((INT32 *) Guid)[2] == ((INT32 *) GuidCheckList[GuidIndex])[2]) &&
          (((INT32 *) Guid)[3] == ((INT32 *) GuidCheckList[GuidIndex])[3])) {
        FvInfoPpi = PrivateData->PpiData.PpiList.PpiPtrs[Index].Ppi->Ppi;
        DEBUG ((DEBUG_VERBOSE, "      FvInfo: %p -> ", FvInfoPpi->FvInfo));
        if ((UINTN) FvInfoPpi->FvInfo == OrgFvHandle) {
          ConvertPointer (
            (VOID **) &FvInfoPpi->FvInfo,
            OrgFvHandle,
            OrgFvHandle + FvSize,
            Offset,
            OffsetPositive
            );
          DEBUG ((DEBUG_VERBOSE, "%p", FvInfoPpi->FvInfo));
        }
        DEBUG ((DEBUG_VERBOSE, "\n"));
        break;
      }
    }
  }
}

/**
  Migrate PPI related pointers to the PEI_CORE image in pre-memory to the corresponding
  address in the PEI_CORE image in permanent memory.

  The descriptor pointer, GUID pointer, and PPI interface (or notification function for Notifies)
  is updated in that order to the permanent memory address.

  @param[in] PrivateData      Pointer to PeiCore's private data structure.
  @param[in] CoreFvHandle     Address of PEI_CORE FV Handle in Temporary RAM.

**/
VOID
ConvertPeiCorePpiPointers (
  IN  PEI_CORE_INSTANCE        *PrivateData,
  IN  PEI_CORE_FV_HANDLE       CoreFvHandle
  )
{
  EFI_FV_FILE_INFO      FileInfo;
  EFI_PHYSICAL_ADDRESS  OrgImageBase;
  EFI_PHYSICAL_ADDRESS  MigratedImageBase;
  UINTN                 PeiCoreModuleSize;
  EFI_PEI_FILE_HANDLE   PeiCoreFileHandle;
  VOID                  *PeiCoreImageBase;
  VOID                  *PeiCoreEntryPoint;
  EFI_STATUS            Status;

  PeiCoreFileHandle = NULL;

  //
  // Find the PEI Core image in the BFV in Temporary RAM
  //
  Status =  CoreFvHandle.FvPpi->FindFileByType (
                                  CoreFvHandle.FvPpi,
                                  EFI_FV_FILETYPE_PEI_CORE,
                                  CoreFvHandle.FvHandle,
                                  &PeiCoreFileHandle
                                  );
  ASSERT_EFI_ERROR (Status);

  if (!EFI_ERROR (Status)) {
    //
    // Determine the image base of the PEI core in Temporary RAM
    //
    Status = CoreFvHandle.FvPpi->GetFileInfo (CoreFvHandle.FvPpi, PeiCoreFileHandle, &FileInfo);
    ASSERT_EFI_ERROR (Status);

    Status = PeiGetPe32Data (PeiCoreFileHandle, &PeiCoreImageBase);
    ASSERT_EFI_ERROR (Status);

    //
    // Determine the entry point of the PEI core in Temporary RAM
    //
    Status = PeCoffLoaderGetEntryPoint ((VOID *) (UINTN) PeiCoreImageBase, &PeiCoreEntryPoint);
    ASSERT_EFI_ERROR (Status);

    //
    // Determine the image base of the PEI Core in permanent memory
    //
    OrgImageBase = (UINTN) PeiCoreImageBase;
    MigratedImageBase = (UINTN) _ModuleEntryPoint - ((UINTN) PeiCoreEntryPoint - (UINTN) PeiCoreImageBase);

    //
    // Calculate the size of the PEI core in permanent memory
    //
    PeiCoreModuleSize = (UINTN) FileInfo.BufferSize - ((UINTN) OrgImageBase - (UINTN) FileInfo.Buffer);

    //
    // Migrate PPI pointers to addresses in the range of the PEI core in Temporary RAM
    // to the corresponding address within the PEI core in permanent memory
    //
    ConvertPpiPointersFv (PrivateData, (UINTN) OrgImageBase, (UINTN) MigratedImageBase, PeiCoreModuleSize);
  }
}

/**
  Migrate a PEIM from Temporary RAM to permanent memory.

  @param[in] FileHandle           Pointer to the FFS file header of the image.
  @param[in] MigratedFileHandle   Pointer to the FFS file header of the migrated image.

  @retval EFI_SUCCESS         Successfully migrated the PEIM to permanent memory.

**/
EFI_STATUS
MigratePeim (
  IN  EFI_PEI_FILE_HANDLE     FileHandle,
  IN  EFI_PEI_FILE_HANDLE     MigratedFileHandle
  )
{
  EFI_STATUS            Status;
  EFI_FFS_FILE_HEADER   *FileHeader;
  VOID                  *Pe32Data;
  VOID                  *ImageAddress;
  CHAR8                 *AsciiString;
  UINTN                 Index;

  Status = EFI_SUCCESS;

  FileHeader = (EFI_FFS_FILE_HEADER *) FileHandle;
  ASSERT (!IS_FFS_FILE2 (FileHeader));

  ImageAddress = NULL;
  PeiGetPe32Data (MigratedFileHandle, &ImageAddress);
  if (ImageAddress != NULL) {
    AsciiString = PeCoffLoaderGetPdbPointer (ImageAddress);
    for (Index = 0; AsciiString[Index] != 0; Index++) {
      if (AsciiString[Index] == '\\' || AsciiString[Index] == '/') {
        AsciiString = AsciiString + Index + 1;
        Index = 0;
      } else if (AsciiString[Index] == '.') {
        AsciiString[Index] = 0;
      }
    }
    DEBUG ((DEBUG_INFO, "%a", AsciiString));

    Pe32Data = (VOID *) ((UINTN) ImageAddress - (UINTN) MigratedFileHandle + (UINTN) FileHandle);
    Status = LoadAndRelocatePeCoffImageInPlace (Pe32Data, ImageAddress);
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}

/**
  Migrates PEIMs in the given firmware volume.

  @param[in] Private          Pointer to the PeiCore's private data structure.
  @param[in] FvIndex          The firmware volume index to migrate.
  @param[in] OrgFvHandle      The handle to the firmware volume in Temporary RAM.
  @param[in] FvHandle         The handle to the firmware volume in permanent memory.

  @retval   EFI_SUCCESS           The PEIMs in the FV were migrated successfully
  @retval   EFI_INVALID_PARAMETER The Private pointer is NULL or FvCount is invalid.

**/
EFI_STATUS
EFIAPI
MigratePeimsInFv (
  IN PEI_CORE_INSTANCE    *Private,
  IN  UINTN               FvIndex,
  IN  UINTN               OrgFvHandle,
  IN  UINTN               FvHandle
  )
{
  EFI_STATUS              Status;
  volatile UINTN          FileIndex;
  EFI_PEI_FILE_HANDLE     MigratedFileHandle;
  EFI_PEI_FILE_HANDLE     FileHandle;

  if (Private == NULL || FvIndex >= Private->FvCount) {
    return EFI_INVALID_PARAMETER;
  }

  if (Private->Fv[FvIndex].ScanFv) {
    for (FileIndex = 0; FileIndex < Private->Fv[FvIndex].PeimCount; FileIndex++) {
      if (Private->Fv[FvIndex].FvFileHandles[FileIndex] != NULL) {
        FileHandle = Private->Fv[FvIndex].FvFileHandles[FileIndex];

        MigratedFileHandle = (EFI_PEI_FILE_HANDLE) ((UINTN) FileHandle - OrgFvHandle + FvHandle);

        DEBUG ((DEBUG_VERBOSE, "    Migrating FileHandle %2d ", FileIndex));
        Status = MigratePeim (FileHandle, MigratedFileHandle);
        DEBUG ((DEBUG_INFO, "\n"));
        ASSERT_EFI_ERROR (Status);

        if (!EFI_ERROR (Status)) {
          Private->Fv[FvIndex].FvFileHandles[FileIndex] = MigratedFileHandle;
          if (FvIndex == Private->CurrentPeimFvCount) {
            Private->CurrentFvFileHandles[FileIndex] = MigratedFileHandle;
          }
        }
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Migrates firmware volumes from Temporary RAM to permanent memory.

  Any pointers in PEI core private data structures within the address range
  covered by the firmware volumes that are migrated are updated from the
  pre-memory address to the permanent memory address.

  @param[in] Private        PeiCore's private data structure
  @param[in] SecCoreData    Points to a data structure containing information about the PEI core's operating
                            environment, such as the size and location of Temporary RAM, the stack location and
                            the BFV location.

  @retval EFI_SUCCESS           Successfully migrated installed firmware volumes from Temporary RAM to permanent memory.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory exists to allocate the pages needed for migration.

**/
EFI_STATUS
MigrateTemporaryRamFvs (
  IN PEI_CORE_INSTANCE            *Private,
  IN CONST EFI_SEC_PEI_HAND_OFF   *SecCoreData
  )
{
  EFI_STATUS                    Status;
  volatile UINTN                FvIndex;
  volatile UINTN                FvChildIndex;
  UINTN                         ChildFvOffset;
  EFI_FIRMWARE_VOLUME_HEADER    *FvHeader;
  EFI_FIRMWARE_VOLUME_HEADER    *ChildFvHeader;
  EFI_FIRMWARE_VOLUME_HEADER    *MigratedFvHeader;
  EFI_FIRMWARE_VOLUME_HEADER    *MigratedChildFvHeader;

  PEI_CORE_FV_HANDLE            PeiCoreFvHandle;
  EFI_PEI_CORE_FV_LOCATION_PPI  *PeiCoreFvLocationPpi;

  ASSERT (Private->PeiMemoryInstalled);

  DEBUG ((DEBUG_VERBOSE, "Beginning migration of Temporary RAM FV contents.\n"));

  //
  // Migrate PPI Pointers installed in the Temporary RAM PEI_CORE to the newly loaded PEI_CORE in permanent memory
  //
  Status = PeiLocatePpi ((CONST EFI_PEI_SERVICES **) &Private->Ps, &gEfiPeiCoreFvLocationPpiGuid, 0, NULL, (VOID **) &PeiCoreFvLocationPpi);
  if (!EFI_ERROR (Status) && (PeiCoreFvLocationPpi->PeiCoreFvLocation != NULL)) {
    PeiCoreFvHandle.FvHandle = (EFI_PEI_FV_HANDLE) PeiCoreFvLocationPpi->PeiCoreFvLocation;
  } else {
    PeiCoreFvHandle.FvHandle = (EFI_PEI_FV_HANDLE) SecCoreData->BootFirmwareVolumeBase;
  }
  for (FvIndex = 0; FvIndex < Private->FvCount; FvIndex++) {
    if (Private->Fv[FvIndex].FvHandle == PeiCoreFvHandle.FvHandle) {
      PeiCoreFvHandle = Private->Fv[FvIndex];
      ConvertPeiCorePpiPointers (Private, PeiCoreFvHandle);
      break;
    }
  }

  //
  // Allocate the migration map structure
  //
  Private->MigrationMap.Entry = AllocateZeroPool (sizeof (MIGRATION_MAP_ENTRY) * Private->FvCount);
  ASSERT (Private->MigrationMap.Entry != NULL);

  //
  // Migrate installed firmware volumes in Temporary RAM to permanent memory
  // and populate the migration map structure
  //
  Status = EFI_SUCCESS;
  for (FvIndex = 0; FvIndex < Private->FvCount; FvIndex++) {
    FvHeader = Private->Fv[FvIndex].FvHeader;
    if (FvHeader == NULL) {
      ASSERT (FvHeader != NULL);
      continue;
    }

    DEBUG ((DEBUG_VERBOSE, "FV[%02d] at 0x%x.\n", FvIndex, (UINTN) FvHeader));
    if (!(((EFI_PHYSICAL_ADDRESS) (UINTN) FvHeader >= Private->PhysicalMemoryBegin) &&
        (((EFI_PHYSICAL_ADDRESS) (UINTN) FvHeader + (FvHeader->FvLength - 1)) < Private->FreePhysicalMemoryTop))) {
      Status =  PeiServicesAllocatePages (
                  EfiBootServicesCode,
                  EFI_SIZE_TO_PAGES ((UINTN) FvHeader->FvLength),
                  (EFI_PHYSICAL_ADDRESS *) &MigratedFvHeader
                  );
      ASSERT_EFI_ERROR (Status);

      DEBUG ((
        DEBUG_VERBOSE,
        "  Migrating FV[%d] from 0x%08X to 0x%08X\n",
        FvIndex,
        (UINTN) FvHeader,
        (UINTN) MigratedFvHeader
        ));
      DEBUG ((
        DEBUG_VERBOSE,
        "  FV buffer range from 0x%08x to 0x%08x\n",
        (UINTN) MigratedFvHeader,
        ((UINTN) MigratedFvHeader) + EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES ((UINTN) FvHeader->FvLength))
        ));

      CopyMem (MigratedFvHeader, FvHeader, (UINTN) FvHeader->FvLength);

      if (Private->MigrationMap.Entry != NULL) {
        Private->MigrationMap.Entry[FvIndex].PreMemoryBase = (EFI_PHYSICAL_ADDRESS) (UINTN) FvHeader;
        Private->MigrationMap.Entry[FvIndex].PreMemoryLength = (UINTN) FvHeader->FvLength;
        Private->MigrationMap.Entry[FvIndex].PostMemoryBase = (EFI_PHYSICAL_ADDRESS) (UINTN) MigratedFvHeader;
        Private->MigrationMap.Entry[FvIndex].PostMemoryLength = (UINTN) FvHeader->FvLength;
        Private->MigrationMap.Count++;
      }

      //
      // Migrate any child firmware volumes for this firmware volume
      //
      for (FvChildIndex = FvIndex; FvChildIndex < Private->FvCount; FvChildIndex++) {
        ChildFvHeader = Private->Fv[FvChildIndex].FvHeader;
        if (((UINTN) ChildFvHeader > (UINTN) FvHeader) &&
            (((UINTN) ChildFvHeader + ChildFvHeader->FvLength) < ((UINTN) FvHeader) + FvHeader->FvLength)) {
          DEBUG ((DEBUG_VERBOSE, "    Child FV[%02d] is being migrated.\n", FvChildIndex));
          ChildFvOffset = (UINTN) ChildFvHeader - (UINTN) FvHeader;
          DEBUG ((DEBUG_VERBOSE, "    Child FV offset = 0x%x.\n", ChildFvOffset));
          MigratedChildFvHeader = (EFI_FIRMWARE_VOLUME_HEADER *) ((UINTN) MigratedFvHeader + ChildFvOffset);
          Private->Fv[FvChildIndex].FvHeader = MigratedChildFvHeader;
          Private->Fv[FvChildIndex].FvHandle = (EFI_PEI_FV_HANDLE) MigratedChildFvHeader;
          DEBUG ((DEBUG_VERBOSE, "    Child migrated FV header at 0x%x.\n", (UINTN) MigratedChildFvHeader));

          Status =  MigratePeimsInFv (Private, FvChildIndex, (UINTN) ChildFvHeader, (UINTN) MigratedChildFvHeader);
          ASSERT_EFI_ERROR (Status);

          ConvertPpiPointersFv (
            Private,
            (UINTN) ChildFvHeader,
            (UINTN) MigratedChildFvHeader,
            (UINTN) ChildFvHeader->FvLength - 1
            );

          ConvertFvHob (Private, (UINTN) ChildFvHeader, (UINTN) MigratedChildFvHeader);
        }
      }
      Private->Fv[FvIndex].FvHeader = MigratedFvHeader;
      Private->Fv[FvIndex].FvHandle = (EFI_PEI_FV_HANDLE) MigratedFvHeader;

      Status = MigratePeimsInFv (Private, FvIndex, (UINTN) FvHeader, (UINTN) MigratedFvHeader);
      ASSERT_EFI_ERROR (Status);

      ConvertPpiPointersFv (
        Private,
        (UINTN) FvHeader,
        (UINTN) MigratedFvHeader,
        (UINTN) FvHeader->FvLength - 1
        );

      ConvertFvHob (Private, (UINTN) FvHeader, (UINTN) MigratedFvHeader);
    }
  }

  //
  // Remove FV HOBs that still point to pre-memory FVs
  //
  RemoveFvHobsInTemporaryMemory (Private);

  //
  // Install the PEI_CONVERT_POINTER_PPI to provide a pointer conversion
  // service to PEIMs based on the FV address mappings stored during migration
  //
  PeiServicesInstallPpi (&mPeiConvertPointerPpiList);

  return Status;
}
