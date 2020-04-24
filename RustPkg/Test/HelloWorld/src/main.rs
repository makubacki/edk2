// Example: Hello World!
//
// This is an example UEFI application that prints "Hello World!", then waits for key input before
// it exits. It serves as base example how to write UEFI applications without any helper modules
// other than the UEFI protocol definitions.
//
// The `main` function serves as entry-point. Depending on your target-configuration, it must be
// exported with a pre-configured name so the linker will correctly mark it as entry-point. The
// target configurations shipped with upstream rust-lang use `efi_main` as symbol name.
//
// Additionally, a panic handler is provided. This is executed by rust on panic. For simplicity,
// we simply end up in an infinite loop. For real applications, this method should probably call
// into `SystemTable->boot_services->exit()` to exit the UEFI application. Note, however, that
// UEFI applications are likely to run in the same address space as the entire firmware. Hence,
// halting the machine might be a viable alternative. All that is out-of-scope for this example,
// though.
//
// Lastly, note that UEFI uses UTF-16 strings. Since rust literals are UTF-8, we have to use an
// open-coded, zero-terminated, UTF-16 array as argument to `output_string()`. Similarly to the
// panic handler, real applications should rather use UTF-16 modules.
//
// Note that as of rust-1.31.0, all features used here are stabilized. No unstable features are
// required, nor do we rely on nightly compilers.

#![no_main]
#![no_std]

use r_efi::efi;

//GlobalAlloc and alloc_error_handler installed by efi_services
use efi_services;
use efi_lib::{self, boot_services};
use efi_str::{self, OsStr, OsString};

use log::Level;

#[panic_handler]
fn panic_handler(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
#[export_name = "efi_main"]
pub extern fn main(_h: efi::Handle, st: *mut efi::SystemTable) -> efi::Status {

    // After r_efi_servcies::init called boot_services, runtime_service and log avaiable
    unsafe { efi_services::init(_h, st); }

    // Print "Hello World!".
    log::info!("hello world\n");

    // Print a ucs2 string with NUL
    let s = efi_str::ucs2_str!("get a ucs2 string end with nul\r\n");
    let s: &OsStr = OsStr::from_u16_slice(&s);

    log::info!("{}", s);

    let s = OsString::from("OsString from &str\n");
    log::log!(Level::Error, "{}", s);

    // use log to output message
    log::log!(Level::Error, "hello world, {}\n", "error");
    log::log!(Level::Info, "hello world, {}\n", "info");
    log::log!(Level::Trace, "hello world, {}\n", "trace");
    log::log!(Level::Debug, "hello world, {}\n", "debug");
    log::log!(Level::Warn, "hello world, {}\n", "wran");

    //runtime-service
    let mut variable_size: usize = 1024;
    let mut variable_name: [u16;1024] = [0u16;1024];
    let mut vender_guid: efi::Guid = efi::Guid::from_fields(
        0x0, 0x0, 0x0, 0x0, 0x0, &[0x00, 0x0, 0x0, 0x0, 0x0, 0x0]
    );
    let runtime_services = efi_services::runtime_services();
    let mut status = runtime_services.get_next_variable_name(&mut variable_size, &mut variable_name, &mut vender_guid as *mut efi::Guid);

    // todo: use iter instead
    loop {
        match status {
            efi::Status::NOT_FOUND => {log::info!("End\n"); break},
            efi::Status::SUCCESS => {
                variable_size = 1024;
                status = runtime_services.get_next_variable_name(&mut variable_size, &mut variable_name, &mut vender_guid as *mut efi::Guid);
                let vname = OsStr::from_u16_slice(&mut variable_name[..]);
                log::log!(Level::Info, "Variable Name is: {}\n", vname);
            },
            efi::Status::BUFFER_TOO_SMALL => {
                log::log!(Level::Warn, "Buffer too small\n");
            }
            _ => {
                log::log!(Level::Error, "status is: 0x{:x}", status.value());
                break
            }
        }
    }


    // Wait for key input, by waiting on the `wait_for_key` event hook.
    let r = unsafe {
        let mut x: usize = 0;
        boot_services().wait_for_event(1, &mut (*(*st).con_in).wait_for_key, &mut x)
    };
    if r.is_error() {
        return r;
    }

    efi::Status::SUCCESS
}
