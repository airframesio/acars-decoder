import type { ErrorObject, ValidateFunction } from "ajv";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { createRequire } from "node:module";

// ajv 8 / ajv-formats are CJS packages; use createRequire for clean interop
// rather than wrestling with default-pluck patterns under NodeNext.
const require = createRequire(import.meta.url);
const Ajv: new (opts?: object) => { compile: (s: unknown) => ValidateFunction } =
  require("ajv/dist/2020");
const addFormats: (ajv: object) => void = require("ajv-formats");

const here = dirname(fileURLToPath(import.meta.url));

let cachedValidator: ValidateFunction | undefined;

function loadSchema(): ValidateFunction {
  if (cachedValidator) return cachedValidator;
  const schemaPath = resolve(here, "..", "..", "schema", "ads-v1.schema.json");
  const schema = JSON.parse(readFileSync(schemaPath, "utf8"));
  const ajv = new Ajv({ allErrors: true, strict: false });
  addFormats(ajv);
  const compiled = ajv.compile(schema);
  cachedValidator = compiled;
  return compiled;
}

export interface ValidationFailure {
  path: string;
  errors: ErrorObject[];
}

export class ValidationError extends Error {
  constructor(public readonly failure: ValidationFailure) {
    super(formatErrors(failure));
    this.name = "ValidationError";
  }
}

function formatErrors(failure: ValidationFailure): string {
  const header = `Spec failed JSON Schema validation: ${failure.path}`;
  const lines = failure.errors.map((e) => `  - ${e.instancePath || "/"} ${e.message ?? ""}`);
  return [header, ...lines].join("\n");
}

/**
 * Validates a parsed spec object against ads-v1.schema.json. Throws `ValidationError`
 * on failure. Path is included in the error for human-friendly messages.
 */
export function validateSpec(spec: unknown, path: string): void {
  const validator = loadSchema();
  const ok = validator(spec);
  if (!ok) {
    throw new ValidationError({ path, errors: validator.errors ?? [] });
  }
}
