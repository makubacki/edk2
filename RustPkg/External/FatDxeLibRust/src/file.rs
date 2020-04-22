#![allow(unused)]
extern crate alloc;

use efi_str::OsStr;

use crate::calloc::{free, malloc};
use crate::fat::Filesystem;
use r_efi::efi::{self, Char16, Guid, Status};
use r_efi::protocols::device_path::Protocol as DevicePathProtocol;
use r_efi::protocols::device_path::{HardDriveDevicePath, HardDriveDevicePathNode};
use r_efi::protocols::file::IoToken;
use r_efi::protocols::file::Protocol as FileProtocol;
use r_efi::protocols::simple_file_system::Protocol as SimpleFileSystemProtocol;

use core::ffi::c_void;

use core::fmt;

#[cfg(not(test))]
#[repr(packed)]
struct FileInfo {
    size: u64,
    file_size: u64,
    physical_size: u64,
    _create_time: r_efi::system::Time,
    _last_access_time: r_efi::system::Time,
    _modification_time: r_efi::system::Time,
    attribute: u64,
    file_name: [Char16; 261],
}

impl core::fmt::Debug for FileInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        unsafe {
            write!(
                f,
                "FInfo: fsize: {}, attr: {:x}, fname: {}",
                self.file_size,
                self.attribute,
                OsStr::from_char16_with_nul(&self.file_name[..] as *const [u16] as *const u16)
            )
        }
    }
}

pub struct FileSystemWrapper<'a> {
    pub fs: Filesystem<'a>,
    pub proto: SimpleFileSystemProtocol,
}

pub struct FileWrapper<'a> {
    pub fs: &'a Filesystem<'a>,
    pub fs_wrapper: *const FileSystemWrapper<'a>,
    pub proto: FileProtocol,
    pub root: bool,
    pub parent: Option<&'a FileWrapper<'a>>,

    pub dir_entry: crate::fat::DirectoryEntry,
    pub file: crate::fat::File<'a>,
    pub dir: crate::fat::Directory<'a>,
}

impl<'a> FileSystemWrapper<'a> {
    pub unsafe fn new(fs: Filesystem<'a>) -> Result<*mut FileSystemWrapper, Status> {
        let fs_wrapper = malloc::<FileSystemWrapper>()?;

        (*fs_wrapper).fs = fs;
        (*fs_wrapper).proto = SimpleFileSystemProtocol {
            revision: r_efi::protocols::simple_file_system::REVISION,
            open_volume: filesystem_open_volumn,
        };
        Ok(fs_wrapper)
    }

    pub unsafe fn create_file(&self, root: bool) -> Result<*mut FileWrapper, Status> {
        let fw = malloc::<FileWrapper>()?;

        (*fw).fs = &(self.fs);

        (*fw).fs_wrapper = self;

        (*fw).proto = FileProtocol {
            revision: r_efi::protocols::file::REVISION,
            open,
            close,
            delete,
            read,
            write,
            get_position,
            set_position,
            get_info,
            set_info,
            flush,
            open_ex,
            read_ex,
            write_ex,
            flush_ex,
        };

        (*fw).root = root;

        (*fw).parent = None;

        if root {
            let root_dir = self.fs.root().unwrap();
            let mut entry = crate::fat::DirectoryEntry {
                name: [0; 11],
                file_type: crate::fat::FileType::Directory,
                cluster: root_dir.cluster.unwrap(),
                size: 0,
                long_name: [0; 261],
            };
            (*fw).dir = root_dir;
            (*fw).dir_entry = entry;
        }

        Ok(fw)
    }

    pub unsafe fn get_hard_drive_device_path(&self) -> Result<*mut c_void, Status> {
        let mut file_system_path = HardDriveDevicePath {
            file_system_path_node: HardDriveDevicePathNode {
                header: DevicePathProtocol {
                    r#type: r_efi::protocols::device_path::TYPE_MEDIA,
                    sub_type: r_efi::protocols::device_path::Hardware::SUBTYPE_PCI,
                    length: [42, 0],
                },
                partition_number: self.fs.part_id,
                partition_start: self.fs.start as u64,
                partition_size: self.fs.last - self.fs.start as u64,
                partition_signature: [0x5452_4150_2049_4645u64, 0],
                partition_format: 0x2 as u8,
                signature_type: 0x2 as u8,
            },
            end: r_efi::protocols::device_path::End {
                header: DevicePathProtocol {
                    r#type: r_efi::protocols::device_path::TYPE_END,
                    sub_type: r_efi::protocols::device_path::End::SUBTYPE_ENTIRE,
                    length: [4, 0],
                },
            },
        };
        let mut fp = crate::calloc::duplicate(&file_system_path)?;
        Ok(fp as *mut c_void)
    }
}

pub extern "win64" fn filesystem_open_volumn(
    proto: *mut SimpleFileSystemProtocol,
    file: *mut *mut FileProtocol,
) -> Status {
    unsafe {
        let wrapper = container_of!(proto, FileSystemWrapper, proto);
        let wrapper: &FileSystemWrapper = &*wrapper;

        let res = wrapper.create_file(true);
        match res {
            Err(err) => {
                return err;
            }
            Ok(fw) => {
                *file = &mut (*fw).proto;
                //log!("open_volumn - status: {:x} - path: \\, file: {:x}", Status::SUCCESS.value(), *file as u64);
                return Status::SUCCESS;
            }
        }
    }
}

pub extern "win64" fn open(
    file_in: *mut FileProtocol,
    file_out: *mut *mut FileProtocol,
    path_in: *mut Char16,
    _: u64,
    _: u64,
) -> Status {
    unsafe {
        let wrapper = container_of!(file_in, FileWrapper, proto);
        let wrapper: &FileWrapper = unsafe { &*wrapper };
        let fs_wrapper = unsafe { &(*wrapper.fs_wrapper) };

        let mut path_os = OsStr::from_char16_with_nul_mut(path_in);
        let path = alloc::string::String::from_utf16_lossy(path_os.as_u16_slice());

        if path == "\\" {
            let file_out_wrapper = fs_wrapper.create_file(true);
            if file_out_wrapper.is_err() {
                log!(
                    "open - status: {:x} - path: {}, file_in: {:x}, file_out: {:x}",
                    file_out_wrapper.err().unwrap().value(),
                    path_os,
                    file_in as u64,
                    *file_out as u64
                );
                return file_out_wrapper.err().unwrap();
            }
            let file_out_wrapper = file_out_wrapper.expect("unwrap error");
            unsafe {
                *file_out = &mut (*file_out_wrapper).proto;
                log!(
                    "open - status: {:x} - path: {}, file_in: {:x}, file_out: {:x}",
                    Status::SUCCESS.value(),
                    path_os,
                    file_in as u64,
                    *file_out as u64
                );
                return Status::SUCCESS;
            }
        }

        if path == "." {
            let file_out_wrapper: *mut FileWrapper = fs_wrapper.create_file(false).unwrap();
            unsafe {
                (*file_out_wrapper).root = wrapper.root;
                (*file_out_wrapper).parent = wrapper.parent;
                (*file_out_wrapper).dir_entry = wrapper.dir_entry;
                if wrapper.dir_entry.file_type == crate::fat::FileType::Directory {
                    (*file_out_wrapper).dir = wrapper
                        .fs
                        .get_directory(wrapper.dir_entry.cluster)
                        .expect("unwrap error");
                } else {
                    (*file_out_wrapper).file = wrapper
                        .fs
                        .get_file(wrapper.dir_entry.cluster, wrapper.dir_entry.size)
                        .expect("unwrap error");
                }
                *file_out = &mut (*file_out_wrapper).proto;
            }
            log!(
                "open - status: {:x} - path: {}, file_in: {:x}, file_out: {:x}",
                Status::SUCCESS.value(),
                path_os,
                file_in as u64,
                *file_out as u64
            );
            return Status::SUCCESS;
        }
        if path == ".." {
            if wrapper.parent.is_none() {
                log!(
                    "open - status: {:x} - path: {}, file_in: {:x}, file_out: {:x}",
                    Status::INVALID_PARAMETER.value(),
                    path_os,
                    file_in as u64,
                    *file_out as u64
                );
                return Status::INVALID_PARAMETER;
            }
            let wrapper = wrapper.parent.expect("unwrap");
            let file_out_wrapper: *mut FileWrapper = fs_wrapper.create_file(false).unwrap();
            unsafe {
                (*file_out_wrapper).root = wrapper.root;
                (*file_out_wrapper).parent = wrapper.parent;
                (*file_out_wrapper).dir_entry = wrapper.dir_entry;
                if wrapper.dir_entry.file_type == crate::fat::FileType::Directory {
                    (*file_out_wrapper).dir = wrapper
                        .fs
                        .get_directory(wrapper.dir_entry.cluster)
                        .expect("unwrap error");
                } else {
                    (*file_out_wrapper).file = wrapper
                        .fs
                        .get_file(wrapper.dir_entry.cluster, wrapper.dir_entry.size)
                        .expect("unwrap error");
                }
                *file_out = &mut (*file_out_wrapper).proto;
            }

            log!(
                "open - status: {:x}, path: {}, file_in: {:x}, file_out: {:x}",
                Status::SUCCESS.value(),
                path_os,
                file_in as u64,
                *file_out as u64
            );
            return Status::SUCCESS;
        }

        match wrapper.fs.open(&path[..]) {
            Ok(f) => {
                let ret = fs_wrapper.create_file(false);
                if ret.is_err() {
                    log!(
                        "open - status: {:x}, path: {}, file_in: {:x}, file_out: {:x}",
                        Status::DEVICE_ERROR.value(),
                        path_os,
                        file_in as u64,
                        *file_out as u64
                    );
                    return Status::DEVICE_ERROR;
                }

                let file_out_wrapper = ret.unwrap();
                unsafe {
                    match f.file_type {
                        crate::fat::FileType::Directory => {
                            let directory = wrapper.fs.get_directory(f.cluster);
                            if directory.is_err() {
                                log!(
                                    "open - status: {:x}, path: {}, file_in: {:x}, file_out: {:x}",
                                    Status::DEVICE_ERROR.value(),
                                    path_os,
                                    file_in as u64,
                                    *file_out as u64
                                );
                                return Status::DEVICE_ERROR;
                            }
                            let mut directory = directory.expect("unwrap error");
                            (*file_out_wrapper).dir = directory;
                            (*file_out_wrapper).parent = Some(wrapper);
                        }
                        crate::fat::FileType::File => {
                            let mut file = wrapper.fs.get_file(f.cluster, f.size);
                            if file.is_err() {
                                return Status::DEVICE_ERROR;
                            }
                            let mut file = file.expect("unwrap error");
                            (*file_out_wrapper).file = file;
                        }
                    }
                    (*file_out_wrapper).dir_entry = f;
                    *file_out = &mut (*file_out_wrapper).proto;
                }
                log!(
                    "open - status: {:x}, path: {}, file_in: {:x}, file_out: {:x}",
                    Status::SUCCESS.value(),
                    path_os,
                    file_in as u64,
                    *file_out as u64
                );
                return Status::SUCCESS;
            }
            Err(crate::fat::Error::NotFound) => {
                log!(
                    "open - status: {:x}, path: {}, file_in: {:x}, file_out: {:x}",
                    Status::NOT_FOUND.value(),
                    path_os,
                    file_in as u64,
                    *file_out as u64
                );
                return Status::NOT_FOUND;
            }
            Err(_) => {
                log!(
                    "open - status: {:x}, path: {}, file_in: {:x}, file_out: {:x}",
                    Status::DEVICE_ERROR.value(),
                    path_os,
                    file_in as u64,
                    *file_out as u64
                );
                return Status::DEVICE_ERROR;
            }
        }
    }
}

pub extern "win64" fn close(proto: *mut FileProtocol) -> Status {
    // log!("file close: {:x}", proto as u64);
    // let wrapper = container_of!(proto, FileWrapper, proto);
    // unsafe{crate::calloc::free(wrapper as *mut FileWrapper);}
    Status::SUCCESS
}

pub extern "win64" fn delete(_: *mut FileProtocol) -> Status {
    // crate::log!("delete unsupported");
    Status::UNSUPPORTED
}

pub fn ascii_to_ucs2(input: &str, output: &mut [u16]) {
    assert!(output.len() >= input.len());

    for (i, c) in input.bytes().enumerate() {
        output[i] = u16::from(c);
    }
}

pub extern "win64" fn read(file: *mut FileProtocol, size: *mut usize, buf: *mut c_void) -> Status {
    unsafe {
        // log!("read called {:?} {:?}", file, size);
        let old_size = *size;
        let wrapper = container_of_mut!(file, FileWrapper, proto);
        let wrapper: &mut FileWrapper = &mut (*wrapper);
        match wrapper.dir_entry.file_type {
            crate::fat::FileType::File => {
                let mut file = &mut wrapper.file;
                let mut current_offset = 0;
                let mut bytes_remaining = *size;
                loop {
                    use crate::fat::Read;
                    let buf = unsafe { core::slice::from_raw_parts_mut(buf as *mut u8, *size) };
                    let mut data: [u8; 512] = [0; 512];
                    match file.read(&mut data) {
                        Ok(bytes_read) => {
                            buf[current_offset..current_offset + bytes_read as usize]
                                .copy_from_slice(&data[0..bytes_read as usize]);
                            current_offset += bytes_read as usize;

                            if bytes_remaining <= bytes_read as usize {
                                *size = current_offset;
                                return Status::SUCCESS;
                            }
                            bytes_remaining -= bytes_read as usize;
                        }
                        Err(_) => {
                            return Status::DEVICE_ERROR;
                        }
                    }
                }
            }
            crate::fat::FileType::Directory => {
                let info = buf as *mut FileInfo;
                let mut directory = &mut wrapper.dir;
                match directory.next_entry() {
                    Err(crate::fat::Error::EndOfFile) => {
                        (*info).size = 0;
                        (*info).attribute = 0;
                        (*info).file_name[0] = 0;
                        (*size) = 0;
                        let status = Status::SUCCESS;
                        log!(
                            "read -  status: {:x}, file: {:x}, info: {:?}",
                            status.value(),
                            file as u64,
                            *info
                        );
                        return status;
                    }
                    Err(e) => {
                        let status = Status::DEVICE_ERROR;
                        log!(
                            "read -  status: {:x}, file: {:x}, info: {:?}",
                            status.value(),
                            file as u64,
                            *info
                        );
                        return status;
                    }
                    Ok(de) => {
                        let mut long_name = de.long_name;
                        if long_name[0] == 0 {
                            for i in 0..11 {
                                long_name[i] = de.name[i] as u16;
                            }
                        }

                        (*info).file_name = long_name;
                        match de.file_type {
                            crate::fat::FileType::File => {
                                (*info).size = core::mem::size_of::<FileInfo>() as u64;
                                (*info).file_size = de.size.into();
                                (*info).physical_size = de.size.into();
                                (*info).attribute = 0x20;
                            }
                            crate::fat::FileType::Directory => {
                                (*info).size = core::mem::size_of::<FileInfo>() as u64;
                                (*info).file_size = 4096;
                                (*info).physical_size = 4096;
                                (*info).attribute = 0x10;
                            }
                        }
                        let status = Status::SUCCESS;
                        log!(
                            "read -  status: {:x}, file: {:x}, info: {:?}",
                            status.value(),
                            file as u64,
                            *info
                        );
                        return status;
                    }
                }
            }
        }
    }
}

pub extern "win64" fn write(_: *mut FileProtocol, _: *mut usize, _: *mut c_void) -> Status {
    crate::log!("write unsupported");
    Status::UNSUPPORTED
}

pub extern "win64" fn get_position(_: *mut FileProtocol, _: *mut u64) -> Status {
    crate::log!("get_position unsupported");
    Status::UNSUPPORTED
}

pub extern "win64" fn set_position(_: *mut FileProtocol, _: u64) -> Status {
    // TODO: set position for opened file and opend directory.
    // crate::log!("set_position todo\n");
    Status::SUCCESS
}

pub extern "win64" fn get_info(
    file: *mut FileProtocol,
    guid: *mut Guid,
    info_size: *mut usize,
    info: *mut c_void,
) -> Status {
    unsafe {
        let wrapper = container_of!(file, FileWrapper, proto);
        let wrapper = &(*wrapper);
        if *guid == r_efi::protocols::file::INFO_ID {
            if *info_size < core::mem::size_of::<FileInfo>() {
                *info_size = core::mem::size_of::<FileInfo>();
                Status::BUFFER_TOO_SMALL
            } else {
                let info = info as *mut FileInfo;
                use crate::fat::Read;
                let mut long_name = wrapper.dir_entry.long_name;
                if long_name[0] == 0 {
                    for i in 0..11 {
                        long_name[i] = (*wrapper).dir_entry.name[i] as u16;
                    }
                }

                (*info).file_name = long_name;
                match wrapper.dir_entry.file_type {
                    crate::fat::FileType::File => {
                        (*info).size = core::mem::size_of::<FileInfo>() as u64;
                        let file = wrapper
                            .fs
                            .get_file(wrapper.dir_entry.cluster, wrapper.dir_entry.size)
                            .expect("error");
                        (*info).file_size = file.get_size().into();
                        (*info).physical_size = file.get_size().into();
                        (*info).attribute = 0x20;
                    }
                    crate::fat::FileType::Directory => {
                        (*info).size = core::mem::size_of::<FileInfo>() as u64;
                        (*info).file_size = 4096;
                        (*info).physical_size = 4096;
                        (*info).attribute = 0x10;
                    }
                }
                log!("get_info - file_in: {:x} info: {:?}", file as u64, &*info);
                Status::SUCCESS
            }
        } else {
            crate::log!("get_info unsupported");
            Status::UNSUPPORTED
        }
    }
}

pub extern "win64" fn set_info(
    _: *mut FileProtocol,
    _: *mut Guid,
    _: usize,
    _: *mut c_void,
) -> Status {
    crate::log!("set_info unsupported");
    Status::UNSUPPORTED
}

pub extern "win64" fn flush(_: *mut FileProtocol) -> Status {
    crate::log!("flush unsupported");
    Status::UNSUPPORTED
}

pub extern "win64" fn open_ex(
    _: *mut FileProtocol,
    _: *mut *mut FileProtocol,
    _: *mut Char16,
    _: u64,
    _: u64,
    _: *mut IoToken,
) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn read_ex(_: *mut FileProtocol, _: *mut IoToken) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn write_ex(_: *mut FileProtocol, _: *mut IoToken) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn flush_ex(_: *mut FileProtocol, _: *mut IoToken) -> Status {
    Status::UNSUPPORTED
}
