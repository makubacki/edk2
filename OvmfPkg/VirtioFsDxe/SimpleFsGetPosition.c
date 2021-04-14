/** @file
  EFI_FILE_PROTOCOL.GetPosition() member function for the Virtio Filesystem
  driver.

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
VirtioFsSimpleFileGetPosition (
  IN     EFI_FILE_PROTOCOL *This,
  OUT UINT64            *Position
  )
{
  VIRTIO_FS_FILE  *VirtioFsFile;

  VirtioFsFile = VIRTIO_FS_FILE_FROM_SIMPLE_FILE (This);
  if (VirtioFsFile->IsDirectory) {
    return EFI_UNSUPPORTED;
  }

  *Position = VirtioFsFile->FilePosition;
  return EFI_SUCCESS;
}
