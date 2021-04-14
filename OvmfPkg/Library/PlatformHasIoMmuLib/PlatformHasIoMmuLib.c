/** @file
  A hook-in library for MdeModulePkg/Bus/Pci/PciHostBridgeDxe.

  Plugging this library instance into PciHostBridgeDxe makes
  PciHostBridgeDxe depend on the platform's dynamic decision whether
  to provide IOMMU implementation (usually through IoMmuDxe driver).

  Copyright (C) 2017, Red Hat, Inc.
  Copyright (C) 2017, AMD, Inc.

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
PlatformHasIoMmuInitialize (
  VOID
  )
{
  //
  // Do nothing, just imbue PciHostBridgeDxe with a protocol dependency on
  // gIoMmuAbsentProtocolGuid OR gEdkiiIoMmuProtocolGuid.
  //
  return RETURN_SUCCESS;
}
