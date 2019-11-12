/** @file
  Pei Core Migration Support

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PeiMain.h"

/**

  Migrate Pointer from the temporary memory to PEI installed memory.

  @param Pointer         Pointer to the Pointer needs to be converted.
  @param TempBottom      Base of old temporary memory
  @param TempTop         Top of old temporary memory
  @param Offset          Offset of new memory to old temporary memory.
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

  Migrate Pointer in ranges of the temporary memory to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of temporary RAM, the stack location and the BFV location.
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

  Migrate Single PPI Pointer from the temporary memory to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of temporary RAM, the stack location and the BFV location.
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

  Migrate PPI Pointers from the temporary memory to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of temporary RAM, the stack location and the BFV location.
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
  @param[in] TemporaryRamMigrated   Temporary memory has been migrated to permanent memory.

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
  from the temporary memory to PEI installed memory.

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
