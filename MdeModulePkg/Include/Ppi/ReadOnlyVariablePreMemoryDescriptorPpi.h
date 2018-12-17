/** @file
  This EDKII-specific variable PPI is used to store the address of the Read Only Variable Pre-Memory
  Descriptor PPI. When PEIM shadow occurs, the PEI variable driver can shadow its unique instance of
  the READ_ONLY_VARIABLE2_PPI to permanent memory.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

This program and the accompanying materials
are licensed and made available under the terms and conditions
of the BSD License which accompanies this distribution.  The
full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _PEI_READ_ONLY_VARIABLE_PRE_MEMORY_DESC_PPI_H_
#define _PEI_READ_ONLY_VARIABLE_PRE_MEMORY_DESC_PPI_H_

extern EFI_GUID gPeiReadOnlyVariablePreMemoryDescriptorPpiGuid;

typedef struct {
  EFI_PEI_PPI_DESCRIPTOR    *PreMemoryDescriptor;
} READ_ONLY_VARIABLE_PRE_MEMORY_DESCRIPTOR_PPI;

#endif