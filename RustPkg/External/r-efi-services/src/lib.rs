//! This module stores a global reference to the UEFI system table
#![no_std]
#![feature(alloc_error_handler)]

use r_efi::efi;
use r_efi_lib::{Allocator, boot_services};

pub unsafe fn init(_handle: efi::Handle, st: *mut efi::SystemTable)
{
    boot_services::init(&(*(*st).boot_services));
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

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
