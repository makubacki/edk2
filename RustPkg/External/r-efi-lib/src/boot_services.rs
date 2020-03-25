extern crate r_efi;
use r_efi::efi;
use core::ptr::NonNull;

use efi::BootServices as EfiBootServices;
use efi::MemoryType;
use efi::Status;

pub struct BootServices {
    pub inner: Option<NonNull<EfiBootServices>>
}

// init boot services
#[warn(dead_code)]
pub unsafe fn init(boot_services: &EfiBootServices) {

    BOOT_SERVICES.inner = NonNull::new(boot_services as *const _ as *mut _);
}

// access the boot services
pub fn boot_services() -> &'static BootServices {
    unsafe { BOOT_SERVICES.inner.expect("need initialize boot service"); &BOOT_SERVICES}
}

// exit boot service
pub fn exit_boot_services() {
    unsafe {
        BOOT_SERVICES.inner = None;
    }
}

static mut BOOT_SERVICES: BootServices = BootServices {
    inner:None
};

impl BootServices {
    pub fn allocate_pool(&self, mem_ty: MemoryType, size: usize) -> Result<*mut u8, Status> {
        let mut buffer = core::ptr::null_mut();
        let status = unsafe{(self.inner.unwrap().as_ref().allocate_pool)(mem_ty, size, &mut buffer)};
        match status {
            Status::SUCCESS => {
                Ok(buffer as *mut u8)
            }
            _ => {
                Err(status)
            }
        }
    }
    pub fn free_pool(&self, addr: *mut u8) -> Result<bool, Status> {
        let status = unsafe{(self.inner.unwrap().as_ref().free_pool)(addr as  *mut core::ffi::c_void)};
        match status {
            Status::SUCCESS => {
                Ok(true)
            }
            _ => {
                Err(status)
            }
        }
    }

    #[allow(dead_code)]
    pub fn wait_for_event(&self, num: usize,
        event: *mut efi::Event,
        index: *mut usize,
    ) -> Status {
        unsafe{(self.inner.unwrap().as_ref().wait_for_event)(num, event, index)}
    }

}
