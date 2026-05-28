//! Coordinate parsing — combined and decimal-minutes formats.
//!
//! Mirrors `runtimes/typescript/utils/coordinate_utils.ts`.

/// Combined "N12345W123456" → (latitude_millidegrees, longitude_millidegrees).
pub fn decode_combined(s: &str) -> Option<(f64, f64)> {
    let bytes = s.as_bytes();
    if bytes.len() < 13 { return None; }
    let first = bytes[0] as char;
    let (middle_idx, lon_start) = if bytes[6] == b' ' { (7, 8) } else { (6, 7) };
    if bytes.len() < lon_start + 6 { return None; }
    let middle = bytes[middle_idx] as char;
    if !(first == 'N' || first == 'S') || !(middle == 'E' || middle == 'W') {
        return None;
    }
    let lat_str = std::str::from_utf8(&bytes[1..6]).ok()?;
    let lon_str = std::str::from_utf8(&bytes[lon_start..lon_start + 6]).ok()?;
    let lat: f64 = lat_str.parse().ok()?;
    let lon: f64 = lon_str.parse().ok()?;
    let lat_sign = if first == 'N' { 1.0 } else { -1.0 };
    let lon_sign = if middle == 'E' { 1.0 } else { -1.0 };
    Some((lat / 1000.0 * lat_sign, lon / 1000.0 * lon_sign))
}

/// DDMM.M decimal-minute parser.
pub fn decode_decimal_minutes(s: &str) -> Option<(f64, f64)> {
    let bytes = s.as_bytes();
    if bytes.len() < 13 { return None; }
    let first = bytes[0] as char;
    let (middle_idx, lon_start) = if bytes[6] == b' ' { (7, 8) } else { (6, 7) };
    if bytes.len() < lon_start + 6 { return None; }
    let middle = bytes[middle_idx] as char;
    if !(first == 'N' || first == 'S') || !(middle == 'E' || middle == 'W') {
        return None;
    }
    let lat_str = std::str::from_utf8(&bytes[1..6]).ok()?;
    let lon_str = std::str::from_utf8(&bytes[lon_start..lon_start + 6]).ok()?;
    let lat_n: f64 = lat_str.parse().ok()?;
    let lon_n: f64 = lon_str.parse().ok()?;
    let lat_deg = (lat_n / 1000.0).trunc();
    let lat_min = (lat_n % 1000.0) / 10.0;
    let lon_deg = (lon_n / 1000.0).trunc();
    let lon_min = (lon_n % 1000.0) / 10.0;
    let lat_sign = if first == 'N' { 1.0 } else { -1.0 };
    let lon_sign = if middle == 'E' { 1.0 } else { -1.0 };
    Some(((lat_deg + lat_min / 60.0) * lat_sign, (lon_deg + lon_min / 60.0) * lon_sign))
}
