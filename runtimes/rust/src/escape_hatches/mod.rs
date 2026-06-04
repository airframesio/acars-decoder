//! Per-plugin escape-hatch functions referenced from spec YAML via `custom: <name>`.
//!
//! As specs are ported and reveal needed hatches, add the corresponding files
//! here and re-export. See `docs/ESCAPE_HATCHES.md` for the contract.
//!
//! Generated plugin code does:
//!   use crate::escape_hatches;
//!   escape_hatches::<name>(...)
//!
//! Currently expected (from reference specs):
//!   - arinc_702_dispatch (parse-level whole-plugin hatch)
//!   - arinc_702_format   (formatter-level)
//!   - label_4a_variant_2_decode (field-level)
//!   - label_4a_variant_3_position (field-level)
//!   - label_4a_format (formatter-level)
//!   - ohma_unwrap_message (field-level)
//!   - ohma_message_item (formatter-level)
//!   - parse_flight_level_or_ground (decode-fn level)
//!   - flight_level_to_altitude_feet (decode-fn level)
