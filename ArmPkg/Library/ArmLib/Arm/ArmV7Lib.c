/** @file

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Library/ArmLib.h>
#include <Library/DebugLib.h>

#include <Chipset/ArmV7.h>

#include "ArmV7Lib.h"
#include "ArmLibPrivate.h"

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
VOID
ArmV7DataCacheOperation (
  IN  ARM_V7_CACHE_OPERATION  DataCacheOperation
  )
{
  UINTN  SavedInterruptState;

  SavedInterruptState = ArmGetInterruptState ();
  ArmDisableInterrupts ();

  ArmV7AllDataCachesOperation (DataCacheOperation);

  ArmDataSynchronizationBarrier ();

  if (SavedInterruptState) {
    ArmEnableInterrupts ();
  }
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
VOID
EFIAPI
ArmInvalidateDataCache (
  VOID
  )
{
  ASSERT (!ArmMmuEnabled ());

  ArmDataSynchronizationBarrier ();
  ArmV7DataCacheOperation (ArmInvalidateDataCacheEntryBySetWay);
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
VOID
EFIAPI
ArmCleanInvalidateDataCache (
  VOID
  )
{
  ASSERT (!ArmMmuEnabled ());

  ArmDataSynchronizationBarrier ();
  ArmV7DataCacheOperation (ArmCleanInvalidateDataCacheEntryBySetWay);
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
VOID
EFIAPI
ArmCleanDataCache (
  VOID
  )
{
  ASSERT (!ArmMmuEnabled ());

  ArmDataSynchronizationBarrier ();
  ArmV7DataCacheOperation (ArmCleanDataCacheEntryBySetWay);
}

/**
  Check whether the CPU supports the GIC system register interface (any version)

  @return   Whether GIC System Register Interface is supported

**/
BOOLEAN
EFIAPI
ArmHasGicSystemRegisters (
  VOID
  )
{
  return ((ArmReadIdPfr1 () & ARM_PFR1_GIC) != 0);
}

/**
  Check whether the CPU supports the Security extensions

  @return   Whether the Security extensions are implemented

**/
BOOLEAN
EFIAPI
ArmHasSecurityExtensions (
  VOID
  )
{
  return ((ArmReadIdPfr1 () & ARM_PFR1_SEC) != 0);
}

/** Checks if CCIDX is implemented.

   @retval TRUE  CCIDX is implemented.
   @retval FALSE CCIDX is not implemented.
**/
BOOLEAN
EFIAPI
ArmHasCcidx (
  VOID
  )
{
  UINTN  Mmfr4;

  Mmfr4 = ArmReadIdMmfr4 ();
  return (((Mmfr4 >> 24) & 0xF) == 1) ? TRUE : FALSE;
}
