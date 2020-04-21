#![allow(unused)]

use core::ffi::c_void;
use r_efi::efi::{self, Status};

extern "C" {
    fn AllocatePool(Size: usize) -> *mut c_void;
    fn FreePool(Buffer: *mut c_void);
}

pub unsafe fn malloc<T>() -> Result<*mut T, Status> {
    let size = core::mem::size_of::<T>();
    let address = AllocatePool(size);
    if address == core::ptr::null_mut::<c_void>() {
        Err(Status::OUT_OF_RESOURCES)
    } else {
        Ok(core::mem::transmute::<*mut c_void, *mut T>(address))
    }
}

pub unsafe fn free<T>(ptr: *mut T) {
    FreePool(ptr as *mut c_void);
}

pub unsafe fn duplicate<T>(d: &T) -> Result<*mut T, Status> {
    let t = malloc::<T>()?;
    unsafe {
        core::ptr::copy_nonoverlapping(
            d as *const T as *const c_void,
            t as *mut c_void,
            core::mem::size_of::<T>(),
        );
    }
    Ok(t)
}
