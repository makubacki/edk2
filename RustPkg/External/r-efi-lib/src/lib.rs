//! This module implements Rust's global allocator interface using UEFI's memory allocation functions.
//! 
#![no_std]
#![feature(alloc_error_handler)]

pub mod boot_services;
mod alloc;

pub use alloc::Allocator;
pub use boot_services::exit_boot_services;
pub use boot_services::boot_services;

#[cfg(test)]
mod tests {
    
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
