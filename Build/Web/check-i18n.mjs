// Checks the panel's i18n: both locale files have the same keys and placeholders, every key the page uses exists,
// and no Chinese text is left in the page outside i18n/zh-Hans.js. Exits with 1 when anything fails.
// Run from the plugin folder: node Build/Web/check-i18n.mjs
import { readFileSync, readdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import vm from 'node:vm';

const here = path.dirname(fileURLToPath(import.meta.url));
const web = path.resolve(here, '../../Resources/Web');
const LOCALES = ['en', 'zh-Hans'];
const SOURCE = 'en';

// Plural forms i18next adds to a key, and the placeholders it interpolates
const PLURAL_SUFFIX = /_(zero|one|two|few|many|other)$/;
const PLACEHOLDER = /\{\{\s*([\w.]+)\s*\}\}/g;

// Uses in the page: t('key') and t("key") in scripts, data-i18n*="key" in markup
const SCRIPT_USE = /\bt\(\s*['"]([\w.-]+)['"]/g;
const MARKUP_USE = /data-i18n(?:-title|-aria-label|-placeholder)?="([\w.-]+)"/g;

// Chinese characters and full-width punctuation
const CHINESE = /[　-〿一-鿿＀-￯]/;

// Generated icon data: its labels and notes are never shown (ShuttlegentIcons.svg emits aria-hidden markup only)
const CHINESE_ALLOWED = new Set(['icons.js']);

let failures = 0;
const fail = (message) => { console.log('FAIL ' + message); ++failures; };

function loadLocale(name) {
  const sandbox = { window: {} };
  vm.runInNewContext(readFileSync(path.join(web, 'i18n', name + '.js'), 'utf8'), sandbox);
  const table = sandbox.window.ShuttlegentLocales && sandbox.window.ShuttlegentLocales[name];
  if (!table) throw new Error('i18n/' + name + '.js does not define window.ShuttlegentLocales["' + name + '"]');
  return table;
}

function flatten(table, prefix, out) {
  for (const [key, value] of Object.entries(table)) {
    const full = prefix ? prefix + '.' + key : key;
    if (value && typeof value === 'object') flatten(value, full, out);
    else out.set(full, String(value));
  }
  return out;
}

// Names a key interpolates across all its forms; count drives plurals and may be left out of a form
const placeholders = (text) => [...new Set([...text.matchAll(PLACEHOLDER)].map((match) => match[1]).filter((name) => name !== 'count'))].sort().join(',');

// Base key (plural suffix removed) → the texts of all its forms
function byBase(flat) {
  const bases = new Map();
  for (const [key, text] of flat) {
    const base = key.replace(PLURAL_SUFFIX, '');
    if (!bases.has(base)) bases.set(base, []);
    bases.get(base).push(text);
  }
  return bases;
}

const tables = Object.fromEntries(LOCALES.map((name) => [name, byBase(flatten(loadLocale(name), '', new Map()))]));
const source = tables[SOURCE];

// 1. Same keys and the same placeholders in every locale
for (const name of LOCALES) {
  if (name === SOURCE) continue;
  for (const [base, texts] of source) {
    const other = tables[name].get(base);
    if (!other) { fail(name + ': missing key ' + base); continue; }
    const want = placeholders(texts.join(' '));
    const got = placeholders(other.join(' '));
    if (want !== got) fail(name + ': placeholders of ' + base + ' are {' + got + '}, expected {' + want + '}');
  }
  for (const base of tables[name].keys()) {
    if (!source.has(base)) fail(name + ': key ' + base + ' is not in ' + SOURCE);
  }
}

// 2. Every key used by the page exists; keys nothing uses are listed
const pageFiles = readdirSync(web).filter((file) => /\.(js|html)$/.test(file)).map((file) => path.join(web, file));
const used = new Set();
for (const file of pageFiles) {
  const text = readFileSync(file, 'utf8');
  for (const pattern of [SCRIPT_USE, MARKUP_USE]) {
    for (const match of text.matchAll(pattern)) used.add(match[1]);
  }
}
for (const key of used) {
  if (!source.has(key)) fail('used but not defined: ' + key);
}
const unused = [...source.keys()].filter((key) => !used.has(key));

// 3. No Chinese outside the Chinese locale file
for (const file of pageFiles) {
  if (CHINESE_ALLOWED.has(path.basename(file))) continue;
  const lines = readFileSync(file, 'utf8').split(/\r?\n/);
  lines.forEach((line, index) => {
    if (CHINESE.test(line)) fail('Chinese text in ' + path.relative(web, file) + ':' + (index + 1) + ': ' + line.trim().slice(0, 120));
  });
}

console.log('keys ' + source.size + ', used ' + used.size + ', unused ' + unused.length + (unused.length ? ': ' + unused.join(', ') : ''));
console.log(failures ? failures + ' problem(s)' : 'OK');
process.exit(failures ? 1 : 0);
