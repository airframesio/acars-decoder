# Shared spec data

Cross-cutting data referenced by plugin specs and emitted into per-language runtimes by codegen. Avoids duplicating canonical constants (CRC tables, decoder semantics, coordinate format definitions) across three language implementations.

| File                  | Purpose                                                                           |
|-----------------------|-----------------------------------------------------------------------------------|
| `crc_tables.yaml`     | CRC polynomial lookup tables. Codegen emits identical tables into each runtime. |
| `decode_fns.yaml`     | Canonical semantics of named decode functions (`coordinate`, `timestamp_hhmmss`, etc.). Per-language runtimes implement these. |
| `coord_formats.yaml`  | Coordinate format definitions (single-axis vs combined, prefix chars, divisor)   |

Files in `spec/shared/` are **excluded** from plugin codegen — they are data, not plugins. The codegen tool reads them when emitting runtime constants tables.

Plugin spec files MAY reference shared data:

```yaml
decode:
  fn: coordinate
  args: { format_ref: shared/coord_formats.yaml#NS_DDDDD_DIV100 }
```

(This `format_ref` indirection is planned for ADS v1.1; v1 uses inline args.)
