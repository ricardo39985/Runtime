import { readFileSync, writeFileSync } from 'node:fs';
import Ajv from 'ajv';
import standaloneCode from 'ajv/dist/standalone/index.js';
const schema = JSON.parse(readFileSync(new URL('../../../packages/contracts/ui.schema.json', import.meta.url), 'utf8'));
const ajv = new Ajv({ strict: true, code: { source: true, esm: true } });
const validate = ajv.compile(schema);
// Compile at build time: the browser needs neither eval nor CSP unsafe-eval.
let code = standaloneCode(ajv, validate);
const helper = 'require("ajv/dist/runtime/ucs2length").default';
if (code.includes(helper)) code = 'import ucs2length from "ajv/dist/runtime/ucs2length.js";\n' + code.replaceAll(helper, 'ucs2length');
if (code.includes('require(')) throw new Error('Add an explicit ESM import for any new validator runtime helper.');
writeFileSync(new URL('../src/lib/ui-validator.generated.js', import.meta.url), '// @ts-nocheck\n// Generated from the canonical Draft 7 contract; do not edit.\n' + code);
