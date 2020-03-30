#![no_std]

pub use ucs2;

#[macro_use]
mod macros;

mod os_str;

pub use os_str::OsString;
pub use os_str::OsStr;

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
