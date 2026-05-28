import type {
  Condition,
  DecodeCall,
  FieldIR,
  FormatterCall,
  FormattedIR,
  ParseStep,
  SpecIR,
  ValueExpr,
  VariantIR,
} from "./ir.js";

/**
 * Emit idiomatic TypeScript plugin source from a SpecIR.
 *
 * Output:
 *   import { DecoderPlugin } from "@airframes/ads-runtime-ts";
 *   import * as helpers from "@airframes/ads-runtime-ts/helpers";
 *   import * as hatches from "../escape_hatches";
 *   export class <name> extends DecoderPlugin { ... }
 *
 * The runtime-ts package re-exports the existing DecoderPlugin / ResultFormatter /
 * utils API from runtimes/typescript/. Stage 2 wires those exports up.
 */
export function emitTypeScript(spec: SpecIR): string {
  const cls = spec.plugin.name;
  const slug = pluginNameToSlug(cls);

  const out: string[] = [];
  out.push(`// AUTO-GENERATED from ${spec.sourcePath}. Do not edit.`);
  out.push(`// Plugin: ${cls}`);
  if (spec.plugin.docs) out.push(`// Docs: ${spec.plugin.docs}`);
  out.push("");
  out.push(`import { DecoderPlugin } from "@airframes/ads-runtime-ts";`);
  out.push(
    `import type { DecodeResult, Message, Options } from "@airframes/ads-runtime-ts";`,
  );
  out.push(`import { ResultFormatter } from "@airframes/ads-runtime-ts";`);
  out.push(`import * as helpers from "@airframes/ads-runtime-ts/helpers";`);
  out.push(`import * as hatches from "../escape_hatches";`);
  out.push("");

  out.push(`export class ${cls} extends DecoderPlugin {`);
  out.push(`  name = ${JSON.stringify(slug)};`);
  out.push("");
  out.push(`  qualifiers() {`);
  out.push(`    return {`);
  out.push(`      labels: ${JSON.stringify(spec.qualifiers.labels)},`);
  if (spec.qualifiers.preambles) {
    out.push(`      preambles: ${JSON.stringify(spec.qualifiers.preambles)},`);
  }
  out.push(`    };`);
  out.push(`  }`);
  out.push("");

  out.push(`  decode(message: Message, options: Options = {}): DecodeResult {`);
  out.push(
    `    const result = this.initResult(message, ${JSON.stringify(formattedDescription(spec.formatted))});`,
  );
  out.push("");

  // Parse phase.
  if (spec.parse.kind === "custom") {
    out.push(`    return hatches.${spec.parse.name}(this, message, result, options);`);
    out.push(`  }`);
    out.push(`}`);
    return out.join("\n") + "\n";
  }

  // Pre-scan the formatter for $varname references. Fields whose name is
  // consumed by a formatter call don't need an auto-emit `result.raw.X = X`,
  // because the formatter writes to `result.raw` under its own canonical key
  // (position, altitude, callsign, …). Without this suppression, the
  // generated code writes both raw.latitude AND raw.position, diverging from
  // the hand-written plugins (which only have raw.position).
  const consumedByFormatter = collectFormatterRefs(spec.formatted);

  for (const step of spec.parse.steps) {
    emitParseStep(step, out, "    ");
  }

  // Fields or Variants.
  if (spec.variants) {
    emitVariants(spec.variants, out, "    ", consumedByFormatter);
  } else if (spec.fields) {
    for (const field of spec.fields) {
      emitField(field, out, "    ", consumedByFormatter);
    }
  }

  // Formatter.
  emitFormatted(spec.formatted, out, "    ");

  // Success path.
  if (!hasExplicitDecodeLevelSetting(spec)) {
    const level = spec.plugin.decodeLevel.toLowerCase();
    const tsLevel = level === "full" ? "'full'" : "'partial'";
    out.push(`    this.setDecodeLevel(result, true, ${tsLevel});`);
  }
  out.push(`    return result;`);
  out.push(`  }`);
  out.push(`}`);
  return out.join("\n") + "\n";
}

function emitParseStep(step: ParseStep, out: string[], indent: string): void {
  switch (step.kind) {
    case "split":
      out.push(
        `${indent}const ${step.into} = message.text.split(${JSON.stringify(step.delimiter)});`,
      );
      break;
    case "regex": {
      const onExpr = renderExpr({ kind: "var", ref: step.on });
      const re = `new RegExp(${JSON.stringify(step.pattern)})`;
      out.push(`${indent}const ${step.into}_match = ${onExpr}.match(${re});`);
      out.push(`${indent}if (!${step.into}_match?.groups) {`);
      out.push(
        `${indent}  return this.failUnknown(result, message.text, options);`,
      );
      out.push(`${indent}}`);
      out.push(`${indent}const ${step.into} = ${step.into}_match.groups;`);
      break;
    }
    case "substring": {
      const fromExpr = renderExpr({ kind: "var", ref: step.from });
      if (step.length !== undefined) {
        out.push(
          `${indent}const ${step.into} = ${fromExpr}.substring(${step.start}, ${step.start + step.length});`,
        );
      } else if (step.end !== undefined) {
        out.push(
          `${indent}const ${step.into} = ${fromExpr}.substring(${step.start}, ${step.end});`,
        );
      } else {
        out.push(`${indent}const ${step.into} = ${fromExpr}.substring(${step.start});`);
      }
      break;
    }
    case "require_length": {
      const checks: string[] = [];
      if (step.equals !== undefined) checks.push(`${step.var}.length !== ${step.equals}`);
      if (step.min !== undefined) checks.push(`${step.var}.length < ${step.min}`);
      if (step.max !== undefined) checks.push(`${step.var}.length > ${step.max}`);
      out.push(`${indent}if (${checks.join(" || ")}) {`);
      out.push(`${indent}  return this.failUnknown(result, message.text, options);`);
      out.push(`${indent}}`);
      break;
    }
    case "bitfield":
      out.push(`${indent}// TODO bitfield: ${JSON.stringify(step.fields)}`);
      for (const f of step.fields) {
        out.push(
          `${indent}const ${f.name} = helpers.bitslice(${renderExpr({ kind: "var", ref: step.source })}, ${f.bitStart}, ${f.bitEnd});`,
        );
      }
      break;
    case "decode_ascii85":
      out.push(
        `${indent}const ${step.into} = helpers.decodeAscii85(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "deflate": {
      const srcExpr = renderExpr({ kind: "var", ref: step.source });
      const sliced = step.offset ? `${srcExpr}.slice(${step.offset})` : srcExpr;
      out.push(
        `${indent}const ${step.into} = helpers.inflate(${sliced}, ${JSON.stringify(step.format)});`,
      );
      break;
    }
    case "base64":
      out.push(
        `${indent}const ${step.into} = helpers.base64ToUint8Array(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "text_decode":
      out.push(
        `${indent}const ${step.into} = helpers.textDecode(${renderExpr({ kind: "var", ref: step.source })}, ${JSON.stringify(step.encoding)});`,
      );
      break;
    case "hex_decode":
      out.push(
        `${indent}const ${step.into} = helpers.hexDecode(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "concat_bits":
      out.push(
        `${indent}const ${step.into} = helpers.concatBits([${step.sources
          .map((s) => renderExpr({ kind: "var", ref: s }))
          .join(", ")}]);`,
      );
      break;
    case "custom":
      out.push(`${indent}hatches.${step.name}(this, message, result, options);`);
      break;
  }
}

function emitField(
  field: FieldIR,
  out: string[],
  indent: string,
  consumedByFormatter: Set<string>,
): void {
  const decodeExpr = field.decode
    ? renderDecodeCall(field.decode, renderExpr(field.from))
    : renderExpr(field.from);
  const skipAutoRaw = consumedByFormatter.has(field.name);
  if (field.when) {
    // Declare outside the if so downstream formatters / variant-shared code
    // can still reference the variable when the guard fails — it'll be
    // undefined, matching the original hand-written plugins' implicit
    // missing-field pattern.
    out.push(`${indent}let ${field.name};`);
    out.push(`${indent}if (${renderCondition(field.when)}) {`);
    out.push(`${indent}  ${field.name} = ${decodeExpr};`);
    if (!skipAutoRaw) out.push(`${indent}  result.raw.${field.name} = ${field.name};`);
    out.push(`${indent}}`);
  } else {
    out.push(`${indent}const ${field.name} = ${decodeExpr};`);
    if (!skipAutoRaw) out.push(`${indent}result.raw.${field.name} = ${field.name};`);
  }
}

function emitVariants(
  variants: VariantIR[],
  out: string[],
  indent: string,
  consumedByFormatter: Set<string>,
): void {
  let first = true;
  for (const v of variants) {
    if (v.isDefault) {
      out.push(`${indent}${first ? "if" : "else"} {`);
      out.push(`${indent}  return this.failUnknown(result, message.text, options);`);
      out.push(`${indent}}`);
      continue;
    }
    const cond = v.when ? renderCondition(v.when) : "true";
    out.push(`${indent}${first ? "if" : "else if"} (${cond}) {`);
    if (v.fields) {
      for (const f of v.fields) {
        emitField(f, out, indent + "  ", consumedByFormatter);
      }
    }
    out.push(`${indent}}`);
    first = false;
  }
}

/**
 * Pre-scan a FormattedIR for $varname references in formatter call args.
 * Returns the set of bare variable names that are read by a formatter.
 */
function collectFormatterRefs(formatted: FormattedIR): Set<string> {
  const refs = new Set<string>();
  if (formatted.kind !== "structured") return refs;
  for (const item of formatted.items) {
    walkArgsForRefs(item.args, refs);
  }
  return refs;
}

function walkArgsForRefs(node: unknown, refs: Set<string>): void {
  if (typeof node === "string") {
    if (node.startsWith("$")) {
      // $name or $name.something or $name[N] — pull out the bare leading name.
      const m = node.match(/^\$([A-Za-z_][A-Za-z0-9_]*)/);
      if (m && m[1]) refs.add(m[1]);
    }
    return;
  }
  if (Array.isArray(node)) {
    for (const v of node) walkArgsForRefs(v, refs);
    return;
  }
  if (node && typeof node === "object") {
    for (const v of Object.values(node)) walkArgsForRefs(v, refs);
  }
}

function emitFormatted(formatted: FormattedIR, out: string[], indent: string): void {
  if (formatted.kind === "custom") {
    out.push(`${indent}hatches.${formatted.name}(result);`);
    return;
  }
  for (const item of formatted.items) {
    emitFormatterCall(item, out, indent);
  }
}

function emitFormatterCall(item: FormatterCall, out: string[], indent: string): void {
  if (item.type === "custom" && item.customName) {
    out.push(`${indent}hatches.${item.customName}(result);`);
    return;
  }
  // Map IR formatter type → ResultFormatter method.
  const methodMap: Record<string, string> = {
    position: "position",
    altitude: "altitude",
    speed: "speed",
    heading: "heading",
    timestamp: "timestamp",
    callsign: "callsign",
    flight_number: "flightNumber",
    tail_number: "tail",
    airport_origin: "departureAirport",
    airport_destination: "arrivalAirport",
    fuel: "currentFuel",
    free_text: "unknownArr",
  };
  const method = methodMap[item.type];
  if (!method) {
    out.push(`${indent}// TODO formatter: ${item.type}`);
    return;
  }
  // Render the call: ResultFormatter.<method>(result, <args>)
  // Args depend on formatter type. For now, support a few common shapes.
  const args: string[] = [];
  if (item.type === "position") {
    const lat = item.args["latitude"] ?? item.args["lat"];
    const lon = item.args["longitude"] ?? item.args["lon"];
    if (lat !== undefined && lon !== undefined) {
      args.push(`{ latitude: ${renderArg(lat)}, longitude: ${renderArg(lon)} }`);
    } else if (item.args["value"] !== undefined) {
      args.push(renderArg(item.args["value"]));
    }
  } else if (item.type === "free_text" && Array.isArray(item.args["values"])) {
    args.push(`[${(item.args["values"] as unknown[]).map(renderArg).join(", ")}]`);
  } else if ("value" in item.args) {
    args.push(renderArg(item.args["value"]));
  }
  out.push(`${indent}ResultFormatter.${method}(result, ${args.join(", ")});`);
}

function renderArg(v: unknown): string {
  if (typeof v === "string" && v.startsWith("$")) {
    return renderExpr({ kind: "var", ref: v });
  }
  return JSON.stringify(v);
}

function renderDecodeCall(call: DecodeCall, valueExpr: string): string {
  if (call.fn === "custom") {
    return `hatches.${call.name}(${valueExpr}, ${JSON.stringify(call.args)})`;
  }
  const camel = call.fn.replace(/_([a-z])/g, (_m, c) => c.toUpperCase());
  if (Object.keys(call.args).length > 0) {
    return `helpers.${camel}(${valueExpr}, ${JSON.stringify(call.args)})`;
  }
  return `helpers.${camel}(${valueExpr})`;
}

function renderExpr(expr: ValueExpr): string {
  switch (expr.kind) {
    case "var":
      return varRefToJs(expr.ref);
    case "literal":
      return JSON.stringify(expr.value);
    case "call":
      if (expr.fn === "length") return `${varRefToJs(expr.arg)}.length`;
      return "/* unknown call */";
  }
}

function varRefToJs(ref: string): string {
  // $message.text   → message.text
  // $parts[1]       → parts[1]
  // $m.unsplit_coords → m.unsplit_coords
  return ref.slice(1);
}

function renderCondition(cond: Condition): string {
  switch (cond.kind) {
    case "equals":
      return `${renderExpr(cond.left)} === ${renderExpr(cond.right)}`;
    case "not_equal":
      return `${renderExpr(cond.left)} !== ${renderExpr(cond.right)}`;
    case "matches":
      return `new RegExp(${JSON.stringify(cond.regex)}).test(${varRefToJs(cond.var)})`;
    case "in":
      return `${JSON.stringify(cond.values)}.includes(${renderExpr(cond.value)})`;
    case "all":
      return cond.conds.map((c) => `(${renderCondition(c)})`).join(" && ");
    case "any":
      return cond.conds.map((c) => `(${renderCondition(c)})`).join(" || ");
    case "not":
      return `!(${renderCondition(cond.cond)})`;
  }
}

function formattedDescription(formatted: FormattedIR): string {
  return formatted.description;
}

function hasExplicitDecodeLevelSetting(_spec: SpecIR): boolean {
  return false;
}

function pluginNameToSlug(name: string): string {
  // Plugin names are already snake-cased with capitals (Label_10_POS, ARINC_702,
  // Label_H1_OHMA). Just lowercase and swap _ for - so we match the existing
  // TS convention ("label-10-pos", "arinc-702", "label-h1-ohma").
  return name.replace(/_/g, "-").toLowerCase();
}
