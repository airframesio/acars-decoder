# Airframes Decoder Spec (ADS) — DSL Reference

ADS v1 is a YAML DSL for describing ACARS message decoders. One YAML file per plugin. Validated by `schema/ads-v1.schema.json`. Consumed by `@airframes/ads-codegen`, which emits idiomatic plugin source for TypeScript, Rust, and C (with WASM, Go, Python, etc. straightforward to add).

**Authoritative reference behavior:** `acars-decoder-typescript` is in production and is the source of truth. Every spec must produce byte-for-byte identical output when re-emitted as TS and run against the same input. `acars-message-documentation` is a secondary reference for filling gaps where TS code is incomplete.

## File layout

```
spec/
├── labels/                # one directory or file per label
│   ├── 10/POS.yaml        # label "10", preamble "POS"
│   ├── 44/POS.yaml
│   ├── H1/OHMA.yaml
│   └── 4A.yaml            # single-file plugin (no preamble distinction)
├── wildcards/             # plugins where qualifier label is "*"
│   └── arinc_702.yaml
└── shared/                # spec data emitted into runtimes (CRC tables, etc.)
    └── crc_tables.yaml
```

Filenames beginning with `_` are excluded from codegen (use for partials/includes).

## Top-level schema

Every spec file has six keys (one optional pair):

```yaml
spec_version: "1"
plugin:        { ... }   # plugin metadata
qualifiers:    { ... }   # which messages this plugin matches
parse:         [ ... ]   # parse steps OR { custom: <hatch> }
fields:        [ ... ]   # XOR with variants
variants:      [ ... ]   # XOR with fields
checksum:      [ ... ]   # optional
formatted:     { ... }   # output items
```

### `plugin`

```yaml
plugin:
  name: Label_10_POS            # PascalCase. Used verbatim as TS class name.
  type: text                    # text | binary
  docs: https://...             # optional URL to research docs
  decode_level: PARTIAL         # NONE | PARTIAL | MESSAGE | FULL (default MESSAGE)
```

### `qualifiers`

```yaml
qualifiers:
  labels: ["10"]                # required; "*" is wildcard
  preambles: ["POS"]            # optional
```

## Variable references

Strings beginning with `$` are variable references resolved at codegen time:

| Reference                | Meaning                                  |
|--------------------------|------------------------------------------|
| `$message.text`          | The raw message text                     |
| `$parts`                 | A variable defined by an earlier step    |
| `$parts[1]`              | Index into an array variable             |
| `$m.unsplit_coords`      | A named regex capture group              |

When a `$var[N]` appears inside a YAML flow sequence (`[...]`), quote it (`"$parts[1]"`) to prevent YAML from parsing the `[` as a nested sequence start.

## `parse` block

Either a list of steps OR a single escape hatch:

```yaml
parse:
  custom: arinc_702_dispatch    # full delegate to per-language native function
```

Or:

```yaml
parse:
  - { split: ",", into: parts }
  - { require_length: { var: parts, equals: 12 }, else: fail }
  - regex: "^(?<lat>...),(?<lon>...)$"
    on: $message.text
    into: m
```

### Step kinds

| Step              | Purpose                                          |
|-------------------|--------------------------------------------------|
| `split`           | `String.split(delimiter)` → array                |
| `regex`           | Match with named capture groups → object        |
| `substring`       | Extract by start/length or start/end             |
| `require_length`  | Assert array length; on fail → `failUnknown`     |
| `bitfield`        | Extract bit ranges from a byte/byte-array source |
| `decode_ascii85`  | ASCII85 → bytes                                  |
| `deflate`         | inflate (raw/zlib/gzip) bytes → bytes            |
| `base64`          | base64 → bytes                                   |
| `text_decode`     | bytes → string (utf-8 / ascii / latin1)          |
| `hex_decode`      | hex string → bytes                               |
| `concat_bits`     | join multiple bit slices into a single value     |
| `custom`          | per-language native helper                       |

Steps that produce a value require an `into: <name>` key. Variables become available to subsequent steps and to `fields`/`variants`/`formatted`.

## `fields` and `variants`

`fields` is a flat list; `variants` is a branched list — they are mutually exclusive at the top level.

### Field

```yaml
- name: latitude
  from: $parts[1]               # source expression
  decode:
    fn: coordinate              # named decode helper (see below)
    args: { ... }
  when: { ... }                 # optional condition; field assigned only if true
  default: ...                  # optional fallback value
```

### Variant

```yaml
variants:
  - name: format_11
    when: { equals: [{ length: $fields }, 11] }
    fields: [ ... ]
  - name: format_6
    when:
      all:
        - { equals: [{ length: $fields }, 6] }
        - { matches: ["$fields[0]", "^[NS]"] }
    fields: [ ... ]
  - default: fail               # fallthrough if no when matches
```

## Decode functions (`decode.fn`)

Canonical set defined in `spec/shared/decode_fns.yaml` and implemented in `runtimes/<lang>/`. Use `fn: custom, custom: <name>` for plugin-specific logic.

| `fn`                          | Purpose                                          |
|-------------------------------|--------------------------------------------------|
| `coordinate`                  | Single-axis or combined lat/lon                  |
| `coordinate_decimal_minutes`  | DDMM.M format                                    |
| `integer`, `float`            | Numeric parsing (with optional substring args)   |
| `string`, `trim`, `uppercase`, `lowercase` | String transforms                  |
| `timestamp_hhmmss`            | HHMMSS → seconds since midnight                  |
| `timestamp_ddhhmm`            | DDHHMM → epoch-style                             |
| `callsign`, `tail_number`, `flight_number`, `airport` | Identifier normalization |
| `altitude_feet`, `speed_knots`, `heading_degrees`, `fuel_kg`, `fuel_lb` | Typed numerics |
| `hex_to_bytes`, `json_parse`  | Format conversions                               |
| `custom`                      | per-language native function (`custom: <name>`)  |

## Conditions

```yaml
{ equals:     [a, b] }
{ not_equal:  [a, b] }
{ matches:    ["$var", "regex"] }      # JS-flavor regex applied to the variable value
{ in:         ["$var", [v1, v2, ...]] }
{ all:        [cond1, cond2, ...] }
{ any:        [cond1, cond2, ...] }
{ not:        cond }
```

Computed sub-expression: `{ length: $var }` returns the length of an array or string variable.

## `checksum`

```yaml
checksum:
  - algorithm: ARINC_665_CRC32
    when: { equals: [$version, 1] }
    on: "0..-4"                 # byte range slice (inclusive start, exclusive end; negative from end)
    expect: tail4               # last 4 bytes hold the expected value
on_checksum_fail:
  decoded: false                # what to do on mismatch
```

## `formatted`

Either structured items or a per-language escape hatch:

```yaml
formatted:
  description: "Position Report"
  items:
    - { type: position, latitude: $latitude, longitude: $longitude }
    - { type: altitude, value: $altitude }
```

Or:

```yaml
formatted:
  description: "Latest New Format"      # human description still required
  custom: label_4a_format                # function in runtimes/<lang>/escape_hatches/
```

Available `type`s: `position`, `altitude`, `speed`, `heading`, `timestamp`, `callsign`, `flight_number`, `tail_number`, `airport_origin`, `airport_destination`, `fuel`, `free_text`, `custom`.

## Escape hatches

Reserved for the ~5% of plugins whose logic resists declarative spec (ARINC 702 heuristic offsets, CBand recursion, MIAM CRC version dispatch, complex variant slicing).

```yaml
parse:
  custom: arinc_702_dispatch          # at the parse level
```

```yaml
- name: pos
  from: $fields
  decode: { fn: custom, custom: label_4a_variant_2_decode }   # at the field level
```

```yaml
formatted:
  description: "OHMA Message"
  custom: ohma_message_format        # at the formatter level
```

Each language ships a function by that exact name in `runtimes/<lang>/escape_hatches/`. The function receives the plugin instance, message, result-so-far, and options. See [ESCAPE_HATCHES.md](ESCAPE_HATCHES.md) for the per-language conventions.

## Validating a spec

```bash
cd codegen
npm run build
node dist/cli.js validate --spec ../spec
```

## Generating code

```bash
node dist/cli.js generate --target ts   --spec ../spec --out ../runtimes/typescript/generated
node dist/cli.js generate --target rust --spec ../spec --out ../runtimes/rust/src/generated
node dist/cli.js generate --target c    --spec ../spec --out ../runtimes/c/src/generated
```

`--check` validates and lists targets without writing files.
