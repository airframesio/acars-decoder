# ADS adoption guide

How each language repo consumes the central `airframes-decoder` repo.

## Common shape

Every language repo:

1. Adds `airframes-decoder` as a git submodule at `vendor/airframes-decoder/`.
2. Imports the language's runtime helpers from `vendor/airframes-decoder/runtimes/<lang>/`.
3. Runs `ads-gen --target <lang>` at build time to regenerate plugin source from spec YAML.
4. Commits generated files (CI verifies they are up to date).
5. Runs the shared corpus (`vendor/airframes-decoder/corpus/`) as part of its test suite.

## `acars-decoder-typescript`

```bash
git submodule add https://github.com/airframesio/airframes-decoder vendor/airframes-decoder
```

`package.json` — add to scripts:

```json
{
  "scripts": {
    "ads:generate": "node vendor/airframes-decoder/codegen/dist/cli.js generate --target ts --spec vendor/airframes-decoder/spec --out lib/plugins/generated",
    "ads:check": "node vendor/airframes-decoder/codegen/dist/cli.js generate --target ts --spec vendor/airframes-decoder/spec --out lib/plugins/generated --check",
    "test:corpus": "vitest run tests/corpus.test.ts"
  }
}
```

`tsconfig.json` — add path aliases so generated code can `import` runtime helpers via short names:

```json
{
  "compilerOptions": {
    "paths": {
      "@airframes/ads-runtime-ts": ["vendor/airframes-decoder/runtimes/typescript/index.ts"],
      "@airframes/ads-runtime-ts/helpers": ["vendor/airframes-decoder/runtimes/typescript/helpers.ts"]
    }
  }
}
```

Replace the existing `lib/utils/*.ts` imports throughout `lib/plugins/` with imports from the runtime path. Stage 2 PR.

## `acars-decoder-rust`

```bash
git submodule add https://github.com/airframesio/airframes-decoder vendor/airframes-decoder
```

`Cargo.toml`:

```toml
[dependencies]
ads-runtime = { path = "vendor/airframes-decoder/runtimes/rust" }
```

`build.rs`:

```rust
use std::process::Command;
fn main() {
    println!("cargo:rerun-if-changed=vendor/airframes-decoder/spec");
    let status = Command::new("node")
        .args(["vendor/airframes-decoder/codegen/dist/cli.js",
               "generate", "--target", "rust",
               "--spec", "vendor/airframes-decoder/spec",
               "--out", "src/plugins/generated"])
        .status()
        .expect("ads-gen failed");
    assert!(status.success(), "ads-gen exited non-zero");
}
```

Integration tests under `tests/corpus.rs` walk `vendor/airframes-decoder/corpus/` and assert deep-equal output.

## `acars-decoder-c`

CMake `CMakeLists.txt`:

```cmake
add_subdirectory(vendor/airframes-decoder/runtimes/c)
target_link_libraries(acars_decoder PRIVATE ads_runtime_c cjson m)

add_custom_command(
    OUTPUT ${GENERATED_C_FILES}
    COMMAND node ${CMAKE_SOURCE_DIR}/vendor/airframes-decoder/codegen/dist/cli.js
            generate --target c
            --spec ${CMAKE_SOURCE_DIR}/vendor/airframes-decoder/spec
            --out ${CMAKE_SOURCE_DIR}/src/plugins/generated
    DEPENDS ${SPEC_YAML_FILES}
)
```

Corpus test binary under `tests/corpus_test.c`.

## Submodule contributor workflow

```bash
git clone --recurse-submodules https://github.com/airframesio/<repo>
# or, after an initial clone:
git submodule update --init --recursive
```

To bump the pinned ADS version:

```bash
cd vendor/airframes-decoder && git fetch && git checkout <commit-or-tag> && cd ../..
git add vendor/airframes-decoder
git commit -m "ADS: bump to <ref>"
```

The next `ads-gen` run picks up any spec changes.

## CI parity

All three language repos call the same reusable workflows from `airframes-decoder/.github/workflows/`:

```yaml
# .github/workflows/ci.yml
jobs:
  codegen-up-to-date:
    uses: airframesio/airframes-decoder/.github/workflows/codegen-check.yml@main
  corpus:
    uses: airframesio/airframes-decoder/.github/workflows/corpus-test.yml@main
    with:
      language: rust
```

See `airframes-decoder/.github/workflows/` for the workflow contracts.
