#![no_std]
#![allow(
    clippy::all,
    missing_docs,
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    improper_ctypes,
    unreachable_pub,
    unsafe_op_in_unsafe_fn
)]

mod raw_lightos_bindings {
    include!(concat!(
        env!("OBJTREE"),
        "/rust/bindings/raw_lightos_bindings.rs"
    ));
}

pub use raw_lightos_bindings::*;
