# Escape hatches

For the ~5% of ACARS plugins whose logic resists clean declarative spec — heuristic offset computation, recursive composition (CBand → MessageDecoder), version-dependent CRC dispatch, complex variant slicing — ADS provides escape hatches at three levels.

## When to use one

Use an escape hatch when adding new DSL primitives would be more complex than the logic itself. The threshold is roughly:

- More than ~5 lines of branching logic that can't be expressed with `variants` + `when`
- Computed offsets that depend on parsed-so-far content (not just message structure)
- Recursive invocations of the message decoder
- Format-specific algorithms (CRC variants, base85 dialects) that warrant a single canonical implementation

If you find yourself wanting more than one escape hatch per spec, consider whether the DSL needs another primitive.

## Levels

### Whole-plugin

```yaml
parse:
  custom: arinc_702_dispatch
formatted:
  description: "ARINC 702 Message"
  custom: arinc_702_format
```

The entire decode flow runs in a hand-written function. Generated code is a one-liner that delegates.

### Field-level

```yaml
- name: variant_result
  from: $fields
  decode:
    fn: custom
    custom: label_4a_variant_2_decode
```

The value of `from` plus the field's args is passed to the named function; its return becomes the field value.

### Formatter-level

```yaml
formatted:
  description: "OHMA Message"
  items:
    - { type: position, ... }
    - { type: custom, custom: ohma_payload_item }
```

Single formatter item dispatched to a hand-written function.

## Per-language conventions

Hand-written functions live in `runtimes/<lang>/escape_hatches/`. They MUST exist with the exact name declared in the spec (codegen does not check this — language build does).

### TypeScript

```ts
// runtimes/typescript/escape_hatches/arinc_702.ts
import type { DecoderPlugin, DecodeResult, Message, Options } from "../index.js";

export function arinc_702_dispatch(
  plugin: DecoderPlugin,
  message: Message,
  result: DecodeResult,
  options: Options,
): DecodeResult {
  // hand-written port of acars-decoder-typescript/lib/plugins/ARINC_702.ts
}
```

### Rust

```rust
// runtimes/rust/src/escape_hatches/arinc_702.rs
use crate::{DecodeResult, Message, Options, Plugin};

pub fn arinc_702_dispatch(
    plugin: &dyn Plugin,
    message: &Message,
    result: &mut DecodeResult,
    options: &Options,
) -> DecodeResult { /* ... */ }
```

### C

```c
/* runtimes/c/src/escape_hatches/arinc_702.c */
#include "ads_runtime.h"

DecodeResult *arinc_702_dispatch(
    const Plugin *plugin,
    const Message *message,
    DecodeResult *result,
    const Options *options
);
```

## Current escape hatch inventory

These survive declarative description as of v1:

| Hatch                                  | Plugin           | Why                                            |
|----------------------------------------|------------------|------------------------------------------------|
| `arinc_702_dispatch`                   | ARINC_702        | Heuristic offset computation, fallback chain   |
| `arinc_702_format`                     | ARINC_702        | Result merging from recursive helper           |
| `label_4a_variant_2_decode`            | Label_4A         | Substring slicing of fields[0] and fields[1]   |
| `label_4a_variant_3_position`          | Label_4A         | Concat + sanitize fields[4]+fields[5]          |
| `label_4a_format`                      | Label_4A         | Variant-specific item ordering                 |
| `ohma_unwrap_message`                  | Label_H1_OHMA    | JSON-in-JSON unwrap (try/catch chains)         |
| `ohma_message_item`                    | Label_H1_OHMA    | Pretty-print + fallback                        |
| `parse_flight_level_or_ground`         | Label_44_POS     | "GRD"/"***" → 0 sentinel                       |
| `flight_level_to_altitude_feet`        | Label_44_POS     | flight_level * 100                             |

When porting a TS plugin reveals a needed hatch, file a follow-up to evaluate adding the equivalent DSL primitive in ADS v1.1.
