/*
 * Root crate for the lightos rust driver model
 */

#![no_std]

extern crate self as lightos;

pub mod bindings;

pub fn k_malloc(size: usize) -> *mut u64 {
    let ret: *mut u64;

    unsafe {
        ret = bindings::kmalloc(size);
    };

    return ret;
}

pub fn k_free(addr: u64) -> ! {
    unsafe {
        bindings::kfree(addr);
    };
}
