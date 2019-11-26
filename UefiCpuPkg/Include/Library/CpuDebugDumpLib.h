/** @file
  This library contains functions to dump CPU structure information.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CPU_DEBUG_DUMP_LIB_H__
#define __CPU_DEBUG_DUMP_LIB_H__

/**
  Dumps the Interrupt Descriptor Table (IDT) to DEBUG output.

**/
VOID
EFIAPI
DumpIdt (
  VOID
  );

/**
  Dumps the Global Descriptor Table (GDT) to DEBUG output.

**/
VOID
EFIAPI
DumpGdt (
  VOID
  );

#endif
