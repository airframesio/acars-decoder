import type { SpecIR } from "./ir.js";

/**
 * Emit C99 source + header from a SpecIR.
 * Filled in by task #7.
 */
export function emitC(spec: SpecIR): { source: string; header: string } {
  const banner = `/* AUTO-GENERATED from ${spec.sourcePath}. Do not edit. */\n/* Plugin: ${spec.plugin.name} */\n`;
  return {
    source: banner + "/* TODO: emit body (task #7) */\n",
    header: banner + "/* TODO: emit header (task #7) */\n",
  };
}
