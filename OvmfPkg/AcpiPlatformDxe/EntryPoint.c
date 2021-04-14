/** @file
  Entry point of OVMF ACPI Platform Driver

  Copyright (C) 2015, Red Hat, Inc.
  Copyright (c) 2008 - 2015, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Guid/RootBridgesConnectedEventGroup.h>
#include "AcpiPlatform.h"

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
STATIC
EFI_ACPI_TABLE_PROTOCOL *
FindAcpiTableProtocol (
  VOID
  )
{
  EFI_STATUS               Status;
  EFI_ACPI_TABLE_PROTOCOL  *AcpiTable;

  Status = gBS->LocateProtocol (
                                &gEfiAcpiTableProtocolGuid,
                                NULL,
                                (VOID **) &AcpiTable
                                );
  ASSERT_EFI_ERROR (Status);
  return AcpiTable;
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
STATIC
VOID
EFIAPI
OnRootBridgesConnected (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS  Status;

  DEBUG (
         (DEBUG_INFO,
          "%a: root bridges have been connected, installing ACPI tables\n",
          __FUNCTION__)
         );
  Status = InstallAcpiTables (FindAcpiTableProtocol ());
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallAcpiTables: %r\n", __FUNCTION__, Status));
  }

  gBS->CloseEvent (Event);
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
EFI_STATUS
EFIAPI
AcpiPlatformEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   RootBridgesConnected;

  //
  // If the platform doesn't support PCI, or PCI enumeration has been disabled,
  // install the tables at once, and let the entry point's return code reflect
  // the full functionality.
  //
  if (PcdGetBool (PcdPciDisableBusEnumeration)) {
    DEBUG (
           (DEBUG_INFO, "%a: PCI or its enumeration disabled, installing "
                        "ACPI tables\n", __FUNCTION__)
           );
    return InstallAcpiTables (FindAcpiTableProtocol ());
  }

  //
  // Otherwise, delay installing the ACPI tables until root bridges are
  // connected. The entry point's return status will only reflect the callback
  // setup. (Note that we're a DXE_DRIVER; our entry point function is invoked
  // strictly before BDS is entered and can connect the root bridges.)
  //
  Status = gBS->CreateEventEx (
                               EVT_NOTIFY_SIGNAL,
                               TPL_CALLBACK,
                               OnRootBridgesConnected,
                               NULL /* Context */,
                               &gRootBridgesConnectedEventGroupGuid,
                               &RootBridgesConnected
                               );
  if (!EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_INFO,
            "%a: waiting for root bridges to be connected, registered callback\n",
            __FUNCTION__)
           );
  }

  return Status;
}
