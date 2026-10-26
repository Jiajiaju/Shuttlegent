// Rewrites the version information of the packed runtime executable so Windows shows it as Shuttlegent
// (Task Manager's Processes page reads FileDescription). Called by BuildRuntime.ps1 after the copy into Runtime/.
//
// Usage: node SetRuntimeVersionInfo.mjs <exe> <plugin version, e.g. 0.1.0> <dsh revision>
//
// Uses resedit, already installed with the DeepSeek Harness dependencies. The SEA blob is another resource of
// the executable; resedit keeps every resource it does not touch, and the regression suite checks the result runs.
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const PRODUCT_NAME = 'Shuttlegent';
const FILE_DESCRIPTION = 'Shuttlegent';
const COMPANY_NAME = 'Shuttlegent';
const INTERNAL_NAME = 'shuttlegent-runtime';
const ORIGINAL_FILENAME = 'shuttlegent-runtime.exe';

// resedit lives in the pnpm store of the submodule, not in a hoisted node_modules
const PNPM_STORE = join(dirname(fileURLToPath(import.meta.url)), '..', 'ThirdParty', 'deepseek-harness', 'node_modules', '.pnpm');
const RESEDIT_PREFIX = 'resedit@';

const [exePath, pluginVersion, dshRevision] = process.argv.slice(2);
if (!exePath || !pluginVersion) {
  console.error('Usage: node SetRuntimeVersionInfo.mjs <exe> <plugin version> [dsh revision]');
  process.exit(2);
}

const reseditDir = readdirSync(PNPM_STORE).find((name) => name.startsWith(RESEDIT_PREFIX));
if (!reseditDir) {
  console.error(`resedit not found under ${PNPM_STORE}; run BuildRuntime.ps1 without -SkipInstall first.`);
  process.exit(1);
}
const require = createRequire(join(PNPM_STORE, reseditDir, 'node_modules', 'resedit', 'package.json'));
const ResEdit = require('resedit');

// Version parts: major.minor.patch, revision 0
const parts = pluginVersion.split('.').map((part) => Number.parseInt(part, 10) || 0);
while (parts.length < 4) parts.push(0);
const versionText = parts.slice(0, 3).join('.');

const executable = ResEdit.NtExecutable.from(readFileSync(exePath), { ignoreCert: true });
const resource = ResEdit.NtExecutableResource.from(executable);
const infos = ResEdit.Resource.VersionInfo.fromEntries(resource.entries);
const info = infos.length > 0 ? infos[0] : ResEdit.Resource.VersionInfo.createEmpty();

// Every language block the executable already has gets the same strings; a new one is US English, Unicode
const languages = info.getAllLanguagesForStringValues();
if (languages.length === 0) languages.push({ lang: 1033, codepage: 1200 });
for (const language of languages) {
  info.setStringValues(language, {
    CompanyName: COMPANY_NAME,
    FileDescription: FILE_DESCRIPTION,
    ProductName: PRODUCT_NAME,
    InternalName: INTERNAL_NAME,
    OriginalFilename: ORIGINAL_FILENAME,
    FileVersion: versionText,
    ProductVersion: versionText,
    Comments: dshRevision ? `DeepSeek Harness ${dshRevision}` : 'DeepSeek Harness',
  });
}
info.setFileVersion(parts[0], parts[1], parts[2], parts[3], languages[0].lang);
info.setProductVersion(parts[0], parts[1], parts[2], parts[3], languages[0].lang);
info.outputToResourceEntries(resource.entries);
resource.outputResource(executable);

writeFileSync(exePath, Buffer.from(executable.generate()));
console.log(`Version info set: ${PRODUCT_NAME} ${versionText}`);
