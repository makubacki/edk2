/** @file
  Intrinsic Memory Routines Wrapper Implementation for OpenSSL-based
  Cryptographic Library.

Copyright (c) 2010, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/BaseMemoryLib.h>

#if defined (__clang__) && !defined (__APPLE__)

  /* Copies bytes between buffers */
  static __attribute__ ((__used__)/**
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
                        )
  void *
  __memcpy (
  void *dest, const void *src, unsigned int count
  )
  {
    return CopyMem (dest, src, (UINTN) count);
  }

  __attribute__ ((__alias__ ("__memcpy")))
  void *
  memcpy (
  void *dest, const void *src, unsigned int count
  );

#else
  /* Copies bytes between buffers */
  void *
  memcpy (
  void *dest, const void *src, unsigned int count
  )
  {
    return CopyMem (dest, src, (UINTN) count);
  }

#endif
