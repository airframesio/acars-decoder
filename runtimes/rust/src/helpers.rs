//! Decode-fn helpers the codegen emits calls into.
//!
//! Mirrors `runtimes/typescript/helpers.ts`. Each function corresponds to a
//! `decode.fn` value in spec YAML.
//!
//! Uniform signature: every helper accepts `(value: &str, args_json: &str)`,
//! where `args_json` is `"{}"` when the spec specifies no args. This lets
//! the emitter always emit a 2-arg call, simplifying both sides.

use serde_json::Value as JsonValue;
use crate::ascii85;
use crate::coordinate;

fn parse_args(s: &str) -> JsonValue {
    serde_json::from_str(s).unwrap_or(JsonValue::Null)
}

// ─── coordinates ─────────────────────────────────────────────────────────────

pub fn coordinate(value: &str, args_json: &str) -> JsonValue {
    let args = parse_args(args_json);
    let trimmed = value.trim();
    let style = args.get("style").and_then(JsonValue::as_str).unwrap_or("single_axis");
    if style == "combined" {
        return match coordinate::decode_combined(trimmed) {
            Some(c) => serde_json::json!({"latitude": c.0, "longitude": c.1}),
            None => JsonValue::Null,
        };
    }
    let divisor = args.get("divisor").and_then(JsonValue::as_f64).unwrap_or(1000.0);
    let positive = ["N", "E"];
    let prefix = match trimmed.chars().next() {
        Some(c) => c.to_string(),
        None => return JsonValue::Null,
    };
    let sign: f64 = if positive.contains(&prefix.as_str()) { 1.0 } else { -1.0 };
    let digits = trimmed[1..].trim();
    let n: f64 = match digits.parse() {
        Ok(n) => n,
        Err(_) => return JsonValue::Null,
    };
    serde_json::json!(sign * n / divisor)
}

pub fn coordinate_decimal_minutes(value: &str, _args_json: &str) -> JsonValue {
    match coordinate::decode_decimal_minutes(value) {
        Some(c) => serde_json::json!({"latitude": c.0, "longitude": c.1}),
        None => JsonValue::Null,
    }
}

// ─── numerics ────────────────────────────────────────────────────────────────

pub fn integer(value: &str, args_json: &str) -> JsonValue {
    let args = parse_args(args_json);
    let mut s = value;
    let owned: String;
    let start = args.get("substring_start").and_then(JsonValue::as_u64).unwrap_or(0) as usize;
    if let Some(len) = args.get("substring_length").and_then(JsonValue::as_u64) {
        owned = s.chars().skip(start).take(len as usize).collect();
        s = &owned;
    } else if start > 0 {
        owned = s.chars().skip(start).collect();
        s = &owned;
    }
    let n: f64 = s.parse().unwrap_or(0.0);
    let mult = args.get("multiplier").and_then(JsonValue::as_f64).unwrap_or(1.0);
    JsonValue::from(n * mult)
}

pub fn float(value: &str, _args_json: &str) -> JsonValue {
    let n: f64 = value.parse().unwrap_or(0.0);
    JsonValue::from(n)
}

// ─── strings ─────────────────────────────────────────────────────────────────

pub fn string(value: &str, _args_json: &str) -> JsonValue {
    JsonValue::String(value.to_string())
}
pub fn trim(value: &str, _args_json: &str) -> JsonValue {
    JsonValue::String(value.trim().to_string())
}
pub fn uppercase(value: &str, _args_json: &str) -> JsonValue {
    JsonValue::String(value.to_uppercase())
}
pub fn lowercase(value: &str, _args_json: &str) -> JsonValue {
    JsonValue::String(value.to_lowercase())
}

// ─── identifiers ─────────────────────────────────────────────────────────────

pub fn callsign(value: &str, _args_json: &str) -> JsonValue {
    JsonValue::String(value.trim().to_string())
}
pub fn flight_number(value: &str, _args_json: &str) -> JsonValue {
    JsonValue::String(value.trim().to_string())
}
pub fn airport(value: &str, _args_json: &str) -> JsonValue {
    JsonValue::String(value.trim().to_uppercase())
}

pub fn tail_number(value: &str, args_json: &str) -> JsonValue {
    let args = parse_args(args_json);
    let mut s = value.trim().to_string();
    if let Some(strip) = args.get("strip_chars").and_then(JsonValue::as_str) {
        for c in strip.chars() {
            s = s.replace(c, "");
        }
    }
    JsonValue::String(s)
}

// ─── timestamps ──────────────────────────────────────────────────────────────

pub fn timestamp_hhmmss(value: &str, args_json: &str) -> JsonValue {
    let args = parse_args(args_json);
    let s = if let Some(app) = args.get("append").and_then(JsonValue::as_str) {
        format!("{}{}", value, app)
    } else {
        value.to_string()
    };
    parse_hhmmss_to_tod(&s)
}

fn parse_hhmmss_to_tod(s: &str) -> JsonValue {
    if s.len() < 6 { return JsonValue::Null; }
    let h: i64 = s[0..2].parse().unwrap_or(0);
    let m: i64 = s[2..4].parse().unwrap_or(0);
    let sec: i64 = s[4..6].parse().unwrap_or(0);
    JsonValue::from(h * 3600 + m * 60 + sec)
}

// ─── binary / encoding (called via parse steps, not decode-fns) ──────────────

pub fn base64_decode(value: &str) -> Vec<u8> {
    use base64::{engine::general_purpose::STANDARD, Engine};
    STANDARD.decode(value.trim()).unwrap_or_default()
}

pub fn inflate(bytes: &[u8], format: &str) -> Vec<u8> {
    use flate2::read::{DeflateDecoder, GzDecoder, ZlibDecoder};
    use std::io::Read;
    let mut out = Vec::new();
    match format {
        "zlib" => { let _ = ZlibDecoder::new(bytes).read_to_end(&mut out); }
        "gzip" => { let _ = GzDecoder::new(bytes).read_to_end(&mut out); }
        _ => { let _ = DeflateDecoder::new(bytes).read_to_end(&mut out); }
    }
    out
}

pub fn text_decode(bytes: &[u8], _encoding: &str) -> String {
    String::from_utf8_lossy(bytes).into_owned()
}

pub fn decode_ascii85(value: &str) -> Vec<u8> {
    ascii85::decode(value).unwrap_or_default()
}

pub fn hex_decode(value: &str) -> Vec<u8> {
    let clean: String = value.chars().filter(|c| c.is_ascii_hexdigit()).collect();
    (0..clean.len() / 2)
        .map(|i| u8::from_str_radix(&clean[i * 2..i * 2 + 2], 16).unwrap_or(0))
        .collect()
}

// ─── bitfield ────────────────────────────────────────────────────────────────

pub fn bitslice(byte: u8, start: u8, end: u8) -> u32 {
    let width = end - start + 1;
    let shift = 8 - end - 1;
    let mask = (1u32 << width) - 1;
    ((byte as u32) >> shift) & mask
}

pub fn concat_bits(values: &[u32]) -> u32 {
    values.iter().fold(0u32, |acc, &v| (acc << 8) | v)
}

// ─── regex ───────────────────────────────────────────────────────────────────

use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::sync::Mutex;

static REGEX_CACHE: Lazy<Mutex<HashMap<String, regex::Regex>>> = Lazy::new(|| Mutex::new(HashMap::new()));

pub fn regex(pattern: &str) -> regex::Regex {
    let mut cache = REGEX_CACHE.lock().expect("regex cache poisoned");
    cache
        .entry(pattern.to_string())
        .or_insert_with(|| regex::Regex::new(pattern).expect("invalid regex"))
        .clone()
}
