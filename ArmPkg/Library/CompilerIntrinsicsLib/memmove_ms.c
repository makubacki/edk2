// ------------------------------------------------------------------------------
//
// Copyright (c) 2019, Pete Batard. All rights reserved.
// Copyright (c) 2021, Arm Limited. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// ------------------------------------------------------------------------------

#if defined (_M_ARM64)
  typedef unsigned __int64 size_t;
#else
  typedef unsigned __int32 size_t;
#endif

void *
memmove (
  void *, const void *, size_t
  );

#pragma intrinsic(memmove)
#pragma function(memmove)

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
void *
memmove (
  void *dest, const void *src, size_t n
  )
{
  unsigned char        *d;
  unsigned char const  *s;

  d = dest;
  s = src;

  if (d < s) {
    while (n-- != 0) {
      *d++ = *s++;
    }
  } else {
    d += n;
    s += n;
    while (n-- != 0) {
      *--d = *--s;
    }
  }

  return dest;
}
