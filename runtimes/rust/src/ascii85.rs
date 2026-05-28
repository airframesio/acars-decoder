//! Pure-Rust ASCII85 / Base85 decoder.
//!
//! Accepts optional `<~...~>` Adobe delimiters and the `z` shorthand for four
//! zero bytes. Returns `None` on invalid input.
//!
//! Mirrors `runtimes/typescript/utils/ascii85.ts`.

pub fn decode(input: &str) -> Option<Vec<u8>> {
    let s = input.trim();
    let s = s.strip_prefix("<~").unwrap_or(s);
    let s = s.strip_suffix("~>").unwrap_or(s);

    let mut out = Vec::with_capacity(s.len() * 4 / 5);
    let mut buf: u32 = 0;
    let mut count: u8 = 0;

    for ch in s.bytes() {
        match ch {
            b'z' if count == 0 => {
                out.extend_from_slice(&[0, 0, 0, 0]);
            }
            b'\t' | b'\n' | b'\r' | b' ' => continue,
            33..=117 => {
                buf = buf.wrapping_mul(85).wrapping_add((ch - 33) as u32);
                count += 1;
                if count == 5 {
                    out.push((buf >> 24) as u8);
                    out.push((buf >> 16) as u8);
                    out.push((buf >> 8) as u8);
                    out.push(buf as u8);
                    buf = 0;
                    count = 0;
                }
            }
            _ => return None,
        }
    }

    if count > 0 {
        for _ in count..5 {
            buf = buf.wrapping_mul(85).wrapping_add(84);
        }
        out.push((buf >> 24) as u8);
        if count >= 3 { out.push((buf >> 16) as u8); }
        if count >= 4 { out.push((buf >> 8) as u8); }
    }

    Some(out)
}
