/** @file
  A hook-in library for:
  - MdeModulePkg/Universal/Variable/Pei/VariablePei.inf
  - MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
  - MdeModulePkg/Universal/Variable/RuntimeDxe/VariableSmm.inf

  Plugging this library instance into one of the above modules makes that
  variable service backend wait for another platform module to dynamically
  initialize or verify EFI_FIRMWARE_VOLUME_HEADER and VARIABLE_STORE_HEADER in
  the non-volatile variable store FVB device. The initialization / verification
  is signaled by installing gEdkiiNvVarStoreFormattedGuid into the
  phase-matching PPI or protocol database, with a NULL interface. (Note that
  installing gEdkiiNvVarStoreFormattedGuid into either the DXE or the MM
  protocol database will unblock VariableSmm -- refer to EFI_SECTION_MM_DEPEX
  in the PI spec.)

  Copyright (C) 2018, Red Hat, Inc.

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
NvVarStoreFormattedInitialize (
  VOID
  )
{
  //
  // Do nothing, just imbue VariablePei / VariableRuntimeDxe / VariableSmm with
  // a PPI or protocol dependency on EDKII_NV_VAR_STORE_FORMATTED_GUID.
  //
  return RETURN_SUCCESS;
}
