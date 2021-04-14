/** @file
  Functions to make Xen hypercalls.

  Copyright (C) 2014, Citrix Ltd.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <IndustryStandard/Xen/hvm/params.h>
#include <IndustryStandard/Xen/memory.h>

#include <Library/DebugLib.h>
#include <Library/XenHypercallLib.h>

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
XenHypercallLibConstruct (
  VOID
  )
{
  XenHypercallLibInit ();
  //
  // We don't fail library construction, since that has catastrophic
  // consequences for client modules (whereas those modules may easily be
  // running on a non-Xen platform). Instead, XenHypercallIsAvailable()
  // will return FALSE.
  //
  return RETURN_SUCCESS;
}

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
UINT64
EFIAPI
XenHypercallHvmGetParam (
  IN UINT32        Index
  )
{
  xen_hvm_param_t  Parameter;
  INTN             Error;

  Parameter.domid = DOMID_SELF;
  Parameter.index = Index;
  Error = XenHypercall2 (
                         __HYPERVISOR_hvm_op,
                         HVMOP_get_param,
                         (INTN) &Parameter
                         );
  if (Error != 0) {
    DEBUG (
           (DEBUG_ERROR,
            "XenHypercall: Error %Ld trying to get HVM parameter %d\n",
            (INT64) Error, Index)
           );
    return 0;
  }

  return Parameter.value;
}

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
INTN
EFIAPI
XenHypercallMemoryOp (
  IN     UINTN Operation,
  IN OUT VOID *Arguments
  )
{
  return XenHypercall2 (
                        __HYPERVISOR_memory_op,
                        Operation,
                        (INTN) Arguments
                        );
}

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
INTN
EFIAPI
XenHypercallEventChannelOp (
  IN     INTN Operation,
  IN OUT VOID *Arguments
  )
{
  return XenHypercall2 (
                        __HYPERVISOR_event_channel_op,
                        Operation,
                        (INTN) Arguments
                        );
}
