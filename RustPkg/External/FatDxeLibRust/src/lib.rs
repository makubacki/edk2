#![cfg_attr(not(test), no_std)]

use r_efi::efi;

mod log;

#[export_name = "FatEntryPoint"]
pub extern fn fat_entry_point(_h: efi::Handle, _st: *mut efi::SystemTable) -> efi::Status {
    log!("fat_entry");
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