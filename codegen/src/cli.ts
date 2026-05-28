#!/usr/bin/env node
import { Command } from "commander";
import { readdirSync, statSync, mkdirSync, writeFileSync } from "node:fs";
import { join, resolve, basename, relative } from "node:path";
import { loadSpec } from "./parse-spec.js";
import { emitTypeScript } from "./emit-typescript.js";
import { emitRust } from "./emit-rust.js";
import { emitC } from "./emit-c.js";
import { ValidationError } from "./validate.js";
import type { SpecIR } from "./ir.js";

const program = new Command();

program
  .name("ads-gen")
  .description("Codegen for the Airframes Decoder Spec (ADS)")
  .version("0.1.0");

program
  .command("generate", { isDefault: true })
  .description("Generate plugin source for a target language")
  .requiredOption("-t, --target <lang>", "Target: ts | rust | c")
  .option("-s, --spec <path>", "Path to spec/ root", "./spec")
  .requiredOption("-o, --out <path>", "Output directory")
  .option("--check", "Validate only; do not write files")
  .action((opts: { target: string; spec: string; out: string; check?: boolean }) => {
    runGenerate(opts);
  });

program
  .command("validate")
  .description("Validate all specs without emitting code")
  .option("-s, --spec <path>", "Path to spec/ root", "./spec")
  .action((opts: { spec: string }) => {
    const specs = loadAllSpecs(opts.spec);
    console.log(`OK — ${specs.length} spec(s) validated.`);
  });

function runGenerate(opts: { target: string; spec: string; out: string; check?: boolean }): void {
  const target = opts.target;
  if (target !== "ts" && target !== "rust" && target !== "c") {
    fail(`Unknown target: ${target} (expected: ts | rust | c)`);
  }
  const specs = loadAllSpecs(opts.spec);
  if (opts.check) {
    console.log(`OK — ${specs.length} spec(s) validated. Skipping emit (--check).`);
    return;
  }
  mkdirSync(opts.out, { recursive: true });

  for (const spec of specs) {
    const baseName = pluginToFileBase(spec, target);
    if (target === "ts") {
      writeFileSync(join(opts.out, `${baseName}.ts`), emitTypeScript(spec));
    } else if (target === "rust") {
      writeFileSync(join(opts.out, `${baseName}.rs`), emitRust(spec));
    } else {
      const { source, header } = emitC(spec);
      writeFileSync(join(opts.out, `${baseName}.c`), source);
      writeFileSync(join(opts.out, `${baseName}.h`), header);
    }
  }
  console.log(`Emitted ${specs.length} plugin(s) → ${opts.out}`);
}

function pluginToFileBase(spec: SpecIR, target: "ts" | "rust" | "c"): string {
  // TS keeps PascalCase to match existing convention (Label_10_POS.ts).
  // Rust + C use snake_case.
  if (target === "ts") return spec.plugin.name;
  return spec.plugin.name
    .replace(/([a-z0-9])([A-Z])/g, "$1_$2")
    .replace(/__+/g, "_")
    .toLowerCase();
}

function loadAllSpecs(specRoot: string): SpecIR[] {
  const root = resolve(specRoot);
  const yamlFiles = walkYaml(root).filter(
    (p) => !p.startsWith(join(root, "shared")) // shared/ is data, not plugin specs
  );
  const specs: SpecIR[] = [];
  for (const file of yamlFiles) {
    try {
      specs.push(loadSpec(file));
    } catch (err) {
      if (err instanceof ValidationError) {
        console.error(err.message);
      } else {
        console.error(`Failed to load ${relative(root, file)}: ${(err as Error).message}`);
      }
      process.exitCode = 1;
    }
  }
  if (process.exitCode === 1) {
    fail("Spec validation failed. See errors above.");
  }
  return specs;
}

function walkYaml(dir: string): string[] {
  const out: string[] = [];
  let entries: string[];
  try {
    entries = readdirSync(dir);
  } catch {
    return out;
  }
  for (const entry of entries) {
    if (entry.startsWith(".")) continue;
    const full = join(dir, entry);
    const s = statSync(full);
    if (s.isDirectory()) {
      out.push(...walkYaml(full));
    } else if (entry.endsWith(".yaml") || entry.endsWith(".yml")) {
      if (basename(entry).startsWith("_")) continue; // _base.yaml etc. are internal
      out.push(full);
    }
  }
  return out;
}

function fail(msg: string): never {
  console.error(msg);
  process.exit(1);
}

program.parseAsync(process.argv).catch((err) => {
  console.error(err);
  process.exit(1);
});
