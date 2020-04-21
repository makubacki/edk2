// Copyright © 2019 Intel Corporation
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
use crate::block::SectorRead;

#[repr(packed)]
struct Header {
    _magic: [u8; 3],
    _identifier: [u8; 8],
    bytes_per_sector: u16,
    sectors_per_cluster: u8,
    reserved_sectors: u16,
    fat_count: u8,
    root_dir_count: u16,
    legacy_sectors: u16,
    _media_type: u8,
    legacy_sectors_per_fat: u16,
    _sectors_per_track: u16,
    _head_count: u16,
    _hidden_sectors: u32,
    sectors: u32,
}

#[repr(packed)]
struct Fat32Header {
    _header: Header,
    sectors_per_fat: u32,
    _flags: u16,
    _version: u16,
    root_cluster: u32,
    _fsinfo_sector: u16,
    _backup_boot_sector: u16,
    _reserved: [u8; 12],
    _drive_no: u8,
    _nt_flags: u8,
    _signature: u8,
    _serial: u32,
    _volume: [u8; 11],
    _id: [u8; 8],
}

#[repr(packed)]
struct FatDirectory {
    name: [u8; 11],
    flags: u8,
    _unused1: [u8; 8],
    cluster_high: u16,
    _unused2: [u8; 4],
    cluster_low: u16,
    size: u32,
}

#[repr(packed)]
struct FatLongNameEntry {
    seq: u8,
    name: [u16; 5],
    _attr: u8,
    r#_type: u8,
    _checksum: u8,
    name2: [u16; 6],
    _cluster: u16,
    name3: [u16; 2],
}

#[derive(Debug, PartialEq, Clone)]
pub enum FatType {
    Unknown,
    FAT12,
    FAT16,
    FAT32,
}

pub struct Filesystem<'a> {
    pub device: &'a dyn SectorRead,
    pub start: u64,
    pub last: u64,
    pub part_id: u32,
    pub bytes_per_sector: u32,
    pub sectors: u32,
    pub fat_type: FatType,
    pub clusters: u32,
    pub sectors_per_fat: u32,
    pub sectors_per_cluster: u32,
    pub fat_count: u32,
    pub root_dir_sectors: u32,
    pub first_fat_sector: u32,
    pub first_data_sector: u32,
    pub data_sector_count: u32,
    pub data_cluster_count: u32,
    pub root_cluster: u32, // FAT32 only
}

#[derive(Clone, Copy)]
pub struct DirectoryEntry {
    pub name: [u8; 11],
    pub long_name: [u8; 255],
    pub file_type: FileType,
    pub size: u32,
    pub cluster: u32,
}

#[derive(Debug, PartialEq)]
pub enum Error {
    BlockError,
    Unsupported,
    NotFound,
    EndOfFile,
    InvalidOffset,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum FileType {
    File,
    Directory,
}

pub struct File<'a> {
    filesystem: &'a Filesystem<'a>,
    start_cluster: u32,
    active_cluster: u32,
    sector_offset: u64,
    size: u32,
    position: u32,
}

pub struct Directory<'a> {
    filesystem: &'a Filesystem<'a>,
    pub cluster: Option<u32>,
    sector: u32,
    offset: usize,
    cluster_start: Option<u32>,
}

fn ucs2_to_ascii(input: &[u16]) -> [u8; 255] {
    let mut output: [u8; 255] = [0; 255];
    let mut i: usize = 0;
    while i < output.len() {
        output[i] = (input[i] & 0xffu16) as u8;
        if output[i] == 0 {
            break;
        }
        i += 1;
    }
    output
}

fn get_short_name(input: &[u8; 11]) -> [u8; 11] {
    let mut index = 0;
    let mut i = 0;
    let mut name_vec: [u8;11] = [0;11];
    for i in 0..8  {
        if input[i] != 32 {
            name_vec[index] = input[i];
            index += 1;
        }
    }
    if input[8] != 32 {
        name_vec[index] = 46;
        index += 1;
        for i in 8..11 {
            if input[i] !=32 {
                name_vec[index] = name_vec[i];
            }
        }
    }
    name_vec
}

impl<'a> Directory<'a> {
    // Returns and then increments to point to the next one, may return EndOfFile if this is the last entry
    pub fn next_entry(&mut self) -> Result<DirectoryEntry, Error> {
        let mut long_entry = [0u16; 260];
        loop {
            let sector = self.get_sector()?;

            let mut data: [u8; 512] = [0; 512];
            match self.filesystem.read(u64::from(sector), &mut data) {
                Ok(_) => {}
                Err(_) => return Err(Error::BlockError),
            };

            let dirs: &[FatDirectory] = unsafe {
                core::slice::from_raw_parts(data.as_ptr() as *const FatDirectory, 512 / 32)
            };

            let lfns: &[FatLongNameEntry] = unsafe {
                core::slice::from_raw_parts(data.as_ptr() as *const FatLongNameEntry, 512 / 32)
            };

            for i in self.offset..dirs.len() {
                let d = &dirs[i];
                // Last entry
                if d.name[0] == 0x0 {
                    return Err(Error::EndOfFile);
                }
                // Directory unused
                if d.name[0] == 0xe5 {
                    continue;
                }
                // LFN entry
                if d.flags == 0x0f {
                    // DOS starts sequences as 1. LFN entries come in reverse order before
                    // actual entry so populate the slice using the sequence.
                    let lfn_seq = ((lfns[i].seq & 0x1f) as usize) - 1;
                    let lfn_block = &mut long_entry[lfn_seq * 13..(lfn_seq + 1) * 13];

                    let s = &mut lfn_block[0..5];
                    s.copy_from_slice(unsafe { &lfns[i].name[..] });
                    let s = &mut lfn_block[5..11];
                    s.copy_from_slice(unsafe { &lfns[i].name2[..] });
                    let s = &mut lfn_block[11..13];
                    s.copy_from_slice(unsafe { &lfns[i].name3[..] });

                    continue;
                }
                let shortname = get_short_name(&d.name);
                let entry = DirectoryEntry {
                    name: shortname,
                    file_type: if d.flags & 0x10 == 0x10 {
                        FileType::Directory
                    } else {
                        FileType::File
                    },
                    cluster: (u32::from(d.cluster_high)) << 16 | u32::from(d.cluster_low),
                    size: d.size,
                    long_name: ucs2_to_ascii(&long_entry[..]),
                };

                self.offset = i + 1;
                return Ok(entry);
            }
            self.sector += 1;
            self.offset = 0;
        }
    }

    fn get_sector(&mut self) ->  Result<u32, Error> {
        let sector = if self.cluster.is_some() {
            if self.sector >= self.filesystem.sectors_per_cluster {
                match self.filesystem.next_cluster(self.cluster.unwrap()) {
                    Ok(new_cluster) => {
                        self.cluster = Some(new_cluster);
                        self.sector = 0;
                        self.offset = 0;
                    }
                    Err(e) => {
                        return Err(e);
                    }
                }
            }
            self.sector
                + self
                    .filesystem
                    .first_sector_of_cluster(self.cluster.unwrap())
        } else {
            self.sector
        };
        Ok(sector)
    }
}

pub trait Read {
    fn read(&mut self, data: &mut [u8]) -> Result<u32, Error>;
    fn seek(&mut self, offset: u32) -> Result<(), Error>;
    fn get_size(&self) -> u32;
}

impl<'a> Read for File<'a> {
    fn read(&mut self, data: &mut [u8]) -> Result<u32, Error> {
        assert_eq!(data.len(), 512);

        if self.position >= self.size {
            return Err(Error::EndOfFile);
        }

        if self.sector_offset == u64::from(self.filesystem.sectors_per_cluster) {
            match self.filesystem.next_cluster(self.active_cluster) {
                Err(e) => {
                    return Err(e);
                }
                Ok(cluster) => {
                    self.active_cluster = cluster;
                    self.sector_offset = 0;
                }
            }
        }

        let cluster_start = self.filesystem.first_sector_of_cluster(self.active_cluster);

        match self
            .filesystem
            .read(u64::from(cluster_start) + self.sector_offset, data)
        {
            Err(_) => Err(Error::BlockError),
            Ok(()) => {
                self.sector_offset += 1;
                if (self.position + 512) > self.size {
                    let bytes_read = self.size - self.position;
                    self.position = self.size;
                    Ok(bytes_read)
                } else {
                    self.position += 512;
                    Ok(512)
                }
            }
        }
    }

    fn seek(&mut self, position: u32) -> Result<(), Error> {
        if position % 512 != 0 {
            return Err(Error::InvalidOffset);
        }

        if position >= self.size {
            return Err(Error::EndOfFile);
        }

        // Beyond, reset to zero and come back
        if position < self.position {
            self.position = 0;
            self.sector_offset = 0;
            self.active_cluster = self.start_cluster;
        }

        // Like read but without reading, follow cluster chain if we reach end of cluster
        while self.position != position {
            if self.sector_offset == u64::from(self.filesystem.sectors_per_cluster) {
                match self.filesystem.next_cluster(self.active_cluster) {
                    Err(e) => {
                        return Err(e);
                    }
                    Ok(cluster) => {
                        self.active_cluster = cluster;
                        self.sector_offset = 0;
                    }
                }
            }

            self.sector_offset += 1;
            self.position += 512;
        }

        Ok(())
    }
    fn get_size(&self) -> u32 {
        self.size
    }
}

impl<'a> SectorRead for Filesystem<'a> {
    fn read(&self, sector: u64, data: &mut [u8]) -> Result<(), crate::block::Error> {
        if self.start + sector > self.last {
            Err(crate::block::Error::DEVICE_ERROR)
        } else {
            self.device.read(self.start + sector, data)
        }
    }
}

// Do a case-insensitive match on the name with the 8.3 format that you get from FAT.
// In the FAT directory entry the "." isn't stored and any gaps are padded with " ".
fn compare_short_name(name: &str, de: &DirectoryEntry) -> bool {
    // 8.3 (plus 1 for the separator)
    if name.len() > 12 {
        return false;
    }

    let mut i = 0;
    for (_, a) in name.as_bytes().iter().enumerate() {
        // Handle cases which are 11 long but not 8.3 (e.g "loader.conf")
        if i == 11 {
            return false;
        }

        // Jump to the extension
        if *a == b'.' {
            i = 8;
            continue;
        }

        let b = de.name[i];
        if a.to_ascii_uppercase() != b.to_ascii_uppercase() {
            return false;
        }

        i += 1;
    }
    true
}

fn compare_name(name: &str, de: &DirectoryEntry) -> bool {
    compare_short_name(name, de) || &de.long_name[0..name.len()] == name.as_bytes()
}

impl<'a> Filesystem<'a> {
    pub fn new(device: &'a dyn SectorRead, start: u64, last: u64, part_id: u32) -> Filesystem {
        Filesystem {
            device,
            start,
            last,
            part_id,
            bytes_per_sector: 0,
            sectors: 0,
            fat_type: FatType::Unknown,
            clusters: 0,
            sectors_per_fat: 0,
            sectors_per_cluster: 0,
            fat_count: 0,
            root_dir_sectors: 0,
            first_fat_sector: 0,
            first_data_sector: 0,
            data_sector_count: 0,
            data_cluster_count: 0,
            root_cluster: 0,
        }
    }

    pub fn init(&mut self) -> Result<(), Error> {
        const FAT12_MAX: u32 = 0xff5;
        const FAT16_MAX: u32 = 0xfff5;

        let mut data: [u8; 512] = [0; 512];
        match self.read(0, &mut data) {
            Ok(_) => {}
            Err(_) => {
                return Err(Error::BlockError)
            },
        };

        let h = unsafe { &*(data.as_ptr() as *const Header) };

        self.bytes_per_sector = u32::from(h.bytes_per_sector);
        self.fat_count = u32::from(h.fat_count);
        self.sectors_per_cluster = u32::from(h.sectors_per_cluster);

        self.sectors = if h.legacy_sectors == 0 {
            h.sectors
        } else {
            u32::from(h.legacy_sectors)
        };

        self.clusters = self.sectors / u32::from(h.sectors_per_cluster);

        self.fat_type = if self.clusters < FAT12_MAX {
            FatType::FAT12
        } else if self.clusters < FAT16_MAX {
            FatType::FAT16
        } else {
            FatType::FAT32
        };

        if self.fat_type == FatType::FAT32 {
            let h32 = unsafe { &*(data.as_ptr() as *const Fat32Header) };
            self.sectors_per_fat = h32.sectors_per_fat;
            self.root_cluster = h32.root_cluster;
        } else {
            self.sectors_per_fat = u32::from(h.legacy_sectors_per_fat);
        }

        if self.fat_type == FatType::FAT12 || self.fat_type == FatType::FAT16 {
            self.root_dir_sectors = ((u32::from(h.root_dir_count * 32)) + self.bytes_per_sector
                - 1)
                / self.bytes_per_sector;
        }

        self.first_fat_sector = u32::from(h.reserved_sectors);
        self.first_data_sector =
            self.first_fat_sector + (self.fat_count * self.sectors_per_fat) + self.root_dir_sectors;
        self.data_sector_count = self.sectors - self.first_data_sector;
        self.data_cluster_count = self.data_sector_count / self.bytes_per_sector;

        Ok(())
    }

    fn next_cluster(&self, cluster: u32) -> Result<u32, Error> {
        match self.fat_type {
            FatType::FAT12 => {
                let mut data: [u8; 512] = [0; 512];

                let fat_offset = cluster + (cluster / 2); // equivalent of x 1.5
                let fat_sector = self.first_fat_sector + (fat_offset / self.bytes_per_sector);
                let offset = fat_offset % self.bytes_per_sector;

                match self.read(u64::from(fat_sector), &mut data) {
                    Ok(_) => {}
                    Err(_) => return Err(Error::BlockError),
                };

                let next_cluster_raw =
                    unsafe { *((data.as_ptr() as u64 + u64::from(offset)) as *const u16) };

                let next_cluster = if cluster % 2 == 0 {
                    next_cluster_raw & 0xfff
                } else {
                    next_cluster_raw >> 4
                };
                if next_cluster >= 0xff8 {
                    Err(Error::EndOfFile)
                } else {
                    Ok(u32::from(next_cluster))
                }
            }
            FatType::FAT16 => {
                let fat: [u16; 512 / 2] = [0; 512 / 2];

                let fat_offset = cluster * 2;
                let fat_sector = self.first_fat_sector + (fat_offset / self.bytes_per_sector);
                let offset = fat_offset % self.bytes_per_sector;

                let data = unsafe { core::slice::from_raw_parts_mut(fat.as_ptr() as *mut u8, 512) };
                match self.read(u64::from(fat_sector), data) {
                    Ok(_) => {}
                    Err(_) => return Err(Error::BlockError),
                };

                let next_cluster = fat[(offset / 2) as usize];

                if next_cluster >= 0xfff8 {
                    Err(Error::EndOfFile)
                } else {
                    Ok(u32::from(next_cluster))
                }
            }
            FatType::FAT32 => {
                let fat: [u32; 512 / 4] = [0; 512 / 4];

                let fat_offset = cluster * 4;
                let fat_sector = self.first_fat_sector + (fat_offset / self.bytes_per_sector);
                let offset = fat_offset % self.bytes_per_sector;

                let data = unsafe { core::slice::from_raw_parts_mut(fat.as_ptr() as *mut u8, 512) };

                match self.read(u64::from(fat_sector), data) {
                    Ok(_) => {}
                    Err(_) => return Err(Error::BlockError),
                };

                let next_cluster_raw = fat[(offset / 4) as usize];
                let next_cluster = next_cluster_raw & 0x0fff_ffff;
                if next_cluster >= 0x0fff_fff8 {
                    Err(Error::EndOfFile)
                } else {
                    Ok(next_cluster)
                }
            }

            _ => {
                crate::log!("next_cluster unsupported error!\n");
                Err(Error::Unsupported)
            },
        }
    }

    fn first_sector_of_cluster(&self, cluster: u32) -> u32 {
        ((cluster - 2) * self.sectors_per_cluster) + self.first_data_sector
    }

    pub fn root(&self) -> Result<Directory, Error> {
        match self.fat_type {
            FatType::FAT12 | FatType::FAT16 => {
                let root_directory_start = self.first_data_sector - self.root_dir_sectors;
                Ok(Directory {
                    filesystem: self,
                    cluster: None,
                    sector: root_directory_start,
                    offset: 0,
                    cluster_start: None
                })
            }
            FatType::FAT32 => Ok(Directory {
                filesystem: self,
                cluster: Some(self.root_cluster),
                sector: 0,
                offset: 0,
                cluster_start: Some(self.root_cluster)
            }),
            _ => {
                crate::log!("root unsupported error!\n");
                Err(Error::Unsupported)
            },
        }
    }

    pub fn get_file(&self, cluster: u32, size: u32) -> Result<File, Error> {
        Ok(File {
            filesystem: self,
            start_cluster: cluster,
            active_cluster: cluster,
            sector_offset: 0,
            size,
            position: 0,
        })
    }

    pub fn get_directory(&self, cluster: u32) -> Result<Directory, Error> {
        Ok(Directory {
            filesystem: self,
            cluster: Some(cluster),
            sector: 0,
            offset: 0,
            cluster_start: Some(cluster)
        })
    }

    pub fn open(&self, path: &str) -> Result<DirectoryEntry, Error> {

        let mut residual = path;

        let mut current_dir = self.root().unwrap();
        let mut current_directory_entry = DirectoryEntry {
            name: [0;11],
            file_type: FileType::Directory,
            cluster: self.root().unwrap().cluster.unwrap(),
            size: 0,
            long_name: [0; 255],
        };

        loop {
            // sub is the directory or file name
            // residual is what is left
            if residual.len() == 0 {
                return Ok(current_directory_entry);
            }

            let sub = match &residual[1..]
                .find('/')
                .or_else(|| (&residual[1..]).find('\\'))
            {
                None => {
                    let sub = &residual[1..];
                    residual = "";
                    sub
                }
                Some(x) => {
                    // +1 due to above find working on substring
                    let sub = &residual[1..=*x];
                    residual = &residual[(*x + 1)..];
                    sub
                }
            };
            if sub.len() == 0 {
                return Ok(current_directory_entry);
            }

            loop {
                match current_dir.next_entry() {
                    Err(Error::EndOfFile) => return {
                        return Err(Error::NotFound);},
                    Err(e) => {
                        return Err(e);
                    },
                    Ok(de) => {
                        let filename = unsafe{core::str::from_utf8_unchecked(&de.name)};
                        if compare_name(sub, &de) {
                            match de.file_type {
                                FileType::Directory => {
                                    current_dir = self.get_directory(de.cluster).unwrap();
                                    current_directory_entry = de;
                                    break;
                                }
                                FileType::File => {
                                    return Ok(de);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

impl fmt::Display for Filesystem<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Filesystem:
        start: {:?}
        last: {:?}
        bytes_per_sectors: {:?}
        sectors: {:?}
        fay_type: {:?}
        clusters: {:?}
        sectors_per_fat: {:?}
        sectors_per_cluster: {:?}
        fat_count: {:?}
        root_dir_sectors: {:?}
        first_fat_sector: {:?}
        first_data_sector: {:?}
        data_sector_count: {:?}
        data_cluster_count: {:?}
        root_cluster: {:?}
        ",
        self.start, self.last, self.bytes_per_sector, self.sectors,
        self.fat_type, self.clusters, self.sectors_per_fat,
        self.sectors_per_cluster, self.fat_count, self.root_dir_sectors,
        self.first_fat_sector, self.first_data_sector,
        self.data_sector_count, self.data_cluster_count,
        self.root_cluster
    )}
}

impl fmt::Display for Directory<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Directory:[cluster: {:?} sector: {} offset: {}]", self.cluster, self.sector, self.offset)
    }
}

impl fmt::Display for DirectoryEntry {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Directory:[cluster: {:?} name: {:?} ]", self.cluster, self.name)
    }
}

#[cfg(test)]
mod tests {
    use super::Read;
    use crate::part::tests::FakeDisk;

    #[test]
    fn test_fat_32_init() {
        let d = FakeDisk::new("test\\fat32.img");
        println!("d.total_sectors(): {}", d.total_sectors());
        let mut fs = crate::fat::Filesystem::new(&d, 0, d.total_sectors(), 0u32);
        fs.init().expect("failed");
        let mut root_dir = fs.root().expect("no root");
        loop {
            let res = root_dir.next_entry();
            match res {
                Ok(entry) => {
                    println!("entry: {}", entry);
                }
                Err(super::Error::EndOfFile) => {println!("end of file"); break;}
                Err(_) => {println!("error"); break;}
            }
        }

        //fs.open(current_dir, path)
        println!("fs is: {}\n {}", fs, root_dir);
        assert_eq!(1,1)
    }
}