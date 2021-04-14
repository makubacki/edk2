/** @file
  Decide whether the firmware should expose an ACPI- and/or a Device Tree-based
  hardware description to the operating system.

  Copyright (c) 2017, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Guid/PlatformHasAcpi.h>
#include <Guid/PlatformHasDeviceTree.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

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
XenPlatformHasAcpiDt (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // If we fail to install any of the necessary protocols below, the OS will be
  // unbootable anyway (due to lacking hardware description), so tolerate no
  // errors here.
  //
  // Always make ACPI available on 64-bit systems.
  //
  if (MAX_UINTN == MAX_UINT64) {
    Status = gBS->InstallProtocolInterface (
                                            &ImageHandle,
                                            &gEdkiiPlatformHasAcpiGuid,
                                            EFI_NATIVE_INTERFACE,
                                            NULL
                                            );
    if (EFI_ERROR (Status)) {
      goto Failed;
    }
  }

  //
  // Expose the Device Tree unconditionally.
  //
  Status = gBS->InstallProtocolInterface (
                                          &ImageHandle,
                                          &gEdkiiPlatformHasDeviceTreeGuid,
                                          EFI_NATIVE_INTERFACE,
                                          NULL
                                          );
  if (EFI_ERROR (Status)) {
    goto Failed;
  }

  return Status;

Failed:
  ASSERT_EFI_ERROR (Status);
  CpuDeadLoop ();
  //
  // Keep compilers happy.
  //
  return Status;
}
