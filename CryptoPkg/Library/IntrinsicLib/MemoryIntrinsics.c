/** @file
  Intrinsic Memory Routines Wrapper Implementation for OpenSSL-based
  Cryptographic Library.

Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>

typedef UINTN size_t;

#if defined (__GNUC__) || defined (__clang__)
  #define GLOBAL_USED  __attribute__((used))
#else
  #define GLOBAL_USED
#endif

/* OpenSSL will use floating point support, and C compiler produces the _fltused
   symbol by default. Simply define this symbol here to satisfy the linker. */
int  GLOBAL_USED  _fltused = 1;

/* Sets buffers to a specified character */
void *
memset (
  void *dest,
  int ch,
  size_t count
  )
{
  //
  // NOTE: Here we use one base implementation for memset, instead of the direct
  //       optimized SetMem() wrapper. Because the IntrinsicLib has to be built
  //       without whole program optimization option, and there will be some
  //       potential register usage errors when calling other optimized codes.
  //

  //
  // Declare the local variables that actually move the data elements as
  // volatile to prevent the optimizer from replacing this function with
  // the intrinsic memset()
  //
  volatile UINT8  *Pointer;

  Pointer = (UINT8 *)dest;
  while (count-- != 0) {
    *(Pointer++) = (UINT8)ch;
  }

  return dest;
}

/* Compare bytes in two buffers. */
int
memcmp (
  const void *buf1,
  const void *buf2,
  size_t count
  )
{
  return (int)CompareMem (buf1, buf2, count);
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
int
strcmp (
  const char *s1,
  const char *s2
  )
{
  return (int)AsciiStrCmp (s1, s2);
}
