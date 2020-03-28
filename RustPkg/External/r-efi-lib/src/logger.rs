//! This add support for `log` crate, providing logger implementation for UEFI
//!

use r_efi::efi::protocols::simple_text_output::Protocol;
use crate::proto::console::output::SimpleTextOutputProtocol;
use core::fmt::Write;

pub struct Logger {
    writer: Option<SimpleTextOutputProtocol>

}

impl Logger {
    pub unsafe fn new(protocol: &mut Protocol) -> Self {
        Logger {
            writer: Some(SimpleTextOutputProtocol::new(protocol as *mut Protocol))
        }
    }

    pub fn disable(&mut self) {
        self.writer = None;
    }
}

impl log::Log for Logger {
    fn enabled(&self, _metadata: &log::Metadata) -> bool {
        self.writer.is_some()
    }

    fn flush(&self) {

    }

    fn log(&self, record: &log::Record) {

        if let Some(ref writer) = self.writer {

            let writer = unsafe {
                let p = writer as *const SimpleTextOutputProtocol as *mut SimpleTextOutputProtocol; 
                p.as_mut().unwrap()
            };

            let _res = write!(writer, "[{:<5}] {}", record.level(), record.args());
        }
    }

}

unsafe impl Sync for Logger {}
unsafe impl Send for Logger {}
