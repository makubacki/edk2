use r_efi::efi;
use core::ptr::NonNull;
use efi::RuntimeServices as EfiRuntimeService;

pub struct RuntimeSercies{
    pub inner: Option<NonNull<EfiRuntimeService>>
}

impl RuntimeSercies {
    pub fn new() -> Self {
        Self{
            inner: None
        }
    }

    pub fn init(&mut self, rts: *mut EfiRuntimeService) {
        self.inner = NonNull::new(rts);
    }

    pub fn get_next_variable_name(&self, variable_name_size: &mut usize, variable_name: &mut [u16], vendor_guid: *mut efi::Guid) -> efi::Status {
        unsafe{((self.inner.unwrap().as_ref()).get_next_variable_name)(variable_name_size as *mut usize, variable_name.as_mut_ptr() as *mut efi::Char16, vendor_guid)}
    }
}