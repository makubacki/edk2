// ------------------------------------------------------------------------------
//
// Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
// Copyright (c) 2021, Arm Limited. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// ------------------------------------------------------------------------------

typedef __SIZE_TYPE__ size_t;

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
__memset (
  void *s, int c, size_t n
  )
{
  unsigned char  *d;

  d = s;

  while (n-- != 0) {
    *d++ = c;
  }

  return s;
}

//
// Other modules (such as CryptoPkg/IntrinsicLib) may provide another
// implementation of memset(), which may conflict with this one if this
// object was pulled into the link due to the definitions below. So make
// our memset() 'weak' to let the other implementation take precedence.
//
__attribute__ ((__weak__, __alias__ ("__memset")))
void *
memset (
  void *dest, int c, size_t n
  );

#ifdef __arm__

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
  void
  __aeabi_memset (
  void *dest, size_t n, int c
  )
  {
    __memset (dest, c, n);
  }

  __attribute__ ((__alias__ ("__aeabi_memset")))
  void
  __aeabi_memset4 (
  void *dest, size_t n, int c
  );

  __attribute__ ((__alias__ ("__aeabi_memset")))
  void
  __aeabi_memset8 (
  void *dest, size_t n, int c
  );

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
  void
  __aeabi_memclr (
  void *dest, size_t n
  )
  {
    __memset (dest, 0, n);
  }

  __attribute__ ((__alias__ ("__aeabi_memclr")))
  void
  __aeabi_memclr4 (
  void *dest, size_t n
  );

  __attribute__ ((__alias__ ("__aeabi_memclr")))
  void
  __aeabi_memclr8 (
  void *dest, size_t n
  );

#endif
