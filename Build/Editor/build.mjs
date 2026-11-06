// Bundles entry.mjs into one minified script the panel loads offline: Resources/Web/vendor/tiptap.js.
// Copies the i18next UMD build, which defines window.i18next, to Resources/Web/vendor/i18next.js with its license.
// Run from this folder: npm ci, then npm run build. Versions are pinned in package.json and package-lock.json.
import { build } from 'esbuild';
import { fileURLToPath } from 'node:url';
import { readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const output = path.resolve(here, '../../Resources/Web/vendor/tiptap.js');

const i18nextDir = path.join(here, 'node_modules/i18next');
const i18nextVersion = JSON.parse(readFileSync(path.join(i18nextDir, 'package.json'), 'utf8')).version;
const i18nextLicense = readFileSync(path.join(i18nextDir, 'LICENSE'), 'utf8').trim().split(/\r?\n/).map((line) => (' * ' + line).trimEnd()).join('\n');
const i18nextScript = readFileSync(path.join(i18nextDir, 'dist/umd/i18next.min.js'), 'utf8').trim();
writeFileSync(path.resolve(here, '../../Resources/Web/vendor/i18next.js'), '/*!\n * i18next ' + i18nextVersion + '\n *\n' + i18nextLicense + '\n */\n' + i18nextScript + '\n');

await build({
  entryPoints: [path.join(here, 'entry.mjs')],
  outfile: output,
  bundle: true,
  minify: true,
  format: 'iife',
  // CEF in UE 5.8 is a recent Chromium
  target: 'chrome120',
  legalComments: 'eof',
  logLevel: 'info'
});
