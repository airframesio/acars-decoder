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
 * Emit idiomatic Rust source from a SpecIR.
 *
 * Targets a runtime API parallel to the TS shape:
 *   use ads_runtime::{Plugin, Message, Options, DecodeResult, Qualifiers, ResultFormatter, helpers};
 *   use crate::escape_hatches;
 *   pub struct LabelXyz;
 *   impl Plugin for LabelXyz { fn qualifiers(&self) -> Qualifiers { ... } fn decode(...) -> DecodeResult { ... } }
 *
 * The Stage 2 Rust impl (`acars-decoder-rust`) provides the `ads_runtime`
 * crate (from `runtimes/rust/`) with these exact types.
 */
export function emitRust(spec: SpecIR): string {
  const cls = spec.plugin.name;
  const slug = pluginNameToSlug(cls);

  const out: string[] = [];
  out.push(`// AUTO-GENERATED from ${spec.sourcePath}. Do not edit.`);
  out.push(`// Plugin: ${cls}`);
  if (spec.plugin.docs) out.push(`// Docs: ${spec.plugin.docs}`);
  out.push("");
  out.push(
    `use ads_runtime::{Plugin, Message, Options, DecodeResult, Qualifiers, ResultFormatter, helpers};`,
  );
  out.push(`use crate::escape_hatches;`);
  out.push("");
  out.push(`pub struct ${cls};`);
  out.push("");
  out.push(`impl Plugin for ${cls} {`);
  out.push(`    fn name(&self) -> &'static str { ${rustString(slug)} }`);
  out.push("");

  // qualifiers()
  out.push(`    fn qualifiers(&self) -> Qualifiers {`);
  out.push(`        Qualifiers {`);
  out.push(`            labels: vec![${spec.qualifiers.labels.map(rustString).join(", ")}],`);
  if (spec.qualifiers.preambles && spec.qualifiers.preambles.length > 0) {
    out.push(`            preambles: vec![${spec.qualifiers.preambles.map(rustString).join(", ")}],`);
  } else {
    out.push(`            preambles: vec![],`);
  }
  out.push(`        }`);
  out.push(`    }`);
  out.push("");

  // decode()
  out.push(`    fn decode(&self, message: &Message, options: &Options) -> DecodeResult {`);
  out.push(
    `        let mut result = DecodeResult::new(self.name(), ${rustString(formattedDescription(spec.formatted))});`,
  );
  out.push(`        result.set_message(message);`);
  out.push("");

  if (spec.parse.kind === "custom") {
    out.push(
      `        return escape_hatches::${spec.parse.name}(self, message, &mut result, options);`,
    );
    out.push(`    }`);
    out.push(`}`);
    return out.join("\n") + "\n";
  }

  // Suppress raw auto-emit for fields consumed by a formatter (see TS emitter).
  const consumedByFormatter = collectFormatterRefs(spec.formatted);

  for (const step of spec.parse.steps) {
    emitParseStep(step, out, "        ");
  }

  if (spec.variants) {
    emitVariants(spec.variants, out, "        ", consumedByFormatter);
  } else if (spec.fields) {
    for (const field of spec.fields) {
      emitField(field, out, "        ", consumedByFormatter);
    }
  }

  emitFormatted(spec.formatted, out, "        ");

  out.push(`        result.set_decoded(true);`);
  out.push(`        result`);
  out.push(`    }`);
  out.push(`}`);
  return out.join("\n") + "\n";
}

function emitParseStep(step: ParseStep, out: string[], indent: string): void {
  switch (step.kind) {
    case "split":
      out.push(
        `${indent}let ${step.into}: Vec<&str> = message.text.split(${rustString(step.delimiter)}).collect();`,
      );
      break;
    case "regex": {
      const onExpr = renderExpr({ kind: "var", ref: step.on });
      out.push(`${indent}let ${step.into}_re = helpers::regex(${rustString(step.pattern)});`);
      // .captures(&str) — pass a reference; works whether onExpr is String
      // (message.text) or &str (parts[n]) via Rust's deref coercion.
      out.push(`${indent}let ${step.into} = match ${step.into}_re.captures(&${onExpr}) {`);
      out.push(`${indent}    Some(c) => c,`);
      out.push(`${indent}    None => return result.fail_unknown(&message.text),`);
      out.push(`${indent}};`);
      break;
    }
    case "substring": {
      const fromExpr = renderExpr({ kind: "var", ref: step.from });
      if (step.length !== undefined) {
        out.push(
          `${indent}let ${step.into} = helpers::substring(${fromExpr}, ${step.start}, Some(${step.start + step.length}));`,
        );
      } else if (step.end !== undefined) {
        out.push(
          `${indent}let ${step.into} = helpers::substring(${fromExpr}, ${step.start}, Some(${step.end}));`,
        );
      } else {
        out.push(`${indent}let ${step.into} = helpers::substring(${fromExpr}, ${step.start}, None);`);
      }
      break;
    }
    case "require_length": {
      const checks: string[] = [];
      if (step.equals !== undefined) checks.push(`${step.var}.len() != ${step.equals}`);
      if (step.min !== undefined) checks.push(`${step.var}.len() < ${step.min}`);
      if (step.max !== undefined) checks.push(`${step.var}.len() > ${step.max}`);
      out.push(`${indent}if ${checks.join(" || ")} {`);
      out.push(`${indent}    return result.fail_unknown(&message.text);`);
      out.push(`${indent}}`);
      break;
    }
    case "bitfield":
      for (const f of step.fields) {
        out.push(
          `${indent}let ${f.name} = helpers::bitslice(${renderExpr({ kind: "var", ref: step.source })}, ${f.bitStart}, ${f.bitEnd});`,
        );
      }
      break;
    case "decode_ascii85":
      out.push(
        `${indent}let ${step.into} = helpers::decode_ascii85(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "deflate": {
      const srcExpr = renderExpr({ kind: "var", ref: step.source });
      // helpers::inflate takes &[u8]; src is Vec<u8>, so always borrow.
      const sliced = step.offset ? `&${srcExpr}[${step.offset}..]` : `&${srcExpr}`;
      out.push(
        `${indent}let ${step.into} = helpers::inflate(${sliced}, ${rustString(step.format)});`,
      );
      break;
    }
    case "base64":
      out.push(
        `${indent}let ${step.into} = helpers::base64_decode(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "text_decode":
      // helpers::text_decode takes &[u8]; src is Vec<u8>, so borrow.
      out.push(
        `${indent}let ${step.into} = helpers::text_decode(&${renderExpr({ kind: "var", ref: step.source })}, ${rustString(step.encoding)});`,
      );
      break;
    case "hex_decode":
      out.push(
        `${indent}let ${step.into} = helpers::hex_decode(${renderExpr({ kind: "var", ref: step.source })});`,
      );
      break;
    case "concat_bits":
      out.push(
        `${indent}let ${step.into} = helpers::concat_bits(&[${step.sources
          .map((s) => renderExpr({ kind: "var", ref: s }))
          .join(", ")}]);`,
      );
      break;
    case "custom":
      out.push(`${indent}escape_hatches::${step.name}(self, message, &mut result, options);`);
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
    // Hoist as Option so downstream formatters see the variable (None when
    // the guard fails) instead of getting a scope error. Mirrors TS's
    // `let X; if (cond) { X = ...; }` pattern.
    out.push(`${indent}let ${field.name}: Option<serde_json::Value> = if ${renderCondition(field.when)} {`);
    out.push(`${indent}    let v = ${decodeExpr};`);
    if (!skipAutoRaw) out.push(`${indent}    result.raw.insert(${rustString(field.name)}, v.clone().into());`);
    out.push(`${indent}    Some(v.into())`);
    out.push(`${indent}} else {`);
    out.push(`${indent}    None`);
    out.push(`${indent}};`);
  } else {
    out.push(`${indent}let ${field.name} = ${decodeExpr};`);
    if (!skipAutoRaw) out.push(`${indent}result.raw.insert(${rustString(field.name)}, ${field.name}.clone().into());`);
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
      out.push(`${indent}    return result.fail_unknown(&message.text);`);
      out.push(`${indent}}`);
      continue;
    }
    const cond = v.when ? renderCondition(v.when) : "true";
    out.push(`${indent}${first ? "if" : "else if"} ${cond} {`);
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
    out.push(`${indent}escape_hatches::${formatted.name}(&mut result);`);
    return;
  }
  for (const item of formatted.items) {
    emitFormatterCall(item, out, indent);
  }
}

function emitFormatterCall(item: FormatterCall, out: string[], indent: string): void {
  if (item.type === "custom" && item.customName) {
    out.push(`${indent}escape_hatches::${item.customName}(&mut result);`);
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
    out.push(`${indent}// TODO formatter: ${item.type}`);
    return;
  }
  if (item.type === "position") {
    const lat = item.args["latitude"] ?? item.args["lat"];
    const lon = item.args["longitude"] ?? item.args["lon"];
    if (lat !== undefined && lon !== undefined) {
      out.push(
        `${indent}ResultFormatter::position(&mut result, ${renderRustArg(lat)}, ${renderRustArg(lon)});`,
      );
      return;
    }
    if (item.args["value"] !== undefined) {
      out.push(
        `${indent}ResultFormatter::position_value(&mut result, ${renderRustArg(item.args["value"])});`,
      );
      return;
    }
  }
  if (item.type === "free_text" && Array.isArray(item.args["values"])) {
    // ResultFormatter::unknown_arr expects Vec<String>; convert &str args
    // via .to_string() at the call site rather than .clone() which yields &str.
    const vals = (item.args["values"] as unknown[])
      .map((v) => {
        if (typeof v === "string" && v.startsWith("$")) {
          return `${renderExpr({ kind: "var", ref: v })}.to_string()`;
        }
        return renderRustArg(v);
      })
      .join(", ");
    out.push(`${indent}ResultFormatter::unknown_arr(&mut result, vec![${vals}]);`);
    return;
  }
  if ("value" in item.args) {
    out.push(
      `${indent}ResultFormatter::${method}(&mut result, ${renderRustArg(item.args["value"])});`,
    );
  } else {
    out.push(`${indent}ResultFormatter::${method}(&mut result);`);
  }
}

function renderRustArg(v: unknown): string {
  if (typeof v === "string" && v.startsWith("$")) {
    return `${renderExpr({ kind: "var", ref: v })}.clone()`;
  }
  if (typeof v === "string") return rustString(v);
  if (typeof v === "number") return String(v);
  if (typeof v === "boolean") return String(v);
  return rustString(JSON.stringify(v));
}

function renderDecodeCall(call: DecodeCall, valueExpr: string): string {
  // Always emit two arguments — runtime helpers all accept (value, args_json)
  // even when args is empty ('{}'). Uniform signature simplifies the runtime.
  //
  // Borrow the value expr so String / owned vars are passed as &str. For
  // already-&str vars (e.g. parts[N] from Vec<&str>), Rust's auto-deref
  // collapses the extra reference layer.
  const argsJson = rustString(JSON.stringify(call.args));
  const borrowed = needsBorrow(valueExpr) ? `&${valueExpr}` : valueExpr;
  if (call.fn === "custom") {
    return `escape_hatches::${call.name}(${borrowed}, ${argsJson})`;
  }
  return `helpers::${call.fn}(${borrowed}, ${argsJson})`;
}

function needsBorrow(expr: string): boolean {
  // Skip explicit borrows / dereferences / numeric literals — they're
  // already in the right shape.
  if (expr.startsWith("&")) return false;
  if (expr.startsWith("*")) return false;
  if (/^-?\d/.test(expr)) return false;
  return true;
}

function renderExpr(expr: ValueExpr): string {
  switch (expr.kind) {
    case "var":
      return varRefToRust(expr.ref);
    case "literal":
      if (typeof expr.value === "string") return rustString(expr.value);
      return String(expr.value);
    case "call":
      if (expr.fn === "length") return `${varRefToRust(expr.arg)}.len()`;
      return "/* unknown call */";
  }
}

function varRefToRust(ref: string): string {
  // $message.text     → message.text (struct field)
  // $parts[1]         → parts[1]
  // $m.unsplit_coords → &m["unsplit_coords"]  (regex::Captures Index<&str> → str)
  //
  // The earlier code appended .as_str() on the named-group access which is
  // an unstable feature (str_as_str). Indexing a Captures by &str already
  // returns a &str, so the extra .as_str() is unnecessary and unstable.
  let body = ref.slice(1);
  body = body.replace(/^([a-z_][a-z0-9_]*)\.([a-z_][a-z0-9_]*)$/i, (_m, head, group) => {
    if (head === "message") return `${head}.${group}`;
    return `&${head}[${rustString(group)}]`;
  });
  return body;
}

function renderCondition(cond: Condition): string {
  switch (cond.kind) {
    case "equals":
      return `${renderExpr(cond.left)} == ${renderExpr(cond.right)}`;
    case "not_equal":
      return `${renderExpr(cond.left)} != ${renderExpr(cond.right)}`;
    case "matches":
      return `helpers::regex(${rustString(cond.regex)}).is_match(${varRefToRust(cond.var)})`;
    case "in":
      return `[${(cond.values as unknown[]).map(renderRustArg).join(", ")}].contains(&${renderExpr(cond.value)})`;
    case "all":
      return cond.conds.map((c) => `(${renderCondition(c)})`).join(" && ");
    case "any":
      return cond.conds.map((c) => `(${renderCondition(c)})`).join(" || ");
    case "not":
      return `!(${renderCondition(cond.cond)})`;
  }
}

function rustString(s: string): string {
  return `"${s.replace(/\\/g, "\\\\").replace(/"/g, '\\"')}"`;
}

function formattedDescription(formatted: FormattedIR): string {
  return formatted.description;
}

function pluginNameToSlug(name: string): string {
  // Smart slug: insert hyphens at camelCase boundaries so generated names
  // match the legacy TS names byte-for-byte (CBand → c-band, StarPOS →
  // star-pos, 3Line → 3-line, but Label_4A stays label-4a).
  let out = "";
  for (let i = 0; i < name.length; i++) {
    const c = name[i] as string;
    const prev = i > 0 ? (name[i - 1] as string) : "";
    const next = i + 1 < name.length ? (name[i + 1] as string) : "";
    if (i > 0 && /[A-Z]/.test(c)) {
      if (/[a-z]/.test(prev)) out += "-";
      else if (/[0-9]/.test(prev) && /[a-z]/.test(next)) out += "-";
      else if (/[A-Z]/.test(prev) && /[a-z]/.test(next)) out += "-";
    }
    out += c;
  }
  return out.replace(/_/g, "-").toLowerCase();
}
