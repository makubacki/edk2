// ------------------------------------------------------------------------------
//
// Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
// Copyright (c) 2021, Arm Limited. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// ------------------------------------------------------------------------------

typedef __SIZE_TYPE__ size_t;

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
static void
__memcpy (
  void *dest,
  const void *src,
  size_t n
  )
{
  unsigned char        *d;
  unsigned char const  *s;

  d = dest;
  s = src;

  while (n-- != 0) {
    *d++ = *s++;
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
void *
memcpy (
  void *dest,
  const void *src,
  size_t n
  )
{
  __memcpy (dest, src, n);
  return dest;
}

#ifdef __arm__

  __attribute__ ((__alias__ ("__memcpy")))
  void
  __aeabi_memcpy (
  void *dest,
  const void *src,
  size_t n
  );

  __attribute__ ((__alias__ ("__memcpy")))
  void
  __aeabi_memcpy4 (
  void *dest,
  const void *src,
  size_t n
  );

  __attribute__ ((__alias__ ("__memcpy")))
  void
  __aeabi_memcpy8 (
  void *dest,
  const void *src,
  size_t n
  );

#endif
