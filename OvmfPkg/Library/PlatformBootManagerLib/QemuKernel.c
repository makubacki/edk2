/** @file

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/QemuLoadImageLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/UefiLib.h>

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
TryRunningQemuKernel (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  KernelImageHandle;

  Status = QemuLoadKernelImage (&KernelImageHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Signal the EVT_SIGNAL_READY_TO_BOOT event
  //
  EfiSignalEventReadyToBoot ();

  REPORT_STATUS_CODE (
                      EFI_PROGRESS_CODE,
                      (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_READY_TO_BOOT_EVENT)
                      );

  //
  // Start the image.
  //
  Status = QemuStartKernelImage (&KernelImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR, "%a: QemuStartKernelImage(): %r\n", __FUNCTION__,
            Status)
           );
  }

  QemuUnloadKernelImage (KernelImageHandle);

  return Status;
}
