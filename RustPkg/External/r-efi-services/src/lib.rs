//! This module stores a global reference to the UEFI system table
#![no_std]
#![feature(alloc_error_handler)]

extern crate log;
use r_efi::efi;
use r_efi_lib::{Allocator, boot_services, logger::Logger, RuntimeSercies};

static mut LOGGER: Option<Logger> = None;

static mut RUNTIME_SERVICES: RuntimeSercies = RuntimeSercies {
    inner: None
};

pub unsafe fn init(_handle: efi::Handle, st: *mut efi::SystemTable)
{
    // init boot_services
    boot_services::init(&(*(*st).boot_services));

    // init logger
    LOGGER = Some(Logger::new(&mut (*(*st).con_out)));
    let logger = LOGGER.as_ref().unwrap();
    log::set_logger(logger).unwrap();
    log::set_max_level(log::LevelFilter::Trace);

    // init runtime_services
    RUNTIME_SERVICES.init((*st).runtime_services);
}

pub fn runtime_services() -> &'static RuntimeSercies {
    unsafe{&RUNTIME_SERVICES}
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
