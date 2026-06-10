//! ResultFormatter — pushes structured items onto `DecodeResult.formatted.items`.
//!
//! Mirrors the TS `ResultFormatter` static-class API byte-for-byte: item
//! `type`/`code`/`label`/`value` strings are cross-checked against
//! `runtimes/typescript/utils/result_formatter.ts` (the production reference)
//! so the shared corpus sees identical output from every language.
//!
//! Every method that takes a value accepts `impl Into<Option<JsonValue>>` so
//! callers can pass either `JsonValue` or `Option<JsonValue>` (the latter
//! arises from when-gated fields where the guard may have failed). Methods
//! no-op on None / Null / NaN, matching the TS pattern of guarding before
//! emitting.

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

/// `DateTimeUtils.timestampToString(time)`:
///   - < 86400        → "HH:MM:SS" (time of day)
///   - < 2678400      → "YYYY-MM-DDTHH:MM:SS" (day-of-month known, month unknown)
///   - otherwise      → full ISO-8601 without millis, "Z" suffix
pub fn timestamp_to_string(time: i64) -> String {
    if time < 86400 {
        let h = time / 3600;
        let m = (time % 3600) / 60;
        let s = time % 60;
        return format!("{:02}:{:02}:{:02}", h, m, s);
    }
    // Civil-from-days (Howard Hinnant) to render an ISO string without chrono.
    let days = time.div_euclid(86400);
    let secs = time.rem_euclid(86400);
    let (h, mi, s) = (secs / 3600, (secs % 3600) / 60, secs % 60);
    let z = days + 719468;
    let era = z.div_euclid(146097);
    let doe = z.rem_euclid(146097);
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = if m <= 2 { y + 1 } else { y };
    if time < 2678400 {
        // TS: `YYYY-MM-${iso.slice(8, 19)}` — keeps day+time, masks year/month.
        format!("YYYY-MM-{:02}T{:02}:{:02}:{:02}", d, h, mi, s)
    } else {
        format!("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", y, m, d, h, mi, s)
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

    /// Generic structured-item push for hatches that emit custom item shapes
    /// (OHMA, WRN, SQ, ATIS, …). Stores no raw field.
    pub fn push_item(result: &mut DecodeResult, kind: &'static str, code: &'static str, label: &str, value: String) {
        Self::push(result, kind, code, label, value);
    }

    // ─── Position ────────────────────────────────────────────────────────────

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
        // TS CoordinateUtils.coordinateString: strictly > 0 → N/E; 0 → S/W.
        let lat_dir = if lat > 0.0 { "N" } else { "S" };
        let lon_dir = if lon > 0.0 { "E" } else { "W" };
        let display = format!("{:.3} {}, {:.3} {}", lat.abs(), lat_dir, lon.abs(), lon_dir);
        Self::push(result, "aircraft_position", "POS", "Aircraft Position", display);
    }

    pub fn position_value(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let lat = v.get("latitude").and_then(JsonValue::as_f64).unwrap_or(0.0);
        let lon = v.get("longitude").and_then(JsonValue::as_f64).unwrap_or(0.0);
        Self::position(result, JsonValue::from(lat), JsonValue::from(lon));
    }

    // ─── Numerics ────────────────────────────────────────────────────────────

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
        Self::push(result, "heading", "HDG", "Heading", format!("{}", n));
    }

    pub fn mach(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("mach", v);
        Self::push(result, "mach", "MACH", "Mach Number", format!("{} mach", n));
    }

    pub fn groundspeed(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("groundspeed", v);
        Self::push(result, "aircraft_groundspeed", "GSPD", "Aircraft Groundspeed", format!("{} knots", n));
    }

    pub fn airspeed(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("airspeed", v);
        Self::push(result, "airspeed", "ASPD", "True Airspeed", format!("{} knots", n));
    }

    /// TS signature takes a *string* and converts M→- / P→+ before Number().
    /// No-ops on empty input.
    pub fn temperature(result: &mut DecodeResult, value: &str) {
        if value.is_empty() { return; }
        let n: f64 = value.replace('M', "-").replace('P', "+").parse().unwrap_or(f64::NAN);
        result.raw.insert("outside_air_temperature", serde_json::json!(n));
        Self::push(result, "outside_air_temperature", "OATEMP", "Outside Air Temperature (C)", format!("{} degrees", n));
    }

    pub fn total_air_temp(result: &mut DecodeResult, value: &str) {
        if value.is_empty() { return; }
        let n: f64 = value.replace('M', "-").replace('P', "+").parse().unwrap_or(f64::NAN);
        result.raw.insert("total_air_temperature", serde_json::json!(n));
        Self::push(result, "temperature", "TATEMP", "Total Air Temperature (C)", format!("{} degrees", n));
    }

    // ─── Time-of-day family (value = seconds since midnight) ────────────────

    fn push_tod(
        result: &mut DecodeResult,
        value: impl Into<Option<JsonValue>>,
        raw_key: &'static str,
        code: &'static str,
        label: &str,
    ) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_i64().unwrap_or(0);
        result.raw.insert(raw_key, v);
        Self::push(result, "time", code, label, timestamp_to_string(n));
    }

    pub fn timestamp(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_tod(result, value, "message_timestamp", "TIMESTAMP", "Message Timestamp");
    }

    pub fn eta(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_tod(result, value, "eta_time", "ETA", "Estimated Time of Arrival");
    }

    pub fn out(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_tod(result, value, "out_time", "OUT", "Out of Gate Time");
    }

    pub fn off(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_tod(result, value, "off_time", "OFF", "Takeoff Time");
    }

    pub fn on(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_tod(result, value, "on_time", "ON", "Landing Time");
    }

    pub fn r#in(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_tod(result, value, "in_time", "IN", "In Gate Time");
    }

    // ─── Calendar ────────────────────────────────────────────────────────────

    fn push_int(
        result: &mut DecodeResult,
        value: impl Into<Option<JsonValue>>,
        raw_key: &'static str,
        kind: &'static str,
        code: &'static str,
        label: &str,
    ) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_i64().unwrap_or(0);
        result.raw.insert(raw_key, v);
        Self::push(result, kind, code, label, n.to_string());
    }

    pub fn day(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_int(result, value, "day", "day", "MSG_DAY", "Day of Month");
    }

    pub fn month(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_int(result, value, "month", "month", "MSG_MON", "Month of Year");
    }

    pub fn departure_day(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_int(result, value, "departure_day", "day", "DEP_DAY", "Departure Day");
    }

    pub fn arrival_day(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::push_int(result, value, "arrival_day", "day", "ARR_DAY", "Arrival Day");
    }

    // ─── Identifiers ─────────────────────────────────────────────────────────

    pub fn callsign(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("callsign", v);
        Self::push(result, "callsign", "CALLSIGN", "Callsign", s);
    }

    pub fn flight_number(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        if s.is_empty() { return; }
        result.raw.insert("flight_number", v);
        Self::push(result, "flight_number", "FLIGHT", "Flight Number", s);
    }

    pub fn tail(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("tail", v);
        Self::push(result, "tail", "TAIL", "Tail", s);
    }

    // ─── Airports / runways ──────────────────────────────────────────────────

    pub fn departure_airport(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("departure_icao", v);
        Self::push(result, "icao", "ORG", "Origin", s);
    }

    pub fn arrival_airport(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("arrival_icao", v);
        Self::push(result, "icao", "DST", "Destination", s);
    }

    pub fn alternate_airport(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("alternate_icao", v);
        Self::push(result, "icao", "ALT_DST", "Alternate Destination", s);
    }

    pub fn arrival_runway(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("arrival_runway", v);
        Self::push(result, "runway", "ARWY", "Arrival Runway", s);
    }

    pub fn alternate_runway(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let s = v.as_str().unwrap_or("").to_string();
        result.raw.insert("alternate_runway", v);
        Self::push(result, "runway", "ALT_ARWY", "Alternate Runway", s);
    }

    // ─── Fuel ────────────────────────────────────────────────────────────────

    pub fn current_fuel(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("fuel_on_board", v);
        Self::push(result, "fuel_on_board", "FOB", "Fuel On Board", format!("{}", n));
    }

    /// Alias kept for the codegen's `fuel` formatter type.
    pub fn fuel(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        Self::current_fuel(result, value);
    }

    pub fn remaining_fuel(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_f64().unwrap_or(0.0);
        result.raw.insert("fuel_remaining", v);
        // The leading space in " FUEL_REM" is present in the TS source; kept
        // for byte parity until fixed upstream.
        Self::push(result, "fuel_remaining", " FUEL_REM", "Fuel Remaining", format!("{}", n));
    }

    // ─── Events ──────────────────────────────────────────────────────────────

    pub fn state_change(result: &mut DecodeResult, from: &str, to: &str) {
        result.raw.insert("state_change", serde_json::json!({ "from": from, "to": to }));
        // TS routes from/to through RouteUtils.formatFlightState before
        // display; the Rust runtime doesn't have that table yet, so raw codes
        // are emitted. Revisit when RouteUtils lands (corpus will flag it).
        Self::push(result, "state_change", "STATE_CHANGE", "State Change", format!("{} -> {}", from, to));
    }

    pub fn door_event(result: &mut DecodeResult, door: &str, state: &str) {
        result.raw.insert("door_event", serde_json::json!({ "door": door, "state": state }));
        Self::push(result, "door_event", "DOOR", "Door Event", format!("{} {}", door, state));
    }

    // ─── Free text ───────────────────────────────────────────────────────────

    pub fn text(result: &mut DecodeResult, value: &str) {
        result.raw.insert("text", serde_json::json!(value));
        Self::push(result, "text", "TEXT", "Text Message", value.to_string());
    }

    // ─── Diagnostics ─────────────────────────────────────────────────────────

    pub fn checksum(result: &mut DecodeResult, value: impl Into<Option<JsonValue>>) {
        let v = match into_value(value) { Some(v) => v, None => return };
        let n = v.as_i64().unwrap_or(0);
        result.raw.insert("checksum", v);
        // TS: '0x' + ('0000' + hex).slice(-4) — always exactly 4 lowercase hex chars.
        let hex = format!("{:04x}", n);
        let tail4 = &hex[hex.len().saturating_sub(4)..];
        Self::push(result, "message_checksum", "CHECKSUM", "Message Checksum", format!("0x{}", tail4));
    }

    /// TS stores the raw field but pushes NO formatted item (the item block
    /// is commented out in the reference). Mirror that exactly.
    pub fn checksum_algorithm(result: &mut DecodeResult, value: &str) {
        result.raw.insert("checksum_algorithm", serde_json::json!(value));
    }

    // ─── Remaining-text accumulation ─────────────────────────────────────────

    pub fn unknown(result: &mut DecodeResult, value: String) {
        Self::unknown_sep(result, &value, ",");
    }

    pub fn unknown_sep(result: &mut DecodeResult, value: &str, sep: &str) {
        match result.remaining.text.as_mut() {
            Some(t) => { t.push_str(sep); t.push_str(value); }
            None => result.remaining.text = Some(value.to_string()),
        }
    }

    pub fn unknown_arr(result: &mut DecodeResult, values: Vec<String>) {
        Self::unknown_arr_sep(result, values, ",");
    }

    pub fn unknown_arr_sep(result: &mut DecodeResult, values: Vec<String>, sep: &str) {
        if values.is_empty() { return; }
        let joined = values.join(sep);
        Self::unknown_sep(result, &joined, sep);
    }
}
