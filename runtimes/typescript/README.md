# @airframes/ads-runtime-ts

TypeScript runtime library for ADS-generated ACARS decoder plugins.

## Layout

```
runtimes/typescript/
├── index.ts                    # Public exports: DecoderPlugin, types, ResultFormatter, utils
├── helpers.ts                  # Decode-fn helpers the codegen emits calls into
├── DecoderPlugin.ts            # Base class generated plugins extend
├── DecoderPluginInterface.ts   # Message, DecodeResult, Qualifiers, RawFields, …
├── MessageDecoder.ts           # Interface the dispatcher implements (concrete impl lives in language repo)
├── DateTimeUtils.ts            # Date/time helpers (HHMMSS → tod, etc.)
├── utils/
│   ├── result_formatter.ts     # ResultFormatter with 30+ methods
│   ├── coordinate_utils.ts     # decodeStringCoordinates, decimal-minutes
│   ├── ascii85.ts              # Pure-JS ASCII85 decoder
│   ├── compression.ts          # pako wrapper + base64
│   ├── miam.ts                 # MIAM PDU parsing
│   ├── arinc_702_helper.ts     # H1 field dispatch + CRC validation
│   ├── icao_fpl_utils.ts       # ICAO flight plan parsing
│   ├── flight_plan_utils.ts
│   └── route_utils.ts
├── types/
│   ├── route.ts
│   ├── waypoint.ts
│   └── wind.ts
└── escape_hatches/             # Per-plugin custom-decoder/formatter hatches
    └── index.ts                # Re-exports each hatch module (added as needed)
```

## Source of truth

Behavior comes from **`acars-decoder-typescript`** (in production). These files are the canonical copies; Stage 2 of the unification removes the duplicates from `acars-decoder-typescript/lib/` and points the language repo at this submodule path via a `tsconfig.json` path alias.

## Used by

Generated plugin source (output of `ads-gen --target ts`):

```ts
import { DecoderPlugin } from "@airframes/ads-runtime-ts";
import type { DecodeResult, Message, Options } from "@airframes/ads-runtime-ts";
import { ResultFormatter } from "@airframes/ads-runtime-ts";
import * as helpers from "@airframes/ads-runtime-ts/helpers";
import * as hatches from "../escape_hatches";

export class Label_10_POS extends DecoderPlugin { /* ... */ }
```

## Type-checking

```bash
npm install
npm run lint
```
