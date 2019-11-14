/** @file
  This file declares the Convert Pointer PPI.

  This PPI is published by the PEI foundation to provide a service for pointer
  conversion. Pointers that are eligible to be converted include those to
  addresses in pre-memory firmware volumes.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PEI_CONVERT_POINTER_PPI_H__
#define __PEI_CONVERT_POINTER_PPI_H__

#define PEI_CONVERT_POINTER_PPI_GUID \
  { \
    0x4b6e407d, 0x5273, 0x4092, {0xad, 0x32, 0x96, 0x11, 0x41, 0x8a, 0x3b, 0x31} \
  }

typedef struct _PEI_CONVERT_POINTER_PPI PEI_CONVERT_POINTER_PPI;

/**
  This interface provides a service to convert a pointer to an address in a pre-memory FV
  to the corresponding address in the permanent memory FV.

  This service is published by the PEI Foundation if the PEI Foundation migrated pre-memory
  FVs to permanent memory. This service is published before temporary RAM is disabled and
  can be used by PEIMs to convert pointer such as code function pointers to the permanent
  memory address before the code in pre-memory is invalidated.

  @param[in]  PeiServices         The pointer to the PEI Services Table.
  @param[in]  This                The pointer to this instance of the PEI_CONVERT_POINTER_PPI.
  @param[in]  Address             The pointer to a pointer that is to be converted from
                                  a pre-memory address in a FV to the corresponding  permanent memory
                                  address in a FV.

  @retval EFI_SUCCESS             The pointer pointed to by Address was modified successfully.
  @retval EFI_NOT_FOUND           The address at the pointer pointed to by Address was not found
                                  in a pre-memory FV address range.
  @retval EFI_INVALID_PARAMETER   Address is NULL or *Address is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *PEI_CONVERT_POINTER) (
  IN CONST EFI_PEI_SERVICES          **PeiServices,
  IN       PEI_CONVERT_POINTER_PPI   *This,
  IN       VOID                      **Address
  );

struct _PEI_CONVERT_POINTER_PPI {
  PEI_CONVERT_POINTER     ConvertPointer;
};

extern EFI_GUID gEfiPeiConvertPointerPpiGuid;

#endif
