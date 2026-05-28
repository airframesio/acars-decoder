import type {
  Condition,
  DecodeCall,
  FieldIR,
  FormattedIR,
  FormatterCall,
  ParseStep,
  SpecIR,
  ValueExpr,
  VariantIR,
} from "./ir.js";

/**
 * Emit C99 plugin source from a SpecIR.
 *
 * Target runtime API (from runtimes/c/include/ads_runtime.h, written in Stage 2):
 *
 *   #include "ads_runtime.h"
 *   ads_decode_result_t *<snake>_decode(const ads_message_t *msg, const ads_options_t *opts);
 *   ads_qualifiers_t      <snake>_qualifiers(void);
 *
 * Plus a registry entry for the dispatcher to find this plugin.
 *
 * Generated files:
 *   <snake>.c  — implementation
 *   <snake>.h  — function prototypes + plugin descriptor
 */
export function emitC(spec: SpecIR): { source: string; header: string } {
  const cls = spec.plugin.name;
  const snake = pluginNameToSnake(cls);
  const slug = pluginNameToSlug(cls);

  return {
    source: emitSource(spec, snake, slug),
    header: emitHeader(spec, snake),
  };
}

function emitHeader(spec: SpecIR, snake: string): string {
  const guard = `ADS_PLUGIN_${snake.toUpperCase()}_H`;
  const out: string[] = [];
  out.push(`/* AUTO-GENERATED from ${spec.sourcePath}. Do not edit. */`);
  out.push(`/* Plugin: ${spec.plugin.name} */`);
  if (spec.plugin.docs) out.push(`/* Docs: ${spec.plugin.docs} */`);
  out.push("");
  out.push(`#ifndef ${guard}`);
  out.push(`#define ${guard}`);
  out.push("");
  out.push(`#include "ads_runtime.h"`);
  out.push("");
  out.push(`ads_decode_result_t *${snake}_decode(const ads_message_t *msg, const ads_options_t *opts);`);
  out.push(`ads_qualifiers_t ${snake}_qualifiers(void);`);
  out.push("");
  out.push(`extern const ads_plugin_descriptor_t ${snake}_descriptor;`);
  out.push("");
  out.push(`#endif /* ${guard} */`);
  return out.join("\n") + "\n";
}

function emitSource(spec: SpecIR, snake: string, slug: string): string {
  const out: string[] = [];
  out.push(`/* AUTO-GENERATED from ${spec.sourcePath}. Do not edit. */`);
  out.push(`/* Plugin: ${spec.plugin.name} */`);
  out.push("");
  out.push(`#include "${snake}.h"`);
  out.push(`#include "ads_helpers.h"`);
  out.push(`#include "ads_escape_hatches.h"`);
  out.push("");

  // qualifiers
  emitQualifiers(spec, snake, out);

  // decode
  emitDecode(spec, snake, slug, out);

  // descriptor
  out.push(`const ads_plugin_descriptor_t ${snake}_descriptor = {`);
  out.push(`    .name = ${cString(slug)},`);
  out.push(`    .qualifiers = ${snake}_qualifiers,`);
  out.push(`    .decode = ${snake}_decode,`);
  out.push(`};`);
  return out.join("\n") + "\n";
}

function emitQualifiers(spec: SpecIR, snake: string, out: string[]): void {
  out.push(`static const char *const ${snake}_labels[] = {`);
  for (const l of spec.qualifiers.labels) out.push(`    ${cString(l)},`);
  out.push(`    NULL,`);
  out.push(`};`);
  if (spec.qualifiers.preambles && spec.qualifiers.preambles.length > 0) {
    out.push(`static const char *const ${snake}_preambles[] = {`);
    for (const p of spec.qualifiers.preambles) out.push(`    ${cString(p)},`);
    out.push(`    NULL,`);
    out.push(`};`);
  }
  out.push("");
  out.push(`ads_qualifiers_t ${snake}_qualifiers(void) {`);
  out.push(`    ads_qualifiers_t q = {0};`);
  out.push(`    q.labels = ${snake}_labels;`);
  if (spec.qualifiers.preambles && spec.qualifiers.preambles.length > 0) {
    out.push(`    q.preambles = ${snake}_preambles;`);
  }
  out.push(`    return q;`);
  out.push(`}`);
  out.push("");
}

function emitDecode(spec: SpecIR, snake: string, _slug: string, out: string[]): void {
  out.push(
    `ads_decode_result_t *${snake}_decode(const ads_message_t *msg, const ads_options_t *opts) {`,
  );
  out.push(`    (void)opts;`);
  out.push(
    `    ads_decode_result_t *result = ads_result_new(${cString(pluginNameToSlug(spec.plugin.name))}, ${cString(formattedDescription(spec.formatted))}, msg);`,
  );
  out.push(`    if (!result) return NULL;`);
  out.push("");

  if (spec.parse.kind === "custom") {
    out.push(`    return ads_hatch_${spec.parse.name}(msg, result, opts);`);
    out.push(`}`);
    return;
  }

  // Suppress raw auto-emit for fields consumed by a formatter (see TS emitter).
  const consumedByFormatter = collectFormatterRefs(spec.formatted);

  for (const step of spec.parse.steps) {
    emitParseStep(step, out, "    ");
  }

  if (spec.variants) {
    emitVariants(spec.variants, out, "    ", consumedByFormatter);
  } else if (spec.fields) {
    for (const field of spec.fields) {
      emitField(field, out, "    ", consumedByFormatter);
    }
  }

  emitFormatted(spec.formatted, out, "    ");
  out.push(`    ads_result_set_decoded(result, true);`);
  out.push(`    return result;`);
  out.push(`}`);
}

function emitParseStep(step: ParseStep, out: string[], indent: string): void {
  switch (step.kind) {
    case "split":
      out.push(
        `${indent}ads_str_list_t ${step.into} = ads_split(msg->text, ${cString(step.delimiter)});`,
      );
      break;
    case "regex":
      out.push(
        `${indent}ads_regex_match_t ${step.into} = ads_regex_match(${cString(step.pattern)}, ${renderExpr({ kind: "var", ref: step.on })});`,
      );
      out.push(`${indent}if (!ads_regex_match_ok(&${step.into})) {`);
      out.push(`${indent}    return ads_result_fail_unknown(result, msg->text);`);
      out.push(`${indent}}`);
      break;
    case "substring":
      out.push(
        `${indent}const char *${step.into} = ads_substring(${renderExpr({ kind: "var", ref: step.from })}, ${step.start}, ${step.length ?? step.end ?? -1});`,
      );
      break;
    case "require_length": {
      const checks: string[] = [];
      if (step.equals !== undefined) checks.push(`${step.var}.count != ${step.equals}`);
      if (step.min !== undefined) checks.push(`${step.var}.count < ${step.min}`);
      if (step.max !== undefined) checks.push(`${step.var}.count > ${step.max}`);
      out.push(`${indent}if (${checks.join(" || ")}) {`);
      out.push(`${indent}    return ads_result_fail_unknown(result, msg->text);`);
      out.push(`${indent}}`);
      break;
    }
    case "bitfield":
      for (const f of step.fields) {
        out.push(
          `${indent}uint32_t ${f.name} = ads_bitslice(${renderExpr({ kind: "var", ref: step.source })}, ${f.bitStart}, ${f.bitEnd});`,
        );
      }
      break;
    case "decode_ascii85":
      out.push(
        `${indent}ads_bytes_t ${step.into} = ads_decode_ascii85(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "deflate":
      out.push(
        `${indent}ads_bytes_t ${step.into} = ads_inflate(${renderExpr({ kind: "var", ref: step.source })}, ${step.offset ?? 0}, ${cString(step.format)});`,
      );
      break;
    case "base64":
      out.push(
        `${indent}ads_bytes_t ${step.into} = ads_base64_decode(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "text_decode":
      out.push(
        `${indent}char *${step.into} = ads_text_decode(${renderExpr({ kind: "var", ref: step.source })}, ${cString(step.encoding)});`,
      );
      break;
    case "hex_decode":
      out.push(
        `${indent}ads_bytes_t ${step.into} = ads_hex_decode(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "concat_bits":
      out.push(
        `${indent}uint32_t ${step.into} = ads_concat_bits((const uint32_t[]){${step.sources
          .map((s) => renderExpr({ kind: "var", ref: s }))
          .join(", ")}}, ${step.sources.length});`,
      );
      break;
    case "custom":
      out.push(`${indent}ads_hatch_${step.name}(msg, result, opts);`);
      break;
  }
}

function emitField(
  field: FieldIR,
  out: string[],
  indent: string,
  consumedByFormatter: Set<string>,
): void {
  const valueExpr = renderExpr(field.from);
  const decodeExpr = field.decode ? renderDecodeCall(field.decode, valueExpr) : valueExpr;
  const skipAutoRaw = consumedByFormatter.has(field.name);
  if (field.when) {
    out.push(`${indent}if (${renderCondition(field.when)}) {`);
    out.push(`${indent}    ads_value_t *${field.name} = ${decodeExpr};`);
    if (!skipAutoRaw) {
      out.push(
        `${indent}    ads_result_raw_set(result, ${cString(field.name)}, ${field.name});`,
      );
    }
    out.push(`${indent}}`);
  } else {
    out.push(`${indent}ads_value_t *${field.name} = ${decodeExpr};`);
    if (!skipAutoRaw) {
      out.push(`${indent}ads_result_raw_set(result, ${cString(field.name)}, ${field.name});`);
    }
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
      out.push(`${indent}${first ? "{" : "else {"}`);
      out.push(`${indent}    return ads_result_fail_unknown(result, msg->text);`);
      out.push(`${indent}}`);
      continue;
    }
    const cond = v.when ? renderCondition(v.when) : "true";
    out.push(`${indent}${first ? "if" : "else if"} (${cond}) {`);
    if (v.fields) {
      for (const f of v.fields) {
        emitField(f, out, indent + "    ", consumedByFormatter);
      }
    }
    out.push(`${indent}}`);
    first = false;
  }
}

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
    out.push(`${indent}ads_hatch_${formatted.name}(result);`);
    return;
  }
  for (const item of formatted.items) {
    emitFormatterCall(item, out, indent);
  }
}

function emitFormatterCall(item: FormatterCall, out: string[], indent: string): void {
  if (item.type === "custom" && item.customName) {
    out.push(`${indent}ads_hatch_${item.customName}(result);`);
    return;
  }
  const methodMap: Record<string, string> = {
    position: "position",
    altitude: "altitude",
    speed: "speed",
    heading: "heading",
    timestamp: "timestamp",
    callsign: "callsign",
    flight_number: "flight_number",
    tail_number: "tail",
    airport_origin: "departure_airport",
    airport_destination: "arrival_airport",
    fuel: "fuel",
    free_text: "unknown_arr",
  };
  const method = methodMap[item.type];
  if (!method) {
    out.push(`${indent}/* TODO formatter: ${item.type} */`);
    return;
  }
  if (item.type === "position") {
    const lat = item.args["latitude"] ?? item.args["lat"];
    const lon = item.args["longitude"] ?? item.args["lon"];
    if (lat !== undefined && lon !== undefined) {
      out.push(
        `${indent}ads_fmt_position(result, ${renderCArg(lat)}, ${renderCArg(lon)});`,
      );
      return;
    }
    if (item.args["value"] !== undefined) {
      out.push(`${indent}ads_fmt_position_value(result, ${renderCArg(item.args["value"])});`);
      return;
    }
  }
  if (item.type === "free_text" && Array.isArray(item.args["values"])) {
    const values = item.args["values"] as unknown[];
    out.push(
      `${indent}ads_fmt_unknown_arr(result, (const char *const[]){${values
        .map(renderCArg)
        .join(", ")}}, ${values.length});`,
    );
    return;
  }
  if ("value" in item.args) {
    out.push(`${indent}ads_fmt_${method}(result, ${renderCArg(item.args["value"])});`);
  } else {
    out.push(`${indent}ads_fmt_${method}(result);`);
  }
}

function renderCArg(v: unknown): string {
  if (typeof v === "string" && v.startsWith("$")) return varRefToC(v.slice(1));
  if (typeof v === "string") return cString(v);
  if (typeof v === "number") return String(v);
  if (typeof v === "boolean") return v ? "true" : "false";
  return cString(JSON.stringify(v));
}

function renderDecodeCall(call: DecodeCall, valueExpr: string): string {
  if (call.fn === "custom") {
    return `ads_hatch_${call.name}(${valueExpr}, ${cString(JSON.stringify(call.args))})`;
  }
  if (Object.keys(call.args).length > 0) {
    return `ads_decode_${call.fn}(${valueExpr}, ${cString(JSON.stringify(call.args))})`;
  }
  return `ads_decode_${call.fn}(${valueExpr})`;
}

function renderExpr(expr: ValueExpr): string {
  switch (expr.kind) {
    case "var":
      return varRefToC(expr.ref.slice(1));
    case "literal":
      if (typeof expr.value === "string") return cString(expr.value);
      if (expr.value === null) return "NULL";
      return String(expr.value);
    case "call":
      if (expr.fn === "length") return `${varRefToC(expr.arg.slice(1))}.count`;
      return "/* unknown call */";
  }
}

function varRefToC(body: string): string {
  // message.text                  → msg->text
  // parts[1]                      → parts.items[1]
  // m.unsplit_coords              → ads_regex_group(&m, "unsplit_coords")
  if (body.startsWith("message.")) {
    return body.replace(/^message\./, "msg->");
  }
  const m = body.match(/^([a-z_][a-z0-9_]*)\.([a-z_][a-z0-9_]*)$/i);
  if (m) {
    return `ads_regex_group(&${m[1]}, ${cString(m[2]!)})`;
  }
  // parts[N] → parts.items[N]
  return body.replace(/^([a-z_][a-z0-9_]*)\[(\d+)\]$/i, "$1.items[$2]");
}

function renderCondition(cond: Condition): string {
  switch (cond.kind) {
    case "equals":
      return cEquality(renderExpr(cond.left), renderExpr(cond.right), true);
    case "not_equal":
      return cEquality(renderExpr(cond.left), renderExpr(cond.right), false);
    case "matches":
      return `ads_regex_test(${cString(cond.regex)}, ${varRefToC(cond.var.slice(1))})`;
    case "in": {
      const list = (cond.values as unknown[]).map(renderCArg).join(", ");
      return `ads_str_in((const char *const[]){${list}}, ${(cond.values as unknown[]).length}, ${renderExpr(cond.value)})`;
    }
    case "all":
      return cond.conds.map((c) => `(${renderCondition(c)})`).join(" && ");
    case "any":
      return cond.conds.map((c) => `(${renderCondition(c)})`).join(" || ");
    case "not":
      return `!(${renderCondition(cond.cond)})`;
  }
}

function cEquality(left: string, right: string, equal: boolean): string {
  const op = equal ? "==" : "!=";
  // If either side is a quoted string literal, use strcmp.
  const isLit = (s: string) => s.startsWith('"');
  if (isLit(left) || isLit(right)) {
    return `(strcmp(${left}, ${right}) ${op} 0)`;
  }
  return `${left} ${op} ${right}`;
}

function cString(s: string): string {
  return `"${s.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\n/g, "\\n")}"`;
}

function formattedDescription(formatted: FormattedIR): string {
  return formatted.description;
}

function pluginNameToSnake(name: string): string {
  return name.replace(/_/g, "_").toLowerCase();
}

function pluginNameToSlug(name: string): string {
  return name.replace(/_/g, "-").toLowerCase();
}
