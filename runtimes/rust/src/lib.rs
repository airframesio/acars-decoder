//! `ads-runtime` — Rust runtime library for ADS-generated ACARS decoder plugins.
//!
//! Generated plugin code (output of `ads-gen --target rust`) imports from this
//! crate. The Stage 2 Rust impl (`acars-decoder-rust`) depends on this crate
//! via a path dependency on the `airframes-decoder` submodule.
//!
//! Behavior matches `acars-decoder-typescript` byte-for-byte; the shared
//! corpus (`airframes-decoder/corpus/`) enforces parity.

pub mod plugin;
pub mod result_formatter;
pub mod helpers;
pub mod ascii85;
pub mod coordinate;
pub mod crc;
pub mod types;

pub use plugin::{DecodeResult, Message, Options, Plugin, Qualifiers, RawValue};
pub use result_formatter::ResultFormatter;
