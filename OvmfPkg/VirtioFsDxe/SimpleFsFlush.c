/** @file
  EFI_FILE_PROTOCOL.Flush() member function for the Virtio Filesystem driver.

  Copyright (C) 2020, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

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
VirtioFsSimpleFileFlush (
  IN EFI_FILE_PROTOCOL *This
  )
{
  VIRTIO_FS_FILE  *VirtioFsFile;
  VIRTIO_FS       *VirtioFs;
  EFI_STATUS      Status;

  VirtioFsFile = VIRTIO_FS_FILE_FROM_SIMPLE_FILE (This);
  VirtioFs     = VirtioFsFile->OwnerFs;

  if (!VirtioFsFile->IsOpenForWriting) {
    return EFI_ACCESS_DENIED;
  }

  //
  // FUSE_FLUSH is for regular files only.
  //
  if (!VirtioFsFile->IsDirectory) {
    Status = VirtioFsFuseFlush (
               VirtioFs,
               VirtioFsFile->NodeId,
               VirtioFsFile->FuseHandle
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Status = VirtioFsFuseFsyncFileOrDir (
             VirtioFs,
             VirtioFsFile->NodeId,
             VirtioFsFile->FuseHandle,
             VirtioFsFile->IsDirectory
             );
  return Status;
}
