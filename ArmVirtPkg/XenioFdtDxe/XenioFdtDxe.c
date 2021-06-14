/** @file
*  Xenio FDT client protocol driver for xen,xen DT node
*
*  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/XenIoMmioLib.h>

#include <Protocol/FdtClient.h>

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
EFI_STATUS
EFIAPI
InitializeXenioFdtDxe (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS           Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;
  CONST UINT64         *Reg;
  UINT32               RegSize;
  UINTN                AddressCells, SizeCells;
  EFI_HANDLE           Handle;
  UINT64               RegBase;

  Status = gBS->LocateProtocol (
                  &gFdtClientProtocolGuid,
                  NULL,
                  (VOID **)&FdtClient
                  );
  ASSERT_EFI_ERROR (Status);

  Status = FdtClient->FindCompatibleNodeReg (
                        FdtClient,
                        "xen,xen",
                        (CONST VOID **)&Reg,
                        &AddressCells,
                        &SizeCells,
                        &RegSize
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      EFI_D_WARN,
      "%a: No 'xen,xen' compatible DT node found\n",
      __FUNCTION__
      ));
    return EFI_UNSUPPORTED;
  }

  ASSERT (AddressCells == 2);
  ASSERT (SizeCells == 2);
  ASSERT (RegSize == 2 * sizeof (UINT64));

  //
  // Retrieve the reg base from this node and wire it up to the
  // MMIO flavor of the XenBus root device I/O protocol
  //
  RegBase = SwapBytes64 (Reg[0]);
  Handle  = NULL;
  Status  = XenIoMmioInstall (&Handle, RegBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: XenIoMmioInstall () failed on a new handle "
      "(Status == %r)\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  DEBUG ((EFI_D_INFO, "Found Xen node with Grant table @ 0x%Lx\n", RegBase));

  return EFI_SUCCESS;
}
