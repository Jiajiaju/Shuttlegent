// Bundles entry.mjs into one minified script the panel loads offline: Resources/Web/vendor/tiptap.js.
// Run from this folder: npm ci, then npm run build. Versions are pinned in package.json and package-lock.json.
import { build } from 'esbuild';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const output = path.resolve(here, '../../Resources/Web/vendor/tiptap.js');

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
