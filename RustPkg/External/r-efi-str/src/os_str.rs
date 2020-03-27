extern crate alloc;

use alloc::vec::Vec;
use core::fmt;

pub struct OsString(Vec<u16>);

impl OsString {
    pub fn new() -> OsString {
        OsString(Vec::new())
    }

    pub fn as_mut_ptr(&mut self) -> *mut u16 {
        self.0.as_mut_ptr()
    }

    pub fn as_ptr(&self) -> *const u16 {
        self.0.as_ptr()
    }
}

impl Default for OsString {
    fn default() -> OsString {
        OsString::new()
    }
}

impl From<&str> for OsString {

    // Get OsString object from &str
    // if error occur, immediately return.
    fn from(s: &str) -> OsString {
        let mut ret = OsString::new();

        let add_char = |ch| {
            ret.0.push(ch);
            Ok(())
        };

        crate::ucs2::encode_with(s, add_char).unwrap_or(());
        ret.0.push(0u16);
        ret
    }
}

impl fmt::Display for OsString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let len = self.0.len();
        let mut vec : Vec<u8> = Vec::new();
        vec.resize(len * 3 + 1, 0u8);
        let _res = ucs2::decode(&(self.0), vec.as_mut_slice());
        write!(f, "({})", core::str::from_utf8(&vec[..]).unwrap())
    }
}