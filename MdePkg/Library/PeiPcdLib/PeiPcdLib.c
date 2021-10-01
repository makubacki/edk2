/** @file
Implementation of PcdLib class library for PEI phase.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent


**/

#include <PiPei.h>

#include <Ppi/Pcd.h>
#include <Ppi/PiPcd.h>
#include <Ppi/PcdInfo.h>
#include <Ppi/PiPcdInfo.h>

#include <Library/PeiServicesLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

/**
  Retrieve the PCD_PPI pointer.

  This function is to locate PCD_PPI PPI via PeiService.
  If fail to locate PCD_PPI, then ASSERT_EFI_ERROR().

  @retval PCD_PPI * The pointer to the PCD_PPI.

**/
PCD_PPI  *
GetPcdPpiPointer (
  VOID
  )
{
  EFI_STATUS  Status;
  PCD_PPI     *PcdPpi;

  Status = PeiServicesLocatePpi (&gPcdPpiGuid, 0, NULL, (VOID **)&PcdPpi);
  ASSERT_EFI_ERROR (Status);

  return PcdPpi;
}

/**
  Retrieve the pointer of EFI_PEI_PCD_PPI defined in PI 1.2 Vol 3.

  This function is to locate EFI_PEI_PCD_PPI PPI via PeiService.
  If fail to locate EFI_PEI_PCD_PPI, then ASSERT_EFI_ERROR().

  @retval EFI_PEI_PCD_PPI * The pointer to the EFI_PEI_PCD_PPI.

**/
EFI_PEI_PCD_PPI *
GetPiPcdPpiPointer (
  VOID
  )
{
  EFI_STATUS       Status;
  EFI_PEI_PCD_PPI  *PiPcdPpi;

  Status = PeiServicesLocatePpi (&gEfiPeiPcdPpiGuid, 0, NULL, (VOID **)&PiPcdPpi);
  ASSERT_EFI_ERROR (Status);

  return PiPcdPpi;
}

/**
  Retrieve the GET_PCD_INFO_PPI pointer.

  This function is to locate GET_PCD_INFO_PPI PPI via PeiService.
  If fail to locate GET_PCD_INFO_PPI, then ASSERT_EFI_ERROR().

  @retval GET_PCD_INFO_PPI * The pointer to the GET_PCD_INFO_PPI.

**/
GET_PCD_INFO_PPI *
GetPcdInfoPpiPointer (
  VOID
  )
{
  EFI_STATUS        Status;
  GET_PCD_INFO_PPI  *PcdInfoPpi;

  Status = PeiServicesLocatePpi (&gGetPcdInfoPpiGuid, 0, NULL, (VOID **)&PcdInfoPpi);
  ASSERT_EFI_ERROR (Status);

  return PcdInfoPpi;
}

/**
  Retrieve the pointer of EFI_GET_PCD_INFO_PPI defined in PI 1.2.1 Vol 3.

  This function is to locate EFI_GET_PCD_INFO_PPI PPI via PeiService.
  If fail to locate EFI_GET_PCD_INFO_PPI, then ASSERT_EFI_ERROR().

  @retval EFI_GET_PCD_INFO_PPI * The pointer to the EFI_GET_PCD_INFO_PPI.

**/
EFI_GET_PCD_INFO_PPI *
GetPiPcdInfoPpiPointer (
  VOID
  )
{
  EFI_STATUS            Status;
  EFI_GET_PCD_INFO_PPI  *PiPcdInfoPpi;

  Status = PeiServicesLocatePpi (&gEfiGetPcdInfoPpiGuid, 0, NULL, (VOID **)&PiPcdInfoPpi);
  ASSERT_EFI_ERROR (Status);

  return PiPcdInfoPpi;
}

/**
  This function provides a means by which SKU support can be established in the PCD infrastructure.

  Sets the current SKU in the PCD database to the value specified by SkuId.  SkuId is returned.

  @param  SkuId   The SKU value that will be used when the PCD service retrieves
                  and sets values associated with a PCD token.

  @return  Return the SKU ID that just be set.

**/
UINTN
EFIAPI
LibPcdSetSku (
  IN UINTN   SkuId
  )
{
  GetPiPcdPpiPointer ()->SetSku (SkuId);

  return SkuId;
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the 8-bit value for the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the 8-bit value for the token specified by TokenNumber.

**/
UINT8
EFIAPI
LibPcdGet8 (
  IN UINTN             TokenNumber
  )
{
  return (GetPcdPpiPointer ())->Get8 (TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the 16-bit value for the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the 16-bit value for the token specified by TokenNumber.

**/
UINT16
EFIAPI
LibPcdGet16 (
  IN UINTN             TokenNumber
  )
{
  return (GetPcdPpiPointer ())->Get16 (TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the 32-bit value for the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the 32-bit value for the token specified by TokenNumber.

**/
UINT32
EFIAPI
LibPcdGet32 (
  IN UINTN             TokenNumber
  )
{
  return (GetPcdPpiPointer ())->Get32 (TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the 64-bit value for the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the 64-bit value for the token specified by TokenNumber.

**/
UINT64
EFIAPI
LibPcdGet64 (
  IN UINTN             TokenNumber
  )
{
  return (GetPcdPpiPointer ())->Get64 (TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the pointer to the buffer of the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the pointer to the token specified by TokenNumber.

**/
VOID *
EFIAPI
LibPcdGetPtr (
  IN UINTN             TokenNumber
  )
{
  return (GetPcdPpiPointer ())->GetPtr (TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the Boolean value of the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the Boolean value of the token specified by TokenNumber.

**/
BOOLEAN
EFIAPI
LibPcdGetBool (
  IN UINTN             TokenNumber
  )
{
  return (GetPcdPpiPointer ())->GetBool (TokenNumber);
}

/**
  This function provides a means by which to retrieve the size of a given PCD token.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the size of the token specified by TokenNumber.

**/
UINTN
EFIAPI
LibPcdGetSize (
  IN UINTN             TokenNumber
  )
{
  return (GetPcdPpiPointer ())->GetSize (TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the 8-bit value for the token specified by TokenNumber and Guid.

  If Guid is NULL, then ASSERT().

  @param[in]  Guid         The pointer to a 128-bit unique value that designates
                           which namespace to retrieve a value from.
  @param[in]  TokenNumber  The PCD token number to retrieve a current value for.

  @return Return the UINT8.

**/
UINT8
EFIAPI
LibPcdGetEx8 (
  IN CONST GUID        *Guid,
  IN UINTN             TokenNumber
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->Get8 (Guid, TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the 16-bit value for the token specified by TokenNumber and Guid.

  If Guid is NULL, then ASSERT().

  @param[in]  Guid         The pointer to a 128-bit unique value that designates
                           which namespace to retrieve a value from.
  @param[in]  TokenNumber  The PCD token number to retrieve a current value for.

  @return Return the UINT16.

**/
UINT16
EFIAPI
LibPcdGetEx16 (
  IN CONST GUID        *Guid,
  IN UINTN             TokenNumber
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->Get16 (Guid, TokenNumber);
}

/**
  Returns the 32-bit value for the token specified by TokenNumber and Guid.
  If Guid is NULL, then ASSERT().

  @param[in]  Guid         The pointer to a 128-bit unique value that designates
                           which namespace to retrieve a value from.
  @param[in]  TokenNumber  The PCD token number to retrieve a current value for.

  @return Return the UINT32.

**/
UINT32
EFIAPI
LibPcdGetEx32 (
  IN CONST GUID        *Guid,
  IN UINTN             TokenNumber
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->Get32 (Guid, TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the 64-bit value for the token specified by TokenNumber and Guid.

  If Guid is NULL, then ASSERT().

  @param[in]  Guid          The pointer to a 128-bit unique value that designates
                            which namespace to retrieve a value from.
  @param[in]  TokenNumber   The PCD token number to retrieve a current value for.

  @return Return the UINT64.

**/
UINT64
EFIAPI
LibPcdGetEx64 (
  IN CONST GUID        *Guid,
  IN UINTN             TokenNumber
  )
{
  ASSERT (Guid != NULL);
  return (GetPiPcdPpiPointer ())->Get64 (Guid, TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the pointer to the buffer of token specified by TokenNumber and Guid.

  If Guid is NULL, then ASSERT().

  @param[in]  Guid          The pointer to a 128-bit unique value that designates
                            which namespace to retrieve a value from.
  @param[in]  TokenNumber   The PCD token number to retrieve a current value for.

  @return Return the VOID* pointer.

**/
VOID *
EFIAPI
LibPcdGetExPtr (
  IN CONST GUID        *Guid,
  IN UINTN             TokenNumber
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->GetPtr (Guid, TokenNumber);
}

/**
  This function provides a means by which to retrieve a value for a given PCD token.

  Returns the Boolean value of the token specified by TokenNumber and Guid.

  If Guid is NULL, then ASSERT().

  @param[in]  Guid          The pointer to a 128-bit unique value that designates
                            which namespace to retrieve a value from.
  @param[in]  TokenNumber   The PCD token number to retrieve a current value for.

  @return Return the BOOLEAN.

**/
BOOLEAN
EFIAPI
LibPcdGetExBool (
  IN CONST GUID        *Guid,
  IN UINTN             TokenNumber
  )
{
  ASSERT (Guid != NULL);
  return (GetPiPcdPpiPointer ())->GetBool (Guid, TokenNumber);
}

/**
  This function provides a means by which to retrieve the size of a given PCD token.

  Returns the size of the token specified by TokenNumber and Guid.

  If Guid is NULL, then ASSERT().

  @param[in]  Guid          The pointer to a 128-bit unique value that designates
                            which namespace to retrieve a value from.
  @param[in]  TokenNumber   The PCD token number to retrieve a current value for.

  @return Return the size.

**/
UINTN
EFIAPI
LibPcdGetExSize (
  IN CONST GUID        *Guid,
  IN UINTN             TokenNumber
  )
{
  ASSERT (Guid != NULL);
  return (GetPiPcdPpiPointer ())->GetSize (Guid, TokenNumber);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 8-bit value for the token specified by TokenNumber
  to the value specified by Value.

  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 8-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSet8S (
  IN UINTN          TokenNumber,
  IN UINT8          Value
  )
{
  return (GetPcdPpiPointer ())->Set8 (TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 16-bit value for the token specified by TokenNumber
  to the value specified by Value.

  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 16-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSet16S (
  IN UINTN          TokenNumber,
  IN UINT16         Value
  )
{
  return (GetPcdPpiPointer ())->Set16 (TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 32-bit value for the token specified by TokenNumber
  to the value specified by Value.

  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 32-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSet32S (
  IN UINTN          TokenNumber,
  IN UINT32         Value
  )
{
  return (GetPcdPpiPointer ())->Set32 (TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 64-bit value for the token specified by TokenNumber
  to the value specified by Value.

  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 64-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSet64S (
  IN UINTN          TokenNumber,
  IN UINT64         Value
  )
{
  return (GetPcdPpiPointer ())->Set64 (TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets a buffer for the token specified by TokenNumber to the value specified
  by Buffer and SizeOfBuffer. If SizeOfBuffer is greater than the maximum size
  support by TokenNumber, then set SizeOfBuffer to the maximum size supported by
  TokenNumber and return EFI_INVALID_PARAMETER to indicate that the set operation
  was not actually performed.

  If SizeOfBuffer is set to MAX_ADDRESS, then SizeOfBuffer must be set to the
  maximum size supported by TokenName and EFI_INVALID_PARAMETER must be returned.

  If SizeOfBuffer is NULL, then ASSERT().
  If SizeOfBuffer > 0 and Buffer is NULL, then ASSERT().

  @param[in]      TokenNumber   The PCD token number to set a current value for.
  @param[in, out] SizeOfBuffer  The size, in bytes, of Buffer.
  @param[in]      Buffer        A pointer to the buffer to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetPtrS (
  IN       UINTN    TokenNumber,
  IN OUT   UINTN    *SizeOfBuffer,
  IN CONST VOID     *Buffer
  )
{
  ASSERT (SizeOfBuffer != NULL);

  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  return (GetPcdPpiPointer ())->SetPtr (TokenNumber, SizeOfBuffer, (VOID *)Buffer);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the boolean value for the token specified by TokenNumber
  to the value specified by Value.

  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The boolean value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetBoolS (
  IN UINTN          TokenNumber,
  IN BOOLEAN        Value
  )
{
  return (GetPcdPpiPointer ())->SetBool (TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 8-bit value for the token specified by TokenNumber
  to the value specified by Value.

  If Guid is NULL, then ASSERT().

  @param[in] Guid           The pointer to a 128-bit unique value that
                            designates which namespace to set a value from.
  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 8-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetEx8S (
  IN CONST GUID     *Guid,
  IN UINTN          TokenNumber,
  IN UINT8          Value
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->Set8 (Guid, TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 16-bit value for the token specified by TokenNumber
  to the value specified by Value.

  If Guid is NULL, then ASSERT().

  @param[in] Guid           The pointer to a 128-bit unique value that
                            designates which namespace to set a value from.
  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 16-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetEx16S (
  IN CONST GUID     *Guid,
  IN UINTN          TokenNumber,
  IN UINT16         Value
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->Set16 (Guid, TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 32-bit value for the token specified by TokenNumber
  to the value specified by Value.

  If Guid is NULL, then ASSERT().

  @param[in] Guid           The pointer to a 128-bit unique value that
                            designates which namespace to set a value from.
  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 32-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetEx32S (
  IN CONST GUID     *Guid,
  IN UINTN          TokenNumber,
  IN UINT32         Value
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->Set32 (Guid, TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the 64-bit value for the token specified by TokenNumber
  to the value specified by Value.

  If Guid is NULL, then ASSERT().

  @param[in] Guid           The pointer to a 128-bit unique value that
                            designates which namespace to set a value from.
  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The 64-bit value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetEx64S (
  IN CONST GUID     *Guid,
  IN UINTN          TokenNumber,
  IN UINT64         Value
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->Set64 (Guid, TokenNumber, Value);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets a buffer for the token specified by TokenNumber to the value specified by
  Buffer and SizeOfBuffer. If SizeOfBuffer is greater than the maximum size
  support by TokenNumber, then set SizeOfBuffer to the maximum size supported by
  TokenNumber and return EFI_INVALID_PARAMETER to indicate that the set operation
  was not actually performed.

  If Guid is NULL, then ASSERT().
  If SizeOfBuffer is NULL, then ASSERT().
  If SizeOfBuffer > 0 and Buffer is NULL, then ASSERT().

  @param[in]      Guid          Pointer to a 128-bit unique value that
                                designates which namespace to set a value from.
  @param[in]      TokenNumber   The PCD token number to set a current value for.
  @param[in, out] SizeOfBuffer  The size, in bytes, of Buffer.
  @param[in]      Buffer        A pointer to the buffer to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetExPtrS (
  IN CONST GUID     *Guid,
  IN       UINTN    TokenNumber,
  IN OUT   UINTN    *SizeOfBuffer,
  IN       VOID     *Buffer
  )
{
  ASSERT (Guid != NULL);

  ASSERT (SizeOfBuffer != NULL);

  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  return (GetPiPcdPpiPointer ())->SetPtr (Guid, TokenNumber, SizeOfBuffer, Buffer);
}

/**
  This function provides a means by which to set a value for a given PCD token.

  Sets the boolean value for the token specified by TokenNumber
  to the value specified by Value.

  If Guid is NULL, then ASSERT().

  @param[in] Guid           The pointer to a 128-bit unique value that
                            designates which namespace to set a value from.
  @param[in] TokenNumber    The PCD token number to set a current value for.
  @param[in] Value          The boolean value to set.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPcdSetExBoolS (
  IN CONST GUID     *Guid,
  IN UINTN          TokenNumber,
  IN BOOLEAN        Value
  )
{
  ASSERT (Guid != NULL);

  return (GetPiPcdPpiPointer ())->SetBool (Guid, TokenNumber, Value);
}

/**
  Set up a notification function that is called when a specified token is set.

  When the token specified by TokenNumber and Guid is set,
  then notification function specified by NotificationFunction is called.
  If Guid is NULL, then the default token space is used.
  If NotificationFunction is NULL, then ASSERT().

  @param[in]  Guid                  The pointer to a 128-bit unique value that
                                    designates which namespace to set a value from.
                                    If NULL, then the default token space is used.
  @param[in]  TokenNumber           The PCD token number to monitor.
  @param[in]  NotificationFunction  The function to call when the token
                                    specified by Guid and TokenNumber is set.

**/
VOID
EFIAPI
LibPcdCallbackOnSet (
  IN CONST GUID               *Guid, OPTIONAL
  IN UINTN                    TokenNumber,
  IN PCD_CALLBACK             NotificationFunction
  )
{
  EFI_STATUS  Status;

  ASSERT (NotificationFunction != NULL);

  Status = (GetPiPcdPpiPointer ())->CallbackOnSet (Guid, TokenNumber, (EFI_PEI_PCD_PPI_CALLBACK)NotificationFunction);

  ASSERT_EFI_ERROR (Status);

  return;
}

/**
  Disable a notification function that was established with LibPcdCallbackonSet().

  Disable a notification function that was previously established with LibPcdCallbackOnSet().
  If NotificationFunction is NULL, then ASSERT().
  If LibPcdCallbackOnSet() was not previously called with Guid, TokenNumber,
  and NotificationFunction, then ASSERT().

  @param[in]  Guid                 Specify the GUID token space.
  @param[in]  TokenNumber          Specify the token number.
  @param[in]  NotificationFunction The callback function to be unregistered.

**/
VOID
EFIAPI
LibPcdCancelCallback (
  IN CONST GUID               *Guid, OPTIONAL
  IN UINTN                    TokenNumber,
  IN PCD_CALLBACK             NotificationFunction
  )
{
  EFI_STATUS  Status;

  ASSERT (NotificationFunction != NULL);

  Status = (GetPiPcdPpiPointer ())->CancelCallback (Guid, TokenNumber, (EFI_PEI_PCD_PPI_CALLBACK)NotificationFunction);

  ASSERT_EFI_ERROR (Status);

  return;
}

/**
  Retrieves the next token in a token space.

  Retrieves the next PCD token number from the token space specified by Guid.
  If Guid is NULL, then the default token space is used.  If TokenNumber is 0,
  then the first token number is returned.  Otherwise, the token number that
  follows TokenNumber in the token space is returned.  If TokenNumber is the last
  token number in the token space, then 0 is returned.

  If TokenNumber is not 0 and is not in the token space specified by Guid, then ASSERT().

  @param[in]  Guid        The pointer to a 128-bit unique value that designates which namespace
                          to set a value from.  If NULL, then the default token space is used.
  @param[in]  TokenNumber The previous PCD token number.  If 0, then retrieves the first PCD
                          token number.

  @return The next valid token number.

**/
UINTN
EFIAPI
LibPcdGetNextToken (
  IN CONST GUID               *Guid, OPTIONAL
  IN UINTN                    TokenNumber
  )
{
  EFI_STATUS  Status;

  Status = (GetPiPcdPpiPointer ())->GetNextToken (Guid, &TokenNumber);
  ASSERT (!EFI_ERROR (Status) || TokenNumber == 0);

  return TokenNumber;
}

/**
  Used to retrieve the list of available PCD token space GUIDs.

  Returns the PCD token space GUID that follows TokenSpaceGuid in the list of token spaces
  in the platform.
  If TokenSpaceGuid is NULL, then a pointer to the first PCD token spaces returned.
  If TokenSpaceGuid is the last PCD token space GUID in the list, then NULL is returned.

  @param  TokenSpaceGuid  The pointer to the a PCD token space GUID

  @return The next valid token namespace.

**/
GUID *
EFIAPI
LibPcdGetNextTokenSpace (
  IN CONST GUID  *TokenSpaceGuid
  )
{
  (GetPiPcdPpiPointer ())->GetNextTokenSpace (&TokenSpaceGuid);

  return (GUID *)TokenSpaceGuid;
}

/**
  Sets a value of a patchable PCD entry that is type pointer.

  Sets the PCD entry specified by PatchVariable to the value specified by Buffer
  and SizeOfBuffer.  Buffer is returned.  If SizeOfBuffer is greater than
  MaximumDatumSize, then set SizeOfBuffer to MaximumDatumSize and return
  NULL to indicate that the set operation was not actually performed.
  If SizeOfBuffer is set to MAX_ADDRESS, then SizeOfBuffer must be set to
  MaximumDatumSize and NULL must be returned.

  If PatchVariable is NULL, then ASSERT().
  If SizeOfBuffer is NULL, then ASSERT().
  If SizeOfBuffer > 0 and Buffer is NULL, then ASSERT().

  @param[out] PatchVariable     A pointer to the global variable in a module that is
                                the target of the set operation.
  @param[in] MaximumDatumSize   The maximum size allowed for the PCD entry specified by PatchVariable.
  @param[in, out] SizeOfBuffer  A pointer to the size, in bytes, of Buffer.
  @param[in] Buffer             A pointer to the buffer to used to set the target variable.

  @return Return the pointer to the buffer been set.

**/
VOID *
EFIAPI
LibPatchPcdSetPtr (
  OUT       VOID        *PatchVariable,
  IN        UINTN       MaximumDatumSize,
  IN OUT    UINTN       *SizeOfBuffer,
  IN CONST  VOID        *Buffer
  )
{
  ASSERT (PatchVariable != NULL);
  ASSERT (SizeOfBuffer  != NULL);

  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) ||
      (*SizeOfBuffer == MAX_ADDRESS))
  {
    *SizeOfBuffer = MaximumDatumSize;
    return NULL;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);

  return (VOID *)Buffer;
}

/**
  Sets a value of a patchable PCD entry that is type pointer.

  Sets the PCD entry specified by PatchVariable to the value specified
  by Buffer and SizeOfBuffer. If SizeOfBuffer is greater than MaximumDatumSize,
  then set SizeOfBuffer to MaximumDatumSize and return RETURN_INVALID_PARAMETER
  to indicate that the set operation was not actually performed.
  If SizeOfBuffer is set to MAX_ADDRESS, then SizeOfBuffer must be set to
  MaximumDatumSize and RETURN_INVALID_PARAMETER must be returned.

  If PatchVariable is NULL, then ASSERT().
  If SizeOfBuffer is NULL, then ASSERT().
  If SizeOfBuffer > 0 and Buffer is NULL, then ASSERT().

  @param[out] PatchVariable     A pointer to the global variable in a module that is
                                the target of the set operation.
  @param[in] MaximumDatumSize   The maximum size allowed for the PCD entry specified by PatchVariable.
  @param[in, out] SizeOfBuffer  A pointer to the size, in bytes, of Buffer.
  @param[in] Buffer             A pointer to the buffer to used to set the target variable.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPatchPcdSetPtrS (
  OUT      VOID     *PatchVariable,
  IN       UINTN    MaximumDatumSize,
  IN OUT   UINTN    *SizeOfBuffer,
  IN CONST VOID     *Buffer
  )
{
  ASSERT (PatchVariable != NULL);
  ASSERT (SizeOfBuffer  != NULL);

  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) ||
      (*SizeOfBuffer == MAX_ADDRESS))
  {
    *SizeOfBuffer = MaximumDatumSize;
    return RETURN_INVALID_PARAMETER;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);

  return RETURN_SUCCESS;
}

/**
  Sets a value and size of a patchable PCD entry that is type pointer.

  Sets the PCD entry specified by PatchVariable to the value specified by Buffer
  and SizeOfBuffer.  Buffer is returned.  If SizeOfBuffer is greater than
  MaximumDatumSize, then set SizeOfBuffer to MaximumDatumSize and return
  NULL to indicate that the set operation was not actually performed.
  If SizeOfBuffer is set to MAX_ADDRESS, then SizeOfBuffer must be set to
  MaximumDatumSize and NULL must be returned.

  If PatchVariable is NULL, then ASSERT().
  If SizeOfPatchVariable is NULL, then ASSERT().
  If SizeOfBuffer is NULL, then ASSERT().
  If SizeOfBuffer > 0 and Buffer is NULL, then ASSERT().

  @param[out] PatchVariable     A pointer to the global variable in a module that is
                                the target of the set operation.
  @param[out] SizeOfPatchVariable A pointer to the size, in bytes, of PatchVariable.
  @param[in] MaximumDatumSize   The maximum size allowed for the PCD entry specified by PatchVariable.
  @param[in, out] SizeOfBuffer  A pointer to the size, in bytes, of Buffer.
  @param[in] Buffer             A pointer to the buffer to used to set the target variable.

  @return Return the pointer to the buffer been set.

**/
VOID *
EFIAPI
LibPatchPcdSetPtrAndSize (
  OUT       VOID        *PatchVariable,
  OUT       UINTN       *SizeOfPatchVariable,
  IN        UINTN       MaximumDatumSize,
  IN OUT    UINTN       *SizeOfBuffer,
  IN CONST  VOID        *Buffer
  )
{
  ASSERT (PatchVariable != NULL);
  ASSERT (SizeOfPatchVariable != NULL);
  ASSERT (SizeOfBuffer  != NULL);

  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) ||
      (*SizeOfBuffer == MAX_ADDRESS))
  {
    *SizeOfBuffer = MaximumDatumSize;
    return NULL;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);
  *SizeOfPatchVariable = *SizeOfBuffer;

  return (VOID *)Buffer;
}

/**
  Sets a value and size of a patchable PCD entry that is type pointer.

  Sets the PCD entry specified by PatchVariable to the value specified
  by Buffer and SizeOfBuffer. If SizeOfBuffer is greater than MaximumDatumSize,
  then set SizeOfBuffer to MaximumDatumSize and return RETURN_INVALID_PARAMETER
  to indicate that the set operation was not actually performed.
  If SizeOfBuffer is set to MAX_ADDRESS, then SizeOfBuffer must be set to
  MaximumDatumSize and RETURN_INVALID_PARAMETER must be returned.

  If PatchVariable is NULL, then ASSERT().
  If SizeOfPatchVariable is NULL, then ASSERT().
  If SizeOfBuffer is NULL, then ASSERT().
  If SizeOfBuffer > 0 and Buffer is NULL, then ASSERT().

  @param[out] PatchVariable     A pointer to the global variable in a module that is
                                the target of the set operation.
  @param[out] SizeOfPatchVariable A pointer to the size, in bytes, of PatchVariable.
  @param[in] MaximumDatumSize   The maximum size allowed for the PCD entry specified by PatchVariable.
  @param[in, out] SizeOfBuffer  A pointer to the size, in bytes, of Buffer.
  @param[in] Buffer             A pointer to the buffer to used to set the target variable.

  @return The status of the set operation.

**/
RETURN_STATUS
EFIAPI
LibPatchPcdSetPtrAndSizeS (
  OUT      VOID     *PatchVariable,
  OUT      UINTN    *SizeOfPatchVariable,
  IN       UINTN    MaximumDatumSize,
  IN OUT   UINTN    *SizeOfBuffer,
  IN CONST VOID     *Buffer
  )
{
  ASSERT (PatchVariable != NULL);
  ASSERT (SizeOfPatchVariable != NULL);
  ASSERT (SizeOfBuffer  != NULL);

  if (*SizeOfBuffer > 0) {
    ASSERT (Buffer != NULL);
  }

  if ((*SizeOfBuffer > MaximumDatumSize) ||
      (*SizeOfBuffer == MAX_ADDRESS))
  {
    *SizeOfBuffer = MaximumDatumSize;
    return RETURN_INVALID_PARAMETER;
  }

  CopyMem (PatchVariable, Buffer, *SizeOfBuffer);
  *SizeOfPatchVariable = *SizeOfBuffer;

  return RETURN_SUCCESS;
}

/**
  Retrieve additional information associated with a PCD token.

  This includes information such as the type of value the TokenNumber is associated with as well as possible
  human readable name that is associated with the token.

  If TokenNumber is not in the default token space specified, then ASSERT().

  @param[in]    TokenNumber The PCD token number.
  @param[out]   PcdInfo     The returned information associated with the requested TokenNumber.
                            The caller is responsible for freeing the buffer that is allocated by callee for PcdInfo->PcdName.
**/
VOID
EFIAPI
LibPcdGetInfo (
  IN        UINTN           TokenNumber,
  OUT       PCD_INFO        *PcdInfo
  )
{
  EFI_STATUS  Status;

  Status = GetPcdInfoPpiPointer ()->GetInfo (TokenNumber, (EFI_PCD_INFO *)PcdInfo);
  ASSERT_EFI_ERROR (Status);
}

/**
  Retrieve additional information associated with a PCD token.

  This includes information such as the type of value the TokenNumber is associated with as well as possible
  human readable name that is associated with the token.

  If TokenNumber is not in the token space specified by Guid, then ASSERT().

  @param[in]    Guid        The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]    TokenNumber The PCD token number.
  @param[out]   PcdInfo     The returned information associated with the requested TokenNumber.
                            The caller is responsible for freeing the buffer that is allocated by callee for PcdInfo->PcdName.
**/
VOID
EFIAPI
LibPcdGetInfoEx (
  IN CONST  GUID            *Guid,
  IN        UINTN           TokenNumber,
  OUT       PCD_INFO        *PcdInfo
  )
{
  EFI_STATUS  Status;

  Status = GetPiPcdInfoPpiPointer ()->GetInfo (Guid, TokenNumber, (EFI_PCD_INFO *)PcdInfo);
  ASSERT_EFI_ERROR (Status);
}

/**
  Retrieve the currently set SKU Id.

  @return   The currently set SKU Id. If the platform has not set at a SKU Id, then the
            default SKU Id value of 0 is returned. If the platform has set a SKU Id, then the currently set SKU
            Id is returned.
**/
UINTN
EFIAPI
LibPcdGetSku (
  VOID
  )
{
  return GetPiPcdInfoPpiPointer ()->GetSku ();
}
