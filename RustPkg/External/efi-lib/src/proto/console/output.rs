
use r_efi::efi;
use r_efi::efi::protocols::simple_text_output::Protocol;
use core::ptr::NonNull;

use core::fmt;
use efi_str;

pub struct SimpleTextOutputProtocol(NonNull<Protocol>, efi::Guid);

impl SimpleTextOutputProtocol {
    pub fn new(protocol_ptr :*mut Protocol) -> Self {
        Self(NonNull::new(protocol_ptr).unwrap(), r_efi::efi::protocols::simple_text_output::PROTOCOL_GUID)
    }

    pub fn reset(&self, extended: efi::Boolean) -> efi::Status {
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.reset)(p_ptr, extended)
    }

    pub fn output_string(&self, s: *mut efi::Char16) -> efi::Status {
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.output_string)(p_ptr, s)
    }

    pub fn test_string(&self, s: *mut efi::Char16) -> efi::Status {
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.test_string)(p_ptr, s)
    }

    pub fn query_mode(&self, mode_number:usize, columns: *mut usize, rows: *mut usize) -> efi::Status {
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.query_mode)(p_ptr, mode_number, columns, rows)
    }

    pub fn set_mode(&self, mode_number: usize) -> efi::Status {
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.set_mode)(p_ptr, mode_number)
    }

    pub fn set_attribute(&self, attribute: usize) -> efi::Status {
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.set_attribute)(p_ptr, attribute)
    }

    pub fn clear_screen(&self) -> efi::Status {
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.clear_screen)(p_ptr)
    }

    pub fn set_cursor_position(&self, column: usize, row: usize) -> efi::Status{
        let p_ptr = self.0.as_ptr();
        let p = unsafe{&*p_ptr};
        (p.set_cursor_position)(p_ptr, column, row)
    }
}

impl fmt::Write for SimpleTextOutputProtocol {
    fn write_str(&mut self, s: &str) -> fmt::Result {

        // Allocate a small buffer on the stack.
        const BUF_SIZE: usize = 128;
        // Add 1 extra character for the null terminator.
        let mut buf = [0u16; BUF_SIZE + 1];

        let mut i = 0;

        // This closure writes the local buffer to the output and resets the buffer.
        let flush_buffer = |buf: &mut [u16], i: &mut usize| {
            buf[*i] = 0;
            let codes = &buf[..=*i];
            *i = 0;

            let status = self.output_string(codes.as_ptr() as *mut efi::Char16);
            if status == efi::Status::SUCCESS {
                Ok(())
            } else {
                Err(fmt::Error)
            }
        };

        let mut add_char = |ch| {
            buf[i] = ch;
            i += 1;

            if i == BUF_SIZE {
                flush_buffer(&mut buf, &mut i).map_err(|_| efi_str::encoder::Error::BufferOverFlow)?;
            }
            Ok(())
        };

        let add_ch = |ret| {
            match ret {
                Err(err) => Err(err),
                Ok(ch) => {
                    if ch == '\n' as u16 {
                        add_char('\r' as u16)?;
                    }
                    add_char(ch)?;
                    Ok(ch as usize)
                }
            }
        };

        efi_str::encoder::encode_fnc(s, add_ch).map_err(|_| fmt::Error)?;

        flush_buffer(&mut buf, &mut i)
    }
}