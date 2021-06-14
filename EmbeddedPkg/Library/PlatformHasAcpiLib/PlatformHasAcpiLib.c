/** @file
  A hook-in library for MdeModulePkg/Universal/Acpi/AcpiTableDxe.

  Plugging this library instance into AcpiTableDxe makes
  EFI_ACPI_TABLE_PROTOCOL and (if enabled) EFI_ACPI_SDT_PROTOCOL depend on the
  platform's dynamic decision whether to expose an ACPI-based hardware
  description to the operating system.

  Universal and platform specific DXE drivers that produce ACPI tables depend
  on EFI_ACPI_TABLE_PROTOCOL / EFI_ACPI_SDT_PROTOCOL in turn.

  Copyright (C) 2017, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>

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
RETURN_STATUS
EFIAPI
PlatformHasAcpiInitialize (
  VOID
  )
{
  //
  // Do nothing, just imbue AcpiTableDxe with a protocol dependency on
  // EDKII_PLATFORM_HAS_ACPI_GUID.
  //
  return RETURN_SUCCESS;
}
