//! This module implements Rust's global allocator interface using UEFI's memory allocation functions.
//! 

use r_efi::efi::MemoryType;
use core::alloc::{GlobalAlloc, Layout};
use core::ptr;

use crate::boot_services::boot_services;

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
