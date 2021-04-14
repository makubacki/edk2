/** @file
  EFI_FILE_PROTOCOL.SetPosition() member function for the Virtio Filesystem
  driver.

  Copyright (C) 2020, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/MemoryAllocationLib.h> // FreePool()

#include "VirtioFsDxe.h"

/**
  [TEMPLATE] - Provide a function description!

  Function overview/purpose.

  Anything a caller should be aware of must be noted in the description.

  All parameters must be described. Parameter names must be Pascal case.

  @retval must be used and each unique return code should be clearly
  described. Providing "Others" is only acceptable if a return code
  is bubbled up from a function called internal to this function. However,
  that's usually not helpful. Try to provide explicit values that mean
  something to the caller.

  Examples:
  @param[in]      ParameterName         Brief parameter description.
  @param[out]     ParameterName         Brief parameter description.
  @param[in,out]  ParameterName         Brief parameter description.

  @retval   EFI_SUCCESS                 Brief return code description.

**/
EFI_STATUS
EFIAPI
VirtioFsSimpleFileSetPosition (
  IN EFI_FILE_PROTOCOL *This,
  IN UINT64            Position
  )
{
  VIRTIO_FS_FILE                      *VirtioFsFile;
  VIRTIO_FS                           *VirtioFs;
  EFI_STATUS                          Status;
  VIRTIO_FS_FUSE_ATTRIBUTES_RESPONSE  FuseAttr;

  VirtioFsFile = VIRTIO_FS_FILE_FROM_SIMPLE_FILE (This);

  //
  // Directories can only be rewound, per spec.
  //
  if (VirtioFsFile->IsDirectory) {
    if (Position != 0) {
      return EFI_UNSUPPORTED;
    }

    VirtioFsFile->FilePosition = 0;
    if (VirtioFsFile->FileInfoArray != NULL) {
      FreePool (VirtioFsFile->FileInfoArray);
      VirtioFsFile->FileInfoArray = NULL;
    }

    VirtioFsFile->SingleFileInfoSize = 0;
    VirtioFsFile->NumFileInfo  = 0;
    VirtioFsFile->NextFileInfo = 0;
    return EFI_SUCCESS;
  }

  //
  // Regular file.
  //
  if (Position < MAX_UINT64) {
    //
    // Caller is requesting absolute file position.
    //
    VirtioFsFile->FilePosition = Position;
    return EFI_SUCCESS;
  }

  //
  // Caller is requesting a seek to EOF.
  //
  VirtioFs = VirtioFsFile->OwnerFs;
  Status   = VirtioFsFuseGetAttr (VirtioFs, VirtioFsFile->NodeId, &FuseAttr);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  VirtioFsFile->FilePosition = FuseAttr.Size;
  return EFI_SUCCESS;
}
