//! ResultFormatter — pushes structured items onto `DecodeResult.formatted.items`.
//!
//! Mirrors the TS `ResultFormatter` static-class API. Function names match
//! `runtimes/typescript/utils/result_formatter.ts` so the cross-language corpus
//! sees identical output shapes.

use serde_json::Value as JsonValue;
use crate::plugin::{DecodeResult, FormattedItem};

pub struct ResultFormatter;

impl ResultFormatter {
    fn push(result: &mut DecodeResult, kind: &'static str, code: &'static str, label: &str, value: String) {
        result.formatted.items.push(FormattedItem {
            kind,
            code,
            label: label.to_string(),
            value,
        });
    }

    pub fn position(result: &mut DecodeResult, latitude: JsonValue, longitude: JsonValue) {
        result.raw.insert("position", serde_json::json!({
            "latitude": latitude,
            "longitude": longitude,
        }));
        let lat = latitude.as_f64().unwrap_or(0.0);
        let lon = longitude.as_f64().unwrap_or(0.0);
        let lat_dir = if lat >= 0.0 { "N" } else { "S" };
        let lon_dir = if lon >= 0.0 { "E" } else { "W" };
        let display = format!("{:.3} {}, {:.3} {}", lat.abs(), lat_dir, lon.abs(), lon_dir);
        Self::push(result, "aircraft_position", "ARP", "Aircraft Position", display);
    }

    /// Position from a pre-computed `{latitude, longitude}` value.
    pub fn position_value(result: &mut DecodeResult, value: JsonValue) {
        let lat = value.get("latitude").and_then(JsonValue::as_f64).unwrap_or(0.0);
        let lon = value.get("longitude").and_then(JsonValue::as_f64).unwrap_or(0.0);
        Self::position(result, lat.into(), lon.into());
    }

    pub fn altitude(result: &mut DecodeResult, value: JsonValue) {
        let n = value.as_f64().unwrap_or(0.0);
        result.raw.insert("altitude", value);
        Self::push(result, "altitude", "ALT", "Altitude", format!("{} feet", n));
    }

    pub fn speed(result: &mut DecodeResult, value: JsonValue) {
        let n = value.as_f64().unwrap_or(0.0);
        result.raw.insert("speed", value);
        Self::push(result, "speed", "SPD", "Speed", format!("{} knots", n));
    }

    pub fn heading(result: &mut DecodeResult, value: JsonValue) {
        let n = value.as_f64().unwrap_or(0.0);
        result.raw.insert("heading", value);
        Self::push(result, "heading", "HDG", "Heading", format!("{}°", n));
    }

    pub fn timestamp(result: &mut DecodeResult, value: JsonValue) {
        let n = value.as_i64().unwrap_or(0);
        result.raw.insert("message_timestamp", value);
        Self::push(result, "timestamp", "TS", "Timestamp", n.to_string());
    }

    pub fn callsign(result: &mut DecodeResult, value: JsonValue) {
        let s = value.as_str().unwrap_or("").to_string();
        result.raw.insert("callsign", value);
        Self::push(result, "callsign", "CS", "Callsign", s);
    }

    pub fn flight_number(result: &mut DecodeResult, value: JsonValue) {
        let s = value.as_str().unwrap_or("").to_string();
        result.raw.insert("flight_number", value);
        Self::push(result, "flight_number", "FLT", "Flight Number", s);
    }

    pub fn tail(result: &mut DecodeResult, value: JsonValue) {
        let s = value.as_str().unwrap_or("").to_string();
        result.raw.insert("tail", value);
        Self::push(result, "tail", "TAIL", "Tail Number", s);
    }

    pub fn departure_airport(result: &mut DecodeResult, value: JsonValue) {
        let s = value.as_str().unwrap_or("").to_string();
        result.raw.insert("departure_icao", value);
        Self::push(result, "airport_origin", "DEP", "Origin", s);
    }

    pub fn arrival_airport(result: &mut DecodeResult, value: JsonValue) {
        let s = value.as_str().unwrap_or("").to_string();
        result.raw.insert("arrival_icao", value);
        Self::push(result, "airport_destination", "ARR", "Destination", s);
    }

    pub fn fuel(result: &mut DecodeResult, value: JsonValue) {
        let n = value.as_f64().unwrap_or(0.0);
        result.raw.insert("fuel_on_board", value);
        Self::push(result, "fuel", "FUEL", "Fuel", n.to_string());
    }

    pub fn unknown_arr(result: &mut DecodeResult, values: Vec<String>) {
        let joined = values.join(",");
        match result.remaining.text.as_mut() {
            Some(t) => { t.push(','); t.push_str(&joined); }
            None => result.remaining.text = Some(joined),
        }
    }

    pub fn unknown(result: &mut DecodeResult, value: String) {
        match result.remaining.text.as_mut() {
            Some(t) => { t.push(','); t.push_str(&value); }
            None => result.remaining.text = Some(value),
        }
    }
}
