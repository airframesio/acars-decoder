import type { SpecIR } from "./ir.js";

/**
 * Emit idiomatic Rust trait impl from a SpecIR.
 * Filled in by task #6.
 */
export function emitRust(spec: SpecIR): string {
  return `// AUTO-GENERATED from ${spec.sourcePath}. Do not edit.\n// Plugin: ${spec.plugin.name}\n// TODO: emit body (task #6)\n`;
}
