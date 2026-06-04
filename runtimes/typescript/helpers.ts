/**
 * Helper functions the codegen emits calls into.
 *
 * Generated plugin code looks like:
 *   import * as helpers from "@airframes/ads-runtime-ts/helpers";
 *   const latitude = helpers.coordinate(parts[1], { ... });
 *
 * These functions wrap the underlying util modules (CoordinateUtils,
 * DateTimeUtils, ascii85, compression) in a uniform shape matching the
 * decode-fn semantics defined in spec/shared/decode_fns.yaml.
 *
 * Source of truth for behavior: acars-decoder-typescript. Behavior here MUST
 * match the inline logic of the original plugin source byte-for-byte; the
 * shared corpus enforces this.
 */

import { CoordinateUtils } from "./utils/coordinate_utils";
import { DateTimeUtils } from "./DateTimeUtils";
import { base64ToUint8Array, inflateData } from "./utils/compression";
import { ascii85Decode as ascii85Impl } from "./utils/ascii85";

// ─── coordinates ─────────────────────────────────────────────────────────────

export interface CoordinateArgs {
  style?: "single_axis" | "combined";
  axis?: "latitude" | "longitude";
  prefix_chars?: string[];
  digits?: number;
  divisor?: number;
  format?: string;
}

/**
 * Coordinate decoder. Two styles:
 *  - single_axis: input like "N12345" → ((sign) * Number(rest)) / divisor
 *  - combined: input like "N12345W123456" → {latitude, longitude} via CoordinateUtils
 */
export function coordinate(value: string, args: CoordinateArgs = {}): number | { latitude: number; longitude: number } | undefined {
  const trimmed = value.trim();
  if (args.style === "combined") {
    return CoordinateUtils.decodeStringCoordinates(trimmed);
  }
  // Default: single-axis prefixed
  const prefixChars = args.prefix_chars ?? ["N", "S", "E", "W"];
  const divisor = args.divisor ?? 1000;
  const positive = ["N", "E"];
  const prefix = trimmed[0];
  if (prefix === undefined || !prefixChars.includes(prefix)) return undefined;
  const sign = positive.includes(prefix) ? 1 : -1;
  const digits = trimmed.substring(1).trim();
  return (sign * Number(digits)) / divisor;
}

export function coordinateDecimalMinutes(value: string, _args: CoordinateArgs = {}): { latitude: number; longitude: number } | undefined {
  return CoordinateUtils.decodeStringCoordinatesDecimalMinutes(value);
}

// ─── numerics ────────────────────────────────────────────────────────────────

export interface IntegerArgs {
  substring_start?: number;
  substring_length?: number;
  multiplier?: number;
}

export function integer(value: string, args: IntegerArgs = {}): number {
  let s = value;
  if (args.substring_start !== undefined || args.substring_length !== undefined) {
    const start = args.substring_start ?? 0;
    const end = args.substring_length !== undefined ? start + args.substring_length : undefined;
    s = end !== undefined ? s.substring(start, end) : s.substring(start);
  }
  const n = Number(s);
  return args.multiplier ? n * args.multiplier : n;
}

export function float(value: string, args: { multiplier?: number } = {}): number {
  const n = Number(value);
  return args.multiplier ? n * args.multiplier : n;
}

// ─── strings ─────────────────────────────────────────────────────────────────

export function string(value: unknown): string {
  return String(value);
}
export function trim(value: string): string {
  return value.trim();
}
export function uppercase(value: string): string {
  return value.toUpperCase();
}
export function lowercase(value: string): string {
  return value.toLowerCase();
}

// ─── identifiers ─────────────────────────────────────────────────────────────

export function callsign(value: string): string {
  return value.trim();
}

export function tailNumber(value: string, args: { strip_chars?: string } = {}): string {
  let s = value.trim();
  if (args.strip_chars) {
    for (const c of args.strip_chars) s = s.split(c).join("");
  }
  return s;
}

export function flightNumber(value: string): string {
  return value.trim();
}

export function airport(value: string): string {
  return value.trim().toUpperCase();
}

// ─── timestamps ──────────────────────────────────────────────────────────────

export function timestampHhmmss(value: string, args: { append?: string } = {}): number {
  const s = args.append ? value + args.append : value;
  return DateTimeUtils.convertHHMMSSToTod(s);
}

// ─── binary / encoding ───────────────────────────────────────────────────────

export function base64ToUint8ArrayHelper(value: string): Uint8Array {
  return base64ToUint8Array(value);
}
export { base64ToUint8ArrayHelper as base64ToUint8Array };

export function inflate(bytes: Uint8Array, _format: "raw" | "zlib" | "gzip" = "raw"): Uint8Array {
  // inflateData(bytes, isZlib) — false → raw
  const out = inflateData(bytes, false);
  if (!out) throw new Error("inflate failed");
  return out;
}

const _textDecoderCache = new Map<string, TextDecoder>();
export function textDecode(bytes: Uint8Array, encoding: "utf-8" | "ascii" | "latin1" = "utf-8"): string {
  let dec = _textDecoderCache.get(encoding);
  if (!dec) {
    dec = new TextDecoder(encoding);
    _textDecoderCache.set(encoding, dec);
  }
  return dec.decode(bytes);
}

export function decodeAscii85(value: string): Uint8Array {
  const out = ascii85Impl(value);
  if (out === null) throw new Error("ASCII85 decode failed");
  return out;
}

export function hexDecode(value: string): Uint8Array {
  const clean = value.replace(/[^0-9a-fA-F]/g, "");
  const out = new Uint8Array(clean.length / 2);
  for (let i = 0; i < out.length; i++) {
    out[i] = parseInt(clean.substring(i * 2, i * 2 + 2), 16);
  }
  return out;
}

// ─── bitfield ────────────────────────────────────────────────────────────────

/**
 * Extract bits [start..end] (inclusive on both ends) from a byte.
 * Bit 0 = MSB by convention here (matches the spec's "0:3" = top 4 bits).
 */
export function bitslice(byte: number, start: number, end: number): number {
  const width = end - start + 1;
  const shift = 8 - end - 1;
  const mask = (1 << width) - 1;
  return (byte >> shift) & mask;
}

export function concatBits(values: number[]): number {
  // Placeholder — each spec defines its own width contract. Real impl will
  // accept (value, width) pairs in v1.1.
  return values.reduce((acc, v) => (acc << 8) | v, 0);
}
