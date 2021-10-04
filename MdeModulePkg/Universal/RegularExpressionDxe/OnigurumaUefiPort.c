/** @file

  Module to rewrite stdlib references within Oniguruma

  (C) Copyright 2014-2021 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "OnigurumaUefiPort.h"

#define ONIGMEM_HEAD_SIGNATURE  SIGNATURE_32('o','m','h','d')

typedef struct {
  UINT32    Signature;
  UINTN     Size;
} ONIGMEM_HEAD;

#define ONIGMEM_OVERHEAD  sizeof(ONIGMEM_HEAD)

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
int EFIAPI
sprintf_s (
  char *str,
  size_t sizeOfBuffer,
  char const *fmt,
  ...
  )
{
  VA_LIST  Marker;
  int      NumberOfPrinted;

  VA_START (Marker, fmt);
  NumberOfPrinted = (int)AsciiVSPrint (str, sizeOfBuffer, fmt, Marker);
  VA_END (Marker);

  return NumberOfPrinted;
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
OnigStrCmp (
  const char *Str1,
  const char *Str2
  )
{
  return (int)AsciiStrCmp (Str1, Str2);
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
strlen (
  const char *str
  )
{
  return strlen_s (str, MAX_STRING_SIZE);
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
malloc (
  size_t size
  )
{
  ONIGMEM_HEAD  *PoolHdr;
  UINTN         NewSize;
  VOID          *Data;

  NewSize = (UINTN)(size) + ONIGMEM_OVERHEAD;

  Data = AllocatePool (NewSize);
  if (Data != NULL) {
    PoolHdr = (ONIGMEM_HEAD *)Data;
    PoolHdr->Signature = ONIGMEM_HEAD_SIGNATURE;
    PoolHdr->Size = size;

    return (VOID *)(PoolHdr + 1);
  }

  return NULL;
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
realloc (
  void *ptr,
  size_t size
  )
{
  ONIGMEM_HEAD  *OldPoolHdr;
  ONIGMEM_HEAD  *NewPoolHdr;
  UINTN         OldSize;
  UINTN         NewSize;
  VOID          *Data;

  NewSize = (UINTN)size + ONIGMEM_OVERHEAD;
  Data    = AllocatePool (NewSize);
  if (Data != NULL) {
    NewPoolHdr = (ONIGMEM_HEAD *)Data;
    NewPoolHdr->Signature = ONIGMEM_HEAD_SIGNATURE;
    NewPoolHdr->Size = size;
    if (ptr != NULL) {
      OldPoolHdr = (ONIGMEM_HEAD *)ptr - 1;
      ASSERT (OldPoolHdr->Signature == ONIGMEM_HEAD_SIGNATURE);
      OldSize = OldPoolHdr->Size;

      CopyMem ((VOID *)(NewPoolHdr + 1), ptr, MIN (OldSize, size));
      FreePool ((VOID *)OldPoolHdr);
    }

    return (VOID *)(NewPoolHdr + 1);
  }

  return NULL;
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
  unsigned int count
  )
{
  return CopyMem (dest, src, (UINTN)count);
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
memset (
  void *dest,
  char ch,
  unsigned int count
  )
{
  return SetMem (dest, count, ch);
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
void
free (
  void *ptr
  )
{
  VOID          *EvalOnce;
  ONIGMEM_HEAD  *PoolHdr;

  EvalOnce = ptr;
  if (EvalOnce == NULL) {
    return;
  }

  PoolHdr = (ONIGMEM_HEAD *)EvalOnce - 1;
  if (PoolHdr->Signature == ONIGMEM_HEAD_SIGNATURE) {
    FreePool (PoolHdr);
  } else {
    FreePool (EvalOnce);
  }
}
