# Shared test corpus

Golden input/expected pairs used by **every** language implementation. The corpus is the cross-language source of truth — if TS, Rust, and C disagree on a sample, the corpus says who is right.

## Layout

Mirrors `spec/`:

```
corpus/
├── labels/10/POS/sample-001.json
├── labels/10/POS/sample-002-invalid.json
├── labels/44/POS/sample-001.json
├── wildcards/arinc_702/sample-001.json
└── ...
```

## Format

```json
{
  "spec": "labels/10/POS",
  "source": "acars-decoder-typescript/lib/plugins/Label_10_POS.test.ts",
  "description": "human-readable test case description",
  "input": {
    "label": "10",
    "text": "POS082150, N 3885,..."
  },
  "expected": {
    "decoded": true,
    "decoder": { "name": "label-10-pos", "type": "pattern-match", "decodeLevel": "partial" },
    "formatted": { "description": "Position Report", "items": [ ... ] },
    "remaining": { "text": "..." }
  }
}
```

## Extracting from TS tests

`acars-decoder-typescript`'s `lib/plugins/*.test.ts` files are the source of truth. A small extractor script (planned, `codegen/scripts/extract-corpus.ts`) instruments the Jest run to dump `(input, decodeResult)` pairs to JSON. Manually-curated samples are also welcome and should set `source: "manual"`.

## Validation

Each language repo's CI loads every JSON file, runs the decoder against `input`, and deep-equals against `expected`. Divergence fails CI in that repo.

A reference TypeScript interpreter (planned, `codegen/src/interpret.ts`) walks the spec IR directly and runs the corpus — this validates that specs are decodable before any language repo adopts them.
