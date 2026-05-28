# @airframes/ads-codegen

Codegen tool for the **Airframes Decoder Spec (ADS)**. Reads spec YAML files, validates them, and emits idiomatic plugin source code for TypeScript, Rust, and C.

## Usage

```bash
ads-gen --target ts   --spec ../spec --out ../runtimes/typescript/generated
ads-gen --target rust --spec ../spec --out ../runtimes/rust/src/generated
ads-gen --target c    --spec ../spec --out ../runtimes/c/src/generated
```

Run `ads-gen --help` for all flags.

## Architecture

```
spec/*.yaml ─▶ parse-spec ─▶ validate ─▶ IR ─▶ emit-{ts,rust,c} ─▶ source files
```

- `src/parse-spec.ts` — YAML → raw object tree
- `src/validate.ts` — JSON Schema validation + cross-reference checks
- `src/ir.ts` — typed intermediate representation
- `src/emit-typescript.ts` — TypeScript plugin classes
- `src/emit-rust.ts` — Rust trait impls
- `src/emit-c.ts` — C functions + headers
- `src/interpret.ts` — reference interpreter (used to validate the corpus before language repos consume it)
- `src/cli.ts` — `ads-gen` CLI entry point

## Development

```bash
npm install
npm run build
npm test
```
