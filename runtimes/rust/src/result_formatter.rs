//! ResultFormatter — pushes structured items onto `DecodeResult.formatted.items`.
//!
//! Mirrors the TS `ResultFormatter` static-class API. Function names match
//! `runtimes/typescript/utils/result_formatter.ts` so the cross-language corpus
//! sees identical output shapes.
//!
//! Every method that takes a value accepts `impl Into<Option<JsonValue>>` so
//! callers can pass either `JsonValue` or `Option<JsonValue>` (the latter
//! arises from when-gated fields where the guard may have failed). Methods
//! no-op on None / Null / NaN, matching the original hand-written plugins'
//! pattern of only emitting items when they had a real value.

use serde_json::Value as JsonValue;
use crate::plugin::{DecodeResult, FormattedItem};

pub struct ResultFormatter;

fn into_value(v: impl Into<Option<JsonValue>>) -> Option<JsonValue> {
    let opt = v.into();
    match opt {
        Some(JsonValue::Null) => None,
        Some(JsonValue::Number(ref n)) if n.as_f64().map(|f| f.is_nan()).unwrap_or(false) => None,
        other => other,
    }
}

impl ResultFormatter {
    fn push(result: &mut DecodeResult, kind: &'static str, code: &'static str, label: &str, value: String) {
        result.formatted.items.push(FormattedItem {
            kind,
            code,
            label: label.to_string(),
            value,
        });
    }

    pub fn position(
        result: &mut DecodeResult,
        latitude: impl Into<Option<JsonValue>>,
        longitude: impl Into<Option<JsonValue>>,
    ) {
        let lat_v = match into_value(latitude) { Some(v) => v, None => return };
        let lon_v = match into_value(longitude) { Some(v) => v, None => return };
        result.raw.insert("position", serde_json::json!({
            "latitude": lat_v,
            "longitude": lon_v,
        }));
        let lat = lat_v.as_f64().unwrap_or(0.0);
        let lon = lon_v.as_f64().unwrap_or(0.0);
        let lat_dir = if lat >= 0.0 { "N" } else { "S" };
        let lon_dir = if lon >= 0.0 { "E" } else { "W" };
        let display = format!("{:.3} {}, {:.3} {}", lat.abs(), lat_dir, lon.abs(), lon_dir);
        Self::push(result, "aircraft_position", "ARP", "Aircraft Position", display);
    }

    pub fn position_value(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let lat = v.get("latitude").and_then(JsonValue::as_f64).unwrap_or(0.0);
        let lon = v.get("longitude").and_then(JsonValue::as_f64).unwrap_or(0.0);
        Self::position(result, JsonValue::from(lat), JsonValue::from(lon));
    }

    pub fn altitude(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("altitude", v);
        Self::push(result, "altitude", "ALT", "Altitude", format!("{} feet", n));
    }

    pub fn speed(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("speed", v);
        Self::push(result, "speed", "SPD", "Speed", format!("{} knots", n));
    }

    pub fn heading(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("heading", v);
        Self::push(result, "heading", "HDG", "Heading", format!("{}°", n));
    }

    pub fn timestamp(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_i64().unwrap_or(0);
        result.raw.insert("message_timestamp", v);
        Self::push(result, "timestamp", "TS", "Timestamp", n.to_string());
    }

    pub fn callsign(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("callsign", v);
        Self::push(result, "callsign", "CS", "Callsign", s);
    }

    pub fn flight_number(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("flight_number", v);
        Self::push(result, "flight_number", "FLT", "Flight Number", s);
    }

    pub fn tail(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("tail", v);
        Self::push(result, "tail", "TAIL", "Tail Number", s);
    }

    pub fn departure_airport(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("departure_icao", v);
        Self::push(result, "airport_origin", "DEP", "Origin", s);
    }

    pub fn arrival_airport(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("arrival_icao", v);
        Self::push(result, "airport_destination", "ARR", "Destination", s);
    }

    pub fn fuel(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("fuel_on_board", v);
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
