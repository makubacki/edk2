//! Rust wrapper for UEFI base on r-efi
//!
//! r-efi defination is in r-efi crate, this package implement rust related function for r-efi.
//!
//! ### mod alloc
//!
//! allocate handler for DXE parase
//!
//! ### mod boot_services
//!

#![no_std]
#![feature(alloc_error_handler)]

pub mod boot_services;
mod alloc;

pub use alloc::Allocator;
pub use boot_services::exit_boot_services;
pub use boot_services::boot_services;

pub mod proto;

pub mod logger;

pub mod runtime_services;
pub use runtime_services::RuntimeSercies;

#[cfg(test)]
mod tests {

    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
