/** @file

  IoMmuDxe driver installs EDKII_IOMMU_PROTOCOL to provide the support for DMA
  operations when SEV is enabled.

  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AmdSevIoMmu.h"

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
IoMmuDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  //
  // When SEV is enabled, install IoMmu protocol otherwise install the
  // placeholder protocol so that other dependent module can run.
  //
  if (MemEncryptSevIsEnabled ()) {
    Status = AmdSevInstallIoMmuProtocol ();
  } else {
    Handle = NULL;

    Status = gBS->InstallMultipleProtocolInterfaces (
                                                     &Handle,
                                                     &gIoMmuAbsentProtocolGuid,
                                                     NULL,
                                                     NULL
                                                     );
  }

  return Status;
}
