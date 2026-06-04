import { readFileSync } from "node:fs";
import { parse as parseYaml } from "yaml";
import { validateSpec } from "./validate.js";
import type {
  BitField,
  ChecksumAlgorithmIR,
  Condition,
  DecodeCall,
  FieldIR,
  FormattedIR,
  FormatterCall,
  ParseBlock,
  ParseStep,
  SpecIR,
  ValueExpr,
  VariantIR,
} from "./ir.js";

/**
 * Loads a single spec YAML file, validates against schema, and lowers to IR.
 */
export function loadSpec(path: string): SpecIR {
  const raw = readFileSync(path, "utf8");
  const parsed = parseYaml(raw);
  validateSpec(parsed, path);
  return lowerToIR(parsed, path);
}

function lowerToIR(spec: any, sourcePath: string): SpecIR {
  return {
    specVersion: spec.spec_version,
    plugin: {
      name: spec.plugin.name,
      type: spec.plugin.type,
      docs: spec.plugin.docs,
      decodeLevel: spec.plugin.decode_level ?? "MESSAGE",
    },
    qualifiers: {
      labels: spec.qualifiers.labels,
      preambles: spec.qualifiers.preambles,
    },
    parse: lowerParse(spec.parse),
    fields: spec.fields?.map(lowerField),
    variants: spec.variants?.map(lowerVariant),
    checksum: spec.checksum?.map(lowerChecksum),
    onChecksumFail: spec.on_checksum_fail,
    formatted: lowerFormatted(spec.formatted),
    sourcePath,
  };
}

function lowerParse(parse: any): ParseBlock {
  if (parse && typeof parse === "object" && "custom" in parse && !Array.isArray(parse)) {
    return { kind: "custom", name: parse.custom };
  }
  if (!Array.isArray(parse)) {
    throw new Error(`Invalid parse block: expected array or {custom}, got ${typeof parse}`);
  }
  return { kind: "steps", steps: parse.map(lowerStep) };
}

function lowerStep(step: any): ParseStep {
  const into: string | undefined = step.into;
  if ("split" in step) {
    if (!into) throw new Error("split step requires 'into'");
    return { kind: "split", delimiter: step.split, into };
  }
  if ("regex" in step) {
    if (!into) throw new Error("regex step requires 'into'");
    return { kind: "regex", pattern: step.regex, on: step.on, into };
  }
  if ("substring" in step) {
    if (!into) throw new Error("substring step requires 'into'");
    return {
      kind: "substring",
      from: step.substring.from,
      start: step.substring.start,
      length: step.substring.length,
      end: step.substring.end,
      into,
    };
  }
  if ("require_length" in step) {
    if (step.else !== "fail") {
      throw new Error("require_length currently requires 'else: fail'");
    }
    return {
      kind: "require_length",
      var: step.require_length.var,
      equals: step.require_length.equals,
      min: step.require_length.min,
      max: step.require_length.max,
      onFail: "fail",
    };
  }
  if ("bitfield" in step) {
    return {
      kind: "bitfield",
      source: step.bitfield.source,
      fields: step.bitfield.fields.map(lowerBitField),
    };
  }
  if ("decode_ascii85" in step) {
    if (!into) throw new Error("decode_ascii85 step requires 'into'");
    return { kind: "decode_ascii85", source: step.decode_ascii85, into };
  }
  if ("deflate" in step) {
    if (!into) throw new Error("deflate step requires 'into'");
    return {
      kind: "deflate",
      source: step.deflate.source,
      offset: step.deflate.offset,
      format: step.deflate.format ?? "raw",
      into,
    };
  }
  if ("base64" in step) {
    if (!into) throw new Error("base64 step requires 'into'");
    return { kind: "base64", source: step.base64, into };
  }
  if ("text_decode" in step) {
    if (!into) throw new Error("text_decode step requires 'into'");
    return {
      kind: "text_decode",
      source: step.text_decode.source,
      encoding: step.text_decode.encoding ?? "utf-8",
      into,
    };
  }
  if ("hex_decode" in step) {
    if (!into) throw new Error("hex_decode step requires 'into'");
    return { kind: "hex_decode", source: step.hex_decode, into };
  }
  if ("concat_bits" in step) {
    if (!into) throw new Error("concat_bits step requires 'into'");
    return { kind: "concat_bits", sources: step.concat_bits, into };
  }
  if ("custom" in step) {
    return { kind: "custom", name: step.custom };
  }
  throw new Error(`Unknown parse step: ${JSON.stringify(step)}`);
}

function lowerBitField(bf: any): BitField {
  const [start, end] = bf.bits.split(":").map((s: string) => Number(s));
  return { name: bf.name, bitStart: start, bitEnd: end };
}

function lowerField(field: any): FieldIR {
  return {
    name: field.name,
    from: lowerExpr(field.from),
    decode: field.decode ? lowerDecode(field.decode) : undefined,
    when: field.when ? lowerCondition(field.when) : undefined,
    default: field.default !== undefined ? lowerExpr(field.default) : undefined,
    description: field.description,
  };
}

function lowerExpr(expr: unknown): ValueExpr {
  if (typeof expr === "string" && expr.startsWith("$")) {
    return { kind: "var", ref: expr };
  }
  if (typeof expr === "object" && expr !== null) {
    if ("length" in expr && typeof (expr as any).length === "string") {
      return { kind: "call", fn: "length", arg: (expr as any).length };
    }
  }
  if (
    typeof expr === "string" ||
    typeof expr === "number" ||
    typeof expr === "boolean" ||
    expr === null
  ) {
    return { kind: "literal", value: expr as string | number | boolean | null };
  }
  throw new Error(`Unsupported value expression: ${JSON.stringify(expr)}`);
}

function lowerDecode(decode: any): DecodeCall {
  if (decode.fn === "custom") {
    return { fn: "custom", name: decode.custom, args: decode.args ?? {} };
  }
  return { fn: decode.fn, args: decode.args ?? {} };
}

function lowerCondition(cond: any): Condition {
  const keys = Object.keys(cond);
  if (keys.length !== 1) {
    throw new Error(`Condition must have exactly one key, got: ${keys.join(", ")}`);
  }
  const key = keys[0]!;
  switch (key) {
    case "equals":
      return { kind: "equals", left: lowerExpr(cond.equals[0]), right: lowerExpr(cond.equals[1]) };
    case "not_equal":
      return {
        kind: "not_equal",
        left: lowerExpr(cond.not_equal[0]),
        right: lowerExpr(cond.not_equal[1]),
      };
    case "matches":
      return { kind: "matches", var: cond.matches[0], regex: cond.matches[1] };
    case "in":
      return {
        kind: "in",
        value: lowerExpr(cond.in[0]),
        values: cond.in[1],
      };
    case "all":
      return { kind: "all", conds: cond.all.map(lowerCondition) };
    case "any":
      return { kind: "any", conds: cond.any.map(lowerCondition) };
    case "not":
      return { kind: "not", cond: lowerCondition(cond.not) };
    default:
      throw new Error(`Unknown condition kind: ${key}`);
  }
}

function lowerVariant(variant: any): VariantIR {
  if ("default" in variant) {
    return { isDefault: true, defaultAction: variant.default };
  }
  return {
    isDefault: false,
    name: variant.name,
    when: lowerCondition(variant.when),
    fields: variant.fields?.map(lowerField),
    checksum: variant.checksum?.map(lowerChecksum),
  };
}

function lowerChecksum(c: any): ChecksumAlgorithmIR {
  const [s, e] = c.on.split("..").map((n: string) => Number(n));
  let expect: ChecksumAlgorithmIR["expect"];
  if (c.expect.startsWith("tail")) {
    expect = { kind: "tail", n: Number(c.expect.slice(4)) };
  } else if (c.expect.startsWith("head")) {
    expect = { kind: "head", n: Number(c.expect.slice(4)) };
  } else {
    const [rs, re] = c.expect.split("..").map((n: string) => Number(n));
    expect = { kind: "range", startByte: rs, endByte: re };
  }
  return {
    algorithm: c.algorithm,
    when: c.when ? lowerCondition(c.when) : undefined,
    on: { startByte: s, endByte: e },
    expect,
  };
}

function lowerFormatted(formatted: any): FormattedIR {
  if ("custom" in formatted) {
    return {
      kind: "custom",
      name: formatted.custom,
      description: formatted.description ?? "Custom",
    };
  }
  return {
    kind: "structured",
    description: formatted.description,
    items: (formatted.items ?? []).map(lowerFormatterCall),
  };
}

function lowerFormatterCall(item: any): FormatterCall {
  const { type, custom, ...rest } = item;
  return {
    type,
    customName: custom,
    args: rest,
  };
}
