//! This module implements Rust's global allocator interface using UEFI's memory allocation functions.
//! 
extern crate r_efi;
use r_efi::efi;

use core::alloc::{GlobalAlloc, Layout};
use core::ptr::{self, NonNull};

use efi::BootServices as EfiBootServices;
use efi::MemoryType;
use efi::Status;

pub struct BootServices {
    pub inner: Option<NonNull<EfiBootServices>>
}

static mut BOOT_SERVICES: BootServices = BootServices {
    inner:None
};

pub unsafe fn init(boot_services: &EfiBootServices) {

    BOOT_SERVICES.inner = NonNull::new(boot_services as *const _ as *mut _);
}

impl BootServices {
    pub fn allocate_pool(&self, mem_ty: MemoryType, size: usize) -> Result<*mut u8, Status> {
        let mut buffer = core::ptr::null_mut();
        let status = unsafe{(self.inner.unwrap().as_ref().allocate_pool)(mem_ty, size, &mut buffer)};
        match status {
            Status::SUCCESS => {
                Ok(buffer as *mut u8)
            }
            _ => {
                Err(status)
            }
        }
    }
    pub fn free_pool(&self, addr: *mut u8) -> Result<bool, Status> {
        let status = unsafe{(self.inner.unwrap().as_ref().free_pool)(addr as  *mut core::ffi::c_void)};
        match status {
            Status::SUCCESS => {
                Ok(true)
            }
            _ => {
                Err(status)
            }
        }
    }

}

// access the boot services
fn boot_services() -> &'static BootServices {
    unsafe { BOOT_SERVICES.inner.expect("need initialize boot service"); &BOOT_SERVICES}
}

// exit boot service
pub fn exit_boot_services() {
    unsafe {
        BOOT_SERVICES.inner = None;
    }
}

pub struct Allocator;

#[allow(clippy::cast_ptr_alignment)]
unsafe impl GlobalAlloc for Allocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let mem_ty = MemoryType::LoaderData;
        let size = layout.size();
        let align = layout.align();

        if align > 8 {
            // allocate more space for alignment
            let ptr = if let Ok(ptr) = boot_services()
                .allocate_pool(mem_ty, size + align)
            {
                ptr
            } else {
                return ptr::null_mut();
            };
            // calculate align offset
            let mut offset = ptr.align_offset(align);
            if offset == 0 {
                offset = align;
            }
            let return_ptr = ptr.add(offset);
            // store allocated pointer before the struct
            (return_ptr as *mut *mut u8).sub(1).write(ptr);
            return_ptr
        } else {
            boot_services()
                .allocate_pool(mem_ty, size)
                .unwrap_or(ptr::null_mut())
        }
    }

    unsafe fn dealloc(&self, mut ptr: *mut u8, layout: Layout) {
        if layout.align() > 8 {
            ptr = (ptr as *const *mut u8).sub(1).read();
        }
        boot_services()
            .free_pool(ptr)
            .unwrap();
    }
}

#[global_allocator]
static ALLOCATOR: Allocator = Allocator;

#[alloc_error_handler]
fn out_of_memory(layout: ::core::alloc::Layout) -> ! {
    panic!(
        "Ran out of free memory while trying to allocate {:#?}",
        layout
    );
}