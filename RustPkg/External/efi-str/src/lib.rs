#![no_std]

pub mod encoder;

#[macro_use]
mod macros;

mod os_str;
pub use os_str::OsStr;

#[cfg(feature="string")]
mod os_string;
#[cfg(feature="string")]
pub use os_string::OsString;

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
