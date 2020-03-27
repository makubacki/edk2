extern crate alloc;

use alloc::vec::Vec;
use core::fmt;


pub struct OsString(Vec<u16>);

pub struct OsStr([u16]);

impl OsStr {
    pub fn new<S: AsRef<OsStr> + ?Sized>(s: &S) -> &OsStr {
        s.as_ref()
    }

    pub fn to_os_string(&self) -> OsString {
        let mut s = Vec::new();
        for index in 0..self.0.len(){
            s.push(self.0[index]);
        }
        OsString(s)
    }

    pub fn len(&self) -> usize{
        self.0.len()
    }

    pub fn from_slice(s: &[u16]) -> &OsStr {
        unsafe{ &*(s as *const [u16] as *const OsStr)}
    }

    pub fn from_slice_mut(s: &mut [u16]) -> &mut OsStr {
        unsafe { &mut *(s as *mut [u16] as *mut OsStr)}
    }
}

impl core::ops::Deref for OsString {
    type Target = OsStr;

    fn deref(&self) -> &OsStr {
        &self[..]
    }
}

impl core::ops::Index<core::ops::RangeFull> for OsString {
    type Output = OsStr;
    fn index(&self, _index: core::ops::RangeFull) -> &OsStr {
        OsStr::from_slice(&(self.0[..]))
    }
}

impl AsRef<OsStr> for OsString {
    fn as_ref(&self) -> &OsStr {
        self
    }
}

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
    // TODO: directly output u16
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let len = self.0.len();
        let mut vec : Vec<u8> = Vec::new();
        vec.resize(len * 3 + 1, 0u8);
        let _res = ucs2::decode(&(self.0), vec.as_mut_slice());
        write!(f, "{}", core::str::from_utf8(&vec[..]).unwrap())
    }
}

impl fmt::Display for &OsStr {
    // TODO: directly output u16
    fn fmt(&self, f:&mut fmt::Formatter<'_>) -> fmt::Result {
        let len = self.0.len();
        let mut vec : Vec<u8> = Vec::new();
        vec.resize(len * 3 + 1, 0u8);
        let _res = ucs2::decode(&(self.0), vec.as_mut_slice());
        write!(f, "{}", core::str::from_utf8(&vec[..]).unwrap())
    }
}