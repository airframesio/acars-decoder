# airframes-decoder

Central home for the **Airframes Decoder Spec (ADS)** — a portable, declarative DSL that captures ACARS decoding semantics in a language-neutral form.

Consumed via build-time codegen by:

- [`acars-decoder-typescript`](https://github.com/airframesio/acars-decoder-typescript) — reference implementation, NPM package
- [`acars-decoder-rust`](https://github.com/airframesio/acars-decoder-rust) — Rust crate with C ABI and WASM targets
- [`acars-decoder-c`](https://github.com/airframesio/acars-decoder-c) — pure-C implementation

See [`docs/DSL.md`](docs/DSL.md) for the spec format reference and [`docs/ADOPTION.md`](docs/ADOPTION.md) for how each language repo consumes ADS.

## Status

Bootstrapping. See branch `init/ads-v1` for the in-progress v1 spec, codegen, and corpus.
