/**
 * Typed intermediate representation (IR) of an ADS spec.
 *
 * Spec YAML → parsed object → validated against JSON Schema → lowered into this IR.
 * All three emitters (TS, Rust, C) consume the IR; they never look at YAML directly.
 *
 * The IR uses discriminated unions for parse steps, decode-fns, conditions, and
 * formatter calls so emitters can `switch (step.kind)` exhaustively.
 */

export type SpecVersion = "1";

export interface PluginIR {
  name: string;
  type: "text" | "binary";
  docs?: string;
  decodeLevel: "NONE" | "PARTIAL" | "MESSAGE" | "FULL";
}

export interface QualifiersIR {
  labels: string[];
  preambles?: string[];
}

export type VarRef = string;

export type ValueExpr =
  | { kind: "var"; ref: VarRef }
  | { kind: "literal"; value: string | number | boolean | null }
  | { kind: "call"; fn: "length"; arg: VarRef };

export type ParseStep =
  | { kind: "split"; delimiter: string; into: string }
  | { kind: "regex"; pattern: string; on: VarRef; into: string }
  | { kind: "substring"; from: VarRef; start: number; length?: number; end?: number; into: string }
  | { kind: "require_length"; var: string; equals?: number; min?: number; max?: number; onFail: "fail" }
  | { kind: "bitfield"; source: VarRef; fields: BitField[] }
  | { kind: "decode_ascii85"; source: VarRef; into: string }
  | { kind: "deflate"; source: VarRef; offset?: number; format: "raw" | "zlib" | "gzip"; into: string }
  | { kind: "base64"; source: VarRef; into: string }
  | { kind: "text_decode"; source: VarRef; encoding: "utf-8" | "ascii" | "latin1"; into: string }
  | { kind: "hex_decode"; source: VarRef; into: string }
  | { kind: "concat_bits"; sources: VarRef[]; into: string }
  | { kind: "custom"; name: string };

export interface BitField {
  name: string;
  bitStart: number;
  bitEnd: number;
}

export type ParseBlock =
  | { kind: "steps"; steps: ParseStep[] }
  | { kind: "custom"; name: string };

export type DecodeCall =
  | {
      fn:
        | "coordinate"
        | "coordinate_decimal_minutes"
        | "integer"
        | "float"
        | "string"
        | "trim"
        | "uppercase"
        | "lowercase"
        | "timestamp_hhmmss"
        | "timestamp_ddhhmm"
        | "hex_to_bytes"
        | "callsign"
        | "airport"
        | "tail_number"
        | "flight_number"
        | "altitude_feet"
        | "speed_knots"
        | "heading_degrees"
        | "fuel_kg"
        | "fuel_lb"
        | "json_parse";
      args: Record<string, unknown>;
    }
  | { fn: "custom"; name: string; args: Record<string, unknown> };

export interface FieldIR {
  name: string;
  from: ValueExpr;
  decode?: DecodeCall;
  when?: Condition;
  default?: ValueExpr;
  description?: string;
}

export type Condition =
  | { kind: "equals"; left: ValueExpr; right: ValueExpr }
  | { kind: "not_equal"; left: ValueExpr; right: ValueExpr }
  | { kind: "matches"; var: VarRef; regex: string }
  | { kind: "in"; value: ValueExpr; values: Array<string | number | boolean | null> }
  | { kind: "all"; conds: Condition[] }
  | { kind: "any"; conds: Condition[] }
  | { kind: "not"; cond: Condition };

export interface VariantIR {
  name?: string;
  when?: Condition;
  isDefault: boolean;
  defaultAction?: "fail";
  fields?: FieldIR[];
  checksum?: ChecksumAlgorithmIR[];
}

export interface ChecksumAlgorithmIR {
  algorithm:
    | "ARINC_665_CRC32"
    | "ARINC_6_CRC16"
    | "CRC16_IBM_SDLC_REV"
    | "CRC16_GENIBUS";
  when?: Condition;
  on: { startByte: number; endByte: number };
  expect: { kind: "tail" | "head"; n: number } | { kind: "range"; startByte: number; endByte: number };
}

export interface FormatterCall {
  type:
    | "position"
    | "altitude"
    | "speed"
    | "heading"
    | "timestamp"
    | "eta"
    | "out"
    | "off"
    | "on"
    | "in"
    | "day"
    | "month"
    | "departure_day"
    | "arrival_day"
    | "callsign"
    | "flight_number"
    | "tail_number"
    | "airport_origin"
    | "airport_destination"
    | "fuel"
    | "fuel_remaining"
    | "remaining_fields"
    | "free_text"
    | "custom";
  customName?: string;
  args: Record<string, unknown>;
}

export type FormattedIR =
  | { kind: "structured"; description: string; items: FormatterCall[] }
  | { kind: "custom"; name: string; description: string };

export interface SpecIR {
  specVersion: SpecVersion;
  plugin: PluginIR;
  qualifiers: QualifiersIR;
  parse: ParseBlock;
  fields?: FieldIR[];
  variants?: VariantIR[];
  checksum?: ChecksumAlgorithmIR[];
  onChecksumFail?: { decoded: boolean };
  formatted: FormattedIR;
  sourcePath: string;
}
