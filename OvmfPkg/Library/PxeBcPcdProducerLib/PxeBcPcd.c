/** @file
  Configure some PCDs dynamically for
  "NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf", from QEMU's fw_cfg.

  Copyright (C) 2020, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/PcdLib.h>
#include <Library/QemuFwCfgSimpleParserLib.h>

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
SetPxeBcPcds (
  VOID
  )
{
  BOOLEAN        FwCfgBool;
  RETURN_STATUS  PcdStatus;

  if (!RETURN_ERROR (
         QemuFwCfgParseBool (
           "opt/org.tianocore/IPv4PXESupport",
           &FwCfgBool
           )
         ))
  {
    PcdStatus = PcdSet8S (PcdIPv4PXESupport, FwCfgBool);
    if (RETURN_ERROR (PcdStatus)) {
      return PcdStatus;
    }
  }

  if (!RETURN_ERROR (
         QemuFwCfgParseBool (
           "opt/org.tianocore/IPv6PXESupport",
           &FwCfgBool
           )
         ))
  {
    PcdStatus = PcdSet8S (PcdIPv6PXESupport, FwCfgBool);
    if (RETURN_ERROR (PcdStatus)) {
      return PcdStatus;
    }
  }

  return RETURN_SUCCESS;
}
