use core::fmt;
use core::slice::Iter;

pub struct OsStr([u16]);

#[cfg(feature = "string")]
use crate::os_string::OsString;

impl OsStr {
    pub fn new<S: AsRef<OsStr> + ?Sized>(s: &S) -> &OsStr {
        s.as_ref()
    }

    #[cfg(feature = "string")]
    pub fn to_os_string(&self) -> OsString {
        let mut s = OsString::new();
        s.push(self);
        s
    }

    pub fn len(&self) -> usize {
        self.0.len()
    }

    pub fn from_slice(s: &[u16]) -> &OsStr {
        unsafe { &*(s as *const [u16] as *const OsStr) }
    }

    pub fn from_slice_mut(s: &mut [u16]) -> &mut OsStr {
        unsafe { &mut *(s as *mut [u16] as *mut OsStr) }
    }

    pub fn iter(&self) -> Iter<'_, u16> {
        self.0.iter()
    }
}

impl fmt::Display for &OsStr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let len = self.0.len();
        const BUFFER_LEN: usize = 42;

        let mut buffer = [0u8; BUFFER_LEN * 3 + 1];

        let mut end_index;
        let mut res: core::result::Result<(), core::fmt::Error> = Ok(());
        for i in 0..((len + BUFFER_LEN) / BUFFER_LEN) {
            if (i + 1) * BUFFER_LEN >= len {
                end_index = len;
            } else {
                end_index = (i + 1) * BUFFER_LEN;
            }
            let ret = crate::encoder::decode(&(self.0[i * BUFFER_LEN..end_index]), &mut buffer);
            if let Ok(length) = ret {
                res = write!(
                    f,
                    "{}",
                    core::str::from_utf8(&buffer[..length]).expect("error encoder")
                );
                res?
            }
        }
        res
    }
}

impl AsRef<OsStr> for OsStr {
    fn as_ref(&self) -> &OsStr {
        self
    }
}
