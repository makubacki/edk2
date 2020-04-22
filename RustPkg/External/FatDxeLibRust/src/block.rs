// Copyright © 2019-2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#![allow(unused)]

use core::fmt;
use r_efi::efi;
use r_efi::protocols::block_io::Protocol as EfiBlockIoProtocol;
use r_efi::protocols::block_io::PROTOCOL_GUID as EfiBlockIoProtocolGuid;

pub type Error = efi::Status;

pub trait SectorRead {
    /// Read a single sector (512 bytes) from the block device. `data` must be
    /// exactly 512 bytes long.
    fn read(&self, sector: u64, data: &mut [u8]) -> Result<(), Error>;
}

pub trait SectorWrite {
    /// Write a single sector (512 bytes) from the block device. `data` must be
    /// exactly 512 bytes long.
    fn write(&self, sector: u64, data: &mut [u8]) -> Result<(), Error>;
    fn flush(&self) -> Result<(), Error>;
}

pub struct BlockIoDevice {
    inner: Option<*mut EfiBlockIoProtocol>,

    // set by new function
    pub media_id: u32,
    pub block_size: u32,
    pub logical_partition: bool,
}

impl BlockIoDevice {
    pub fn new(block_io: *mut EfiBlockIoProtocol) -> BlockIoDevice {
        unsafe {
            BlockIoDevice {
                inner: Some(block_io),
                media_id: (*(*block_io).media).media_id,
                block_size: (*(*block_io).media).block_size,
                logical_partition: (*(*block_io).media).logical_partition,
            }
        }
    }
}

impl SectorRead for BlockIoDevice {
    fn read(&self, sector: u64, data: &mut [u8]) -> Result<(), Error> {
        if let Some(mut block_io) = self.inner {
            unsafe {
                let media_id = (*((*block_io).media)).media_id;
                let status = ((*block_io).read_blocks)(
                    block_io,
                    media_id,
                    sector,
                    data.len(),
                    data as *mut [u8] as *mut core::ffi::c_void,
                );
                if status.is_error() {
                    crate::log!("block io read error: {:x}", status.value());
                    return Err(status);
                }
            }
            Ok(())
        } else {
            Err(efi::Status::DEVICE_ERROR)
        }
    }
}

impl SectorWrite for BlockIoDevice {
    fn write(&self, _sector: u64, _data: &mut [u8]) -> Result<(), Error> {
        if let Some(mut block_io) = self.inner {
            Ok(())
        } else {
            Err(efi::Status::DEVICE_ERROR)
        }
    }

    fn flush(&self) -> Result<(), Error> {
        if let Some(mut block_io) = self.inner {
            Ok(())
        } else {
            Err(efi::Status::DEVICE_ERROR)
        }
    }
}
