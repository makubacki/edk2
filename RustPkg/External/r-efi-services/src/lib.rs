//! This module stores a global reference to the UEFI system table
#![no_std]
#![feature(alloc_error_handler)]

pub mod boot_service;

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
