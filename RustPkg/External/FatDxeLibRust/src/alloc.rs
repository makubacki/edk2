
#![allow(unused)]

use r_efi::efi;

extern "C" {
    fn AllocatePool (Size: usize) -> *mut core::ffi::c_void;
    fn FreePool (Buffer: *mut core::ffi::c_void);
}

pub unsafe fn malloc<T>() -> Result<*mut T, efi::Status> {
    let size = core::mem::size_of::<T>();
    let address = AllocatePool(size);
    if address == core::ptr::null_mut::<core::ffi::c_void>(){
        Err(efi::Status::OUT_OF_RESOURCES)
    } else {
        Ok(core::mem::transmute::<*mut core::ffi::c_void, *mut T>(address))
    }
}

pub unsafe fn free<T>(ptr: *mut T) {
    FreePool(ptr as *mut core::ffi::c_void);
}