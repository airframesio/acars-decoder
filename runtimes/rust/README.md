# ads-runtime (Rust)

Rust runtime library for ADS-generated ACARS decoder plugins.

Consumed by `acars-decoder-rust` via a path dependency on the
`airframes-decoder` submodule:

```toml
[dependencies]
ads-runtime = { path = "vendor/airframes-decoder/runtimes/rust" }
```

## Layout

```
runtimes/rust/
├── Cargo.toml
├── README.md
└── src/
    ├── lib.rs                 # public exports: Plugin, Message, Options, DecodeResult, helpers, ...
    ├── plugin.rs              # Plugin trait + result types
    ├── result_formatter.rs    # ResultFormatter (position, altitude, timestamp, ...)
    ├── helpers.rs             # Decode-fn helpers the codegen calls into
    ├── coordinate.rs          # Combined + decimal-minutes coordinate parsers
    ├── ascii85.rs             # Pure-Rust ASCII85 decoder
    ├── crc.rs                 # CRC-16 IBM-SDLC and GENIBUS implementations
    ├── types.rs               # Route, Waypoint, Wind
    └── escape_hatches/mod.rs  # Per-plugin custom decoder/formatter hatches
```

## Generated plugin import shape

```rust
use ads_runtime::{Plugin, Message, Options, DecodeResult, Qualifiers, ResultFormatter, helpers};
use crate::escape_hatches;

pub struct Label_10_POS;

impl Plugin for Label_10_POS {
    fn name(&self) -> &'static str { "label-10-pos" }
    fn qualifiers(&self) -> Qualifiers { /* ... */ }
    fn decode(&self, message: &Message, options: &Options) -> DecodeResult { /* ... */ }
}
```

## Source of truth

`acars-decoder-typescript` (in production). All decoders must produce
byte-identical output via the shared corpus at `airframes-decoder/corpus/`.

## Build

```bash
cargo build
cargo test
```
