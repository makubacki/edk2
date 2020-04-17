#![cfg_attr(not(test), no_std)]

#[macro_use]
mod log;

#[macro_use]
mod common;

use r_efi::efi;

#[cfg(not(test))]
mod calloc;

mod block;
mod part;
mod fat;

#[cfg(not(test))]
mod file;

unsafe fn dump_disk_512(bs: &mut efi::BootServices) {
    let _address = bs.locate_protocol as *const ();
    log!("boot service: {:p}", _address);
    // let mut interface = core::ptr::null_mut();
    // let status = (bs.locate_protocol)(
    //     &efi::protocols::block_io::PROTOCOL_GUID as *const efi::Guid as *mut efi::Guid,
    //     core::ptr::null_mut(),
    //     &mut interface as *mut *mut core::ffi::c_void
    // );
    // if status.is_error() {
    //     log!("locate protocol failed");
    // }
    // let res = block::BlockIoDevice::new(interface as *mut efi::protocols::block_io::Protocol).ok();
    // if let Some(device) = res {
    //     let _part = part::find_efi_partition(&(*device));
    //     log!("parted info: {:?}", _part);
    // }

    let mut handles: *mut efi::Handle = core::ptr::null_mut();
    let mut num_handles = 0usize;

    let status = (bs.locate_handle_buffer)(
        efi::LocateSearchType::ByProtocol,
        &efi::protocols::block_io::PROTOCOL_GUID as *const efi::Guid as *mut efi::Guid,
        core::ptr::null_mut(),
        &mut num_handles as *mut usize,
        &mut handles as *mut *mut efi::Handle as *mut *mut *mut core::ffi::c_void
    );
    if status.is_error() {
        log!("locate protocol failed");
        return;
    }
    log!("locate protocol number: {}", num_handles);
    let list = core::slice::from_raw_parts(handles as *const efi::Handle, num_handles);
    let mut interface = core::ptr::null_mut();
    for index in 0..num_handles {
        let handle = list[index];
        let status = (bs.handle_protocol)(
            handle,
            &efi::protocols::block_io::PROTOCOL_GUID as *const efi::Guid as *mut efi::Guid,
            &mut interface as *mut *mut core::ffi::c_void
        );
        if status.is_error() {
            log!("handle_protocol error: {:x}", status.value());
            break;
        }
        let device = &block::BlockIoDevice::new(interface as *mut efi::protocols::block_io::Protocol);

        #[cfg(not(test))]
        let device = &*crate::calloc::duplicate(device).unwrap();

        let ret = part::find_efi_partition(device);
        log!("parted info: {} {:?}, \nmedia_id: {}, partition: {}", index,  ret, device.media_id, device.logical_partition);
        if ret.is_err() {
            continue;
        }
        let (start, end, part_id) = ret.expect("error");

        let mut fs = fat::Filesystem::new(device, start, end, part_id);
        if fs.init().is_err() {
            log!("filesystem init error");
            continue;
        }
        #[cfg(not(test))] {
            let res = crate::file::FileSystemWrapper::new(fs);
            if res.is_err() {
                log!("filesystem wrapper error");
                continue;
            }
            let fs_wrapper = res.expect("error");
            let mut handle : efi::Handle = core::ptr::null_mut();
            let status = (bs.install_protocol_interface)(
                &mut handle as *mut efi::Handle,
                &mut r_efi::protocols::simple_file_system::PROTOCOL_GUID as *mut efi::Guid,
                r_efi::efi::InterfaceType::NativeInterface,
                &mut (*fs_wrapper).proto as *mut efi::protocols::simple_file_system::Protocol as * mut core::ffi::c_void
            );
            if status.is_error() {
                log!("install simple file system protocol failed");
                continue ;
            }

            let res = (*fs_wrapper).get_hard_drive_device_path();
            if res.is_err() {
                log!("get_hard_drive_device_path failed");
                continue ;
            }
            let status = (bs.install_protocol_interface)(
                &mut handle as *mut efi::Handle,
                &mut r_efi::protocols::device_path::PROTOCOL_GUID as *mut efi::Guid,
                r_efi::efi::InterfaceType::NativeInterface,
                res.expect("error")
            );
            if status.is_error() {
                log!("install device path protocol failed");
                continue ;
            }
            log!("install simple file system ok!");

        }
    }

    #[cfg(not(test))]
    crate::calloc::free(handles);

}

#[export_name = "FatEntryPoint"]
pub extern fn fat_entry_point(_h: efi::Handle, st: *mut efi::SystemTable) -> efi::Status {
    log!("fat_entry");

    unsafe{dump_disk_512(&mut *((*st).boot_services))}
    efi::Status::SUCCESS
}

#[export_name = "FatUnload"]
pub extern fn fat_unload(_h: efi::Handle) -> efi::Status {
    log!("fat_unload");
    efi::Status::SUCCESS
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {

    }
}