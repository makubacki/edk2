extern crate alloc;

use alloc::vec::Vec;

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
