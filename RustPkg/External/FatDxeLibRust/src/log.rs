#[cfg(not(test))]
extern crate debug_lib;

#[macro_export]
macro_rules! log {
    ($($arg:tt)*) => {
        #[cfg(not(test))]
        debug_lib::println!("EFI_STUB: {}", format_args!($($arg)*));
    }
}