// Copyright (c) 2020 Intel Corporation
// Copyright (c) Microsoft Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#![cfg_attr(not(test), no_std)]
#![feature(alloc_error_handler)]

#[cfg(not(test))]
extern crate uefi_rust_panic_lib;

#[cfg(feature="alloc_by_c")]
mod alloc_by_c;

#[cfg(feature="alloc_by_bs")]
mod alloc_by_bs;

#[cfg(feature="alloc_by_bs")]
pub use alloc_by_bs::init;
