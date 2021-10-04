/** @file
  EFI_FILE_PROTOCOL.Close() member function for the Virtio Filesystem driver.

  Copyright (C) 2020, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>             // RemoveEntryList()
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
VirtioFsSimpleFileClose (
  IN EFI_FILE_PROTOCOL *This
  )
{
  VIRTIO_FS_FILE  *VirtioFsFile;
  VIRTIO_FS       *VirtioFs;

  VirtioFsFile = VIRTIO_FS_FILE_FROM_SIMPLE_FILE (This);
  VirtioFs     = VirtioFsFile->OwnerFs;

  //
  // All actions in this function are "best effort"; the UEFI spec requires
  // EFI_FILE_PROTOCOL.Close() to sync all data to the device, but it also
  // requires EFI_FILE_PROTOCOL.Close() to release resources unconditionally,
  // and to return EFI_SUCCESS unconditionally.
  //
  // Flush, sync, release, and (if needed) forget. If any action fails, we
  // still try the others.
  //
  if (VirtioFsFile->IsOpenForWriting) {
    if (!VirtioFsFile->IsDirectory) {
      VirtioFsFuseFlush (
        VirtioFs,
        VirtioFsFile->NodeId,
        VirtioFsFile->FuseHandle
        );
    }

    VirtioFsFuseFsyncFileOrDir (
      VirtioFs,
      VirtioFsFile->NodeId,
      VirtioFsFile->FuseHandle,
      VirtioFsFile->IsDirectory
      );
  }

  VirtioFsFuseReleaseFileOrDir (
    VirtioFs,
    VirtioFsFile->NodeId,
    VirtioFsFile->FuseHandle,
    VirtioFsFile->IsDirectory
    );

  //
  // VirtioFsFile->FuseHandle is gone at this point, but VirtioFsFile->NodeId
  // is still valid. If we've known VirtioFsFile->NodeId from a lookup, then
  // now we should ask the server to forget it *once*.
  //
  if (VirtioFsFile->NodeId != VIRTIO_FS_FUSE_ROOT_DIR_NODE_ID) {
    VirtioFsFuseForget (VirtioFs, VirtioFsFile->NodeId);
  }

  //
  // One fewer file left open for the owner filesystem.
  //
  RemoveEntryList (&VirtioFsFile->OpenFilesEntry);

  FreePool (VirtioFsFile->CanonicalPathname);
  if (VirtioFsFile->FileInfoArray != NULL) {
    FreePool (VirtioFsFile->FileInfoArray);
  }

  FreePool (VirtioFsFile);
  return EFI_SUCCESS;
}
