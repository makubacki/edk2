/** @file
  The AhciPei driver is used to manage ATA hard disk device working under AHCI
  mode at PEI phase.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _AHCI_PEI_BLOCKIO_H_
#define _AHCI_PEI_BLOCKIO_H_

//
// ATA hard disk device for EFI_PEI_BLOCK_DEVICE_TYPE
//
#define EDKII_PEI_BLOCK_DEVICE_TYPE_ATA_HARD_DISK  8

/**
  Gets the count of block I/O devices that one specific block driver detects.

  This function is used for getting the count of block I/O devices that one
  specific block driver detects. If no device is detected, then the function
  will return zero.

  @param[in]  PeiServices          General-purpose services that are available
                                   to every PEIM.
  @param[in]  This                 Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI
                                   instance.
  @param[out] NumberBlockDevices   The number of block I/O devices discovered.

  @retval     EFI_SUCCESS          The operation performed successfully.

**/
EFI_STATUS
EFIAPI
AhciBlockIoGetDeviceNo (
  IN  EFI_PEI_SERVICES              **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI *This,
  OUT UINTN                         *NumberBlockDevices
  );

/**
  Gets a block device's media information.

  This function will provide the caller with the specified block device's media
  information. If the media changes, calling this function will update the media
  information accordingly.

  @param[in]  PeiServices   General-purpose services that are available to every
                            PEIM
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, the PPIs that
                            want to talk to a single device must specify the
                            device index that was assigned during the enumeration
                            process. This index is a number from one to
                            NumberBlockDevices.
  @param[out] MediaInfo     The media information of the specified block media.
                            The caller is responsible for the ownership of this
                            data structure.

  @par Note:
      The MediaInfo structure describes an enumeration of possible block device
      types.  This enumeration exists because no device paths are actually passed
      across interfaces that describe the type or class of hardware that is publishing
      the block I/O interface. This enumeration will allow for policy decisions
      in the Recovery PEIM, such as "Try to recover from legacy floppy first,
      LS-120 second, CD-ROM third." If there are multiple partitions abstracted
      by a given device type, they should be reported in ascending order; this
      order also applies to nested partitions, such as legacy MBR, where the
      outermost partitions would have precedence in the reporting order. The
      same logic applies to systems such as IDE that have precedence relationships
      like "Master/Slave" or "Primary/Secondary". The master device should be
      reported first, the slave second.

  @retval EFI_SUCCESS        Media information about the specified block device
                             was obtained successfully.
  @retval EFI_DEVICE_ERROR   Cannot get the media information due to a hardware
                             error.

**/
EFI_STATUS
EFIAPI
AhciBlockIoGetMediaInfo (
  IN  EFI_PEI_SERVICES              **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI *This,
  IN  UINTN                         DeviceIndex,
  OUT EFI_PEI_BLOCK_IO_MEDIA        *MediaInfo
  );

/**
  Reads the requested number of blocks from the specified block device.

  The function reads the requested number of blocks from the device. All the
  blocks are read, or an error is returned. If there is no media in the device,
  the function returns EFI_NO_MEDIA.

  @param[in]  PeiServices   General-purpose services that are available to
                            every PEIM.
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, PPIs that
                            want to talk to a single device must specify the device
                            index that was assigned during the enumeration process.
                            This index is a number from one to NumberBlockDevices.
  @param[in]  StartLBA      The starting logical block address (LBA) to read from
                            on the device
  @param[in]  BufferSize    The size of the Buffer in bytes. This number must be
                            a multiple of the intrinsic block size of the device.
  @param[out] Buffer        A pointer to the destination buffer for the data.
                            The caller is responsible for the ownership of the
                            buffer.

  @retval EFI_SUCCESS             The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting
                                  to perform the read operation.
  @retval EFI_INVALID_PARAMETER   The read request contains LBAs that are not
                                  valid, or the buffer is not properly aligned.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_BAD_BUFFER_SIZE     The BufferSize parameter is not a multiple of
                                  the intrinsic block size of the device.

**/
EFI_STATUS
EFIAPI
AhciBlockIoReadBlocks (
  IN  EFI_PEI_SERVICES              **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI *This,
  IN  UINTN                         DeviceIndex,
  IN  EFI_PEI_LBA                   StartLBA,
  IN  UINTN                         BufferSize,
  OUT VOID                          *Buffer
  );

/**
  Gets the count of block I/O devices that one specific block driver detects.

  This function is used for getting the count of block I/O devices that one
  specific block driver detects. If no device is detected, then the function
  will return zero.

  @param[in]  PeiServices          General-purpose services that are available
                                   to every PEIM.
  @param[in]  This                 Indicates the EFI_PEI_RECOVERY_BLOCK_IO2_PPI
                                   instance.
  @param[out] NumberBlockDevices   The number of block I/O devices discovered.

  @retval     EFI_SUCCESS          The operation performed successfully.

**/
EFI_STATUS
EFIAPI
AhciBlockIoGetDeviceNo2 (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO2_PPI *This,
  OUT UINTN                          *NumberBlockDevices
  );

/**
  Gets a block device's media information.

  This function will provide the caller with the specified block device's media
  information. If the media changes, calling this function will update the media
  information accordingly.

  @param[in]  PeiServices   General-purpose services that are available to every
                            PEIM
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO2_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, the PPIs that
                            want to talk to a single device must specify the
                            device index that was assigned during the enumeration
                            process. This index is a number from one to
                            NumberBlockDevices.
  @param[out] MediaInfo     The media information of the specified block media.
                            The caller is responsible for the ownership of this
                            data structure.

  @par Note:
      The MediaInfo structure describes an enumeration of possible block device
      types.  This enumeration exists because no device paths are actually passed
      across interfaces that describe the type or class of hardware that is publishing
      the block I/O interface. This enumeration will allow for policy decisions
      in the Recovery PEIM, such as "Try to recover from legacy floppy first,
      LS-120 second, CD-ROM third." If there are multiple partitions abstracted
      by a given device type, they should be reported in ascending order; this
      order also applies to nested partitions, such as legacy MBR, where the
      outermost partitions would have precedence in the reporting order. The
      same logic applies to systems such as IDE that have precedence relationships
      like "Master/Slave" or "Primary/Secondary". The master device should be
      reported first, the slave second.

  @retval EFI_SUCCESS        Media information about the specified block device
                             was obtained successfully.
  @retval EFI_DEVICE_ERROR   Cannot get the media information due to a hardware
                             error.

**/
EFI_STATUS
EFIAPI
AhciBlockIoGetMediaInfo2 (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO2_PPI *This,
  IN  UINTN                          DeviceIndex,
  OUT EFI_PEI_BLOCK_IO2_MEDIA        *MediaInfo
  );

/**
  Reads the requested number of blocks from the specified block device.

  The function reads the requested number of blocks from the device. All the
  blocks are read, or an error is returned. If there is no media in the device,
  the function returns EFI_NO_MEDIA.

  @param[in]  PeiServices   General-purpose services that are available to
                            every PEIM.
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO2_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, PPIs that
                            want to talk to a single device must specify the device
                            index that was assigned during the enumeration process.
                            This index is a number from one to NumberBlockDevices.
  @param[in]  StartLBA      The starting logical block address (LBA) to read from
                            on the device
  @param[in]  BufferSize    The size of the Buffer in bytes. This number must be
                            a multiple of the intrinsic block size of the device.
  @param[out] Buffer        A pointer to the destination buffer for the data.
                            The caller is responsible for the ownership of the
                            buffer.

  @retval EFI_SUCCESS             The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting
                                  to perform the read operation.
  @retval EFI_INVALID_PARAMETER   The read request contains LBAs that are not
                                  valid, or the buffer is not properly aligned.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_BAD_BUFFER_SIZE     The BufferSize parameter is not a multiple of
                                  the intrinsic block size of the device.

**/
EFI_STATUS
EFIAPI
AhciBlockIoReadBlocks2 (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO2_PPI *This,
  IN  UINTN                          DeviceIndex,
  IN  EFI_PEI_LBA                    StartLBA,
  IN  UINTN                          BufferSize,
  OUT VOID                           *Buffer
  );

#endif
