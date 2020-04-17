#![allow(unused)]

use r_efi::efi::{self, Status, Char16, Guid};
use r_efi::protocols::simple_file_system::Protocol as SimpleFileSystemProtocol;
use r_efi::protocols::file::Protocol as FileProtocol;
use r_efi::protocols::file::IoToken;
use r_efi::protocols::device_path::Protocol as DevicePathProtocol;
use r_efi::protocols::device_path::{HardDriveDevicePath, HardDriveDevicePathNode};
use crate::fat::Filesystem;
use crate::alloc::{malloc, free};

use core::ffi::c_void;

pub struct FileSystemWrapper<'a> {
    pub fs: Filesystem<'a>,
    pub proto: SimpleFileSystemProtocol
}

pub struct FileWrapper<'a> {
    pub fs: &'a Filesystem<'a>,
    pub proto: FileProtocol,
    pub root: bool,
    pub parent: Option<&'a FileWrapper<'a>>
}

impl<'a> FileSystemWrapper<'a> {
    pub unsafe fn new(fs: Filesystem<'a>) -> Result<*mut FileSystemWrapper, Status> {
        let fs_wrapper = malloc::<FileSystemWrapper>()?;

        (*fs_wrapper).fs = fs;
        (*fs_wrapper).proto = SimpleFileSystemProtocol {
            revision: r_efi::protocols::simple_file_system::REVISION,
            open_volume: filesystem_open_volumn
        };
        Ok(fs_wrapper)
    }

    pub unsafe fn create_file(&self, root: bool) -> Result<*mut FileWrapper, Status> {
        let fw = malloc::<FileWrapper>()?;

        (*fw).fs = &(self.fs);

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
        // (*fw).proto.revision = r_efi::protocols::file::REVISION;
        // (*fw).proto.open = open;
        // (*fw).proto.close = close;
        // (*fw).proto.delete = delete;
        // (*fw).proto.read = read;
        // (*fw).proto.write = write;
        // (*fw).proto.get_position = get_position;
        // (*fw).proto.set_position = set_position;
        // (*fw).proto.get_info = get_info;
        // (*fw).proto.set_info = set_info;
        // (*fw).proto.flush = flush;

        (*fw).root = root;

        (*fw).parent = None;

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
                partition_signature: [0x5452_4150_2049_4645u64,0],
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
        let mut fp = crate::alloc::duplicate(&file_system_path)?;
        Ok(fp as *mut c_void)
    }
}

pub extern "win64" fn filesystem_open_volumn(
    proto: *mut SimpleFileSystemProtocol,
    file: *mut *mut FileProtocol,
) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn open(
    file_in: *mut FileProtocol,
    file_out: *mut *mut FileProtocol,
    path_in: *mut Char16,
    _: u64,
    _: u64,
) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn close(proto: *mut FileProtocol) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn delete(_: *mut FileProtocol) -> Status {
    crate::log!("delete unsupported");
    Status::UNSUPPORTED
}

pub extern "win64" fn read(file: *mut FileProtocol, size: *mut usize, buf: *mut c_void) -> Status {
    Status::UNSUPPORTED
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
    Status::UNSUPPORTED
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
    _: *mut IoToken
) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn read_ex(
    _: *mut FileProtocol,
    _: *mut IoToken
) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn write_ex(
    _: *mut FileProtocol,
    _: *mut IoToken
) -> Status {
    Status::UNSUPPORTED
}

pub extern "win64" fn flush_ex(
    _: *mut FileProtocol,
    _: *mut IoToken
) -> Status {
    Status::UNSUPPORTED
}