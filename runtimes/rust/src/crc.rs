//! CRC algorithms used by ACARS / ARINC 702 / MIAM PDUs.
//!
//! These mirror `runtimes/typescript/utils/arinc_702_helper.ts`. The lookup
//! tables themselves will be emitted from `spec/shared/crc_tables.yaml` in a
//! follow-up; the bitwise implementations below are correct and table-free.

/// CRC-16/IBM-SDLC with reversed nibbles, as ARINC 702 H1 messages use it.
pub fn crc16_ibm_sdlc_rev(data: &[u8]) -> u16 {
    let mut crc: u32 = 0xffff;
    for &byte in data {
        crc ^= byte as u32;
        for _ in 0..8 {
            if crc & 0x0001 != 0 {
                crc = (crc >> 1) ^ 0x8408;
            } else {
                crc >>= 1;
            }
        }
    }
    let crc = (crc ^ 0xffff) & 0xffff;
    let n1 = (crc >> 12) & 0xf;
    let n2 = (crc >> 8) & 0xf;
    let n3 = (crc >> 4) & 0xf;
    let n4 = crc & 0xf;
    ((n4 << 12) | (n3 << 8) | (n2 << 4) | n1) as u16
}

/// CRC-16/GENIBUS (poly 0x1021, init 0xffff).
pub fn crc16_genibus(data: &[u8]) -> u16 {
    let mut crc: u32 = 0xffff;
    let polynomial: u32 = 0x1021;
    for &byte in data {
        crc ^= (byte as u32) << 8;
        for _ in 0..8 {
            if crc & 0x8000 != 0 {
                crc = ((crc << 1) ^ polynomial) & 0xffff;
            } else {
                crc = (crc << 1) & 0xffff;
            }
        }
    }
    ((crc ^ 0xffff) & 0xffff) as u16
}

/// Convenience: try both CRC variants and report which (if any) matched.
pub fn match_arinc_702_crc(data: &[u8], expected: u16) -> Option<&'static str> {
    if crc16_ibm_sdlc_rev(data) == expected { return Some("IBM-SDLC"); }
    if crc16_genibus(data) == expected { return Some("GENIBUS"); }
    None
}
