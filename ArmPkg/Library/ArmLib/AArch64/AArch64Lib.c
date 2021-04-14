/** @file

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Portions copyright (c) 2011 - 2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Library/ArmLib.h>
#include <Library/DebugLib.h>

#include <Chipset/AArch64.h>

#include "AArch64Lib.h"
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
AArch64DataCacheOperation (
  IN  AARCH64_CACHE_OPERATION  DataCacheOperation
  )
{
  UINTN  SavedInterruptState;

  SavedInterruptState = ArmGetInterruptState ();
  ArmDisableInterrupts ();

  AArch64AllDataCachesOperation (DataCacheOperation);

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
  AArch64DataCacheOperation (ArmInvalidateDataCacheEntryBySetWay);
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
  AArch64DataCacheOperation (ArmCleanInvalidateDataCacheEntryBySetWay);
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
  AArch64DataCacheOperation (ArmCleanDataCacheEntryBySetWay);
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
  return ((ArmReadIdAA64Pfr0 () & AARCH64_PFR0_GIC) != 0);
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
  UINTN  Mmfr2;

  Mmfr2 = ArmReadIdAA64Mmfr2 ();
  return (((Mmfr2 >> 20) & 0xF) == 1) ? TRUE : FALSE;
}
