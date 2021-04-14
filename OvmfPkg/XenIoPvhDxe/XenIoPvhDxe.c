/** @file

  Driver for the XenIo protocol

  This driver simply allocate space for the grant tables.

  Copyright (c) 2019, Citrix Systems, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/XenIoMmioLib.h>
#include <Library/XenPlatformLib.h>

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
InitializeXenIoPvhDxe (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  VOID        *Allocation;
  EFI_STATUS  Status;
  EFI_HANDLE  XenIoHandle;

  Allocation  = NULL;
  XenIoHandle = NULL;

  if (!XenPvhDetected ()) {
    return EFI_UNSUPPORTED;
  }

  Allocation = AllocateReservedPages (FixedPcdGet32 (PcdXenGrantFrames));
  if (Allocation == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }

  Status = XenIoMmioInstall (&XenIoHandle, (UINTN) Allocation);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  return EFI_SUCCESS;

Error:
  if (Allocation != NULL) {
    FreePages (Allocation, FixedPcdGet32 (PcdXenGrantFrames));
  }

  return Status;
}
