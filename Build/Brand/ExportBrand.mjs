// Exports every Shuttlegent mark, lockup and icon from Build/Brand/brand-geometry.json.
// Writes the SVG files directly, renders the PNG files in headless Chrome over the DevTools protocol,
// and writes Resources/Web/brand-geometry.js so the panel animates the same shapes.
//
// Usage: node ExportBrand.mjs --chrome <chrome.exe> --plugin <plugin root>

import { spawn } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

const MARK_PALETTES = ["color", "mono", "red-white", "reverse"];
const LOCKUP_SVG_PALETTES = ["color", "mono", "reverse"];
// The README's dark-mode header also needs a red-white horizontal lockup, as PNG only
const LOCKUP_PNG_PALETTES = { horizontal: ["color", "mono", "reverse", "red-white"], stacked: ["color", "mono", "reverse"] };
// Inner offset of the glyph path inside the wordmark files
const WORD_OFFSET = "translate(-5.908203 -33.88672)";

function parseArgs(argv) {
  const args = {};
  for (let i = 0; i < argv.length; i += 2) args[argv[i].replace(/^--/, "")] = argv[i + 1];
  if (!args.chrome || !args.plugin) throw new Error("Usage: node ExportBrand.mjs --chrome <chrome.exe> --plugin <plugin root>");
  return args;
}

const round = value => Number(value.toFixed(2));

// Closed Catmull-Rom spline through the silhouette, as cubic Béziers, in the shuttle's own axis frame
function shuttlePathData(profile) {
  const top = profile.slice().reverse().map(([u, h]) => [-u, -h]).concat(profile.slice(1).map(([u, h]) => [u, -h]));
  const bottom = top.slice(1, -1).reverse().map(([u, v]) => [u, -v]);
  const points = top.concat(bottom);
  const count = points.length;
  const at = index => points[(index + count) % count];
  let d = `M ${points[0][0]} ${points[0][1]}`;
  for (let index = 0; index < count; ++index) {
    const p0 = at(index - 1), p1 = at(index), p2 = at(index + 1), p3 = at(index + 2);
    const c1 = [round(p1[0] + (p2[0] - p0[0]) / 6), round(p1[1] + (p2[1] - p0[1]) / 6)];
    const c2 = [round(p2[0] - (p3[0] - p1[0]) / 6), round(p2[1] - (p3[1] - p1[1]) / 6)];
    d += ` C ${c1.join(" ")} ${c2.join(" ")} ${p2.join(" ")}`;
  }
  return d + " Z";
}

function threadRect(geometry, side, extra = "") {
  const t = geometry.thread;
  return `<rect x="${side * t.centerX - t.width / 2}" y="${t.top}" width="${t.width}" height="${t.height}" rx="${t.width / 2}"${extra}/>`;
}

// Mark shapes in the mark's own view box; idPrefix keeps mask ids unique when several marks share a page
function markShapes(geometry, shuttleD, paletteName, idPrefix) {
  const palette = geometry.palettes[paletteName];
  const thread = geometry.colors[palette.thread];
  const shuttleFill = geometry.colors[palette.shuttle];
  const rotate = `rotate(${geometry.shuttle.axisDeg})`;
  if (!palette.knockout) {
    return threadRect(geometry, -1, ` fill="${thread}"`)
      + `<path d="${shuttleD}" transform="${rotate}" fill="${shuttleFill}"/>`
      + threadRect(geometry, 1, ` fill="${thread}"`);
  }
  // One colour: the shuttle cuts a gap into the left thread, the right thread cuts a gap into the shuttle
  const [x, y, w, h] = geometry.viewBox;
  const gap = geometry.knockoutGap * 2;
  const field = `<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="#fff"/>`;
  const cut = `fill="#000" stroke="#000" stroke-width="${gap}" stroke-linejoin="round"`;
  return `<defs>`
    + `<mask id="${idPrefix}-left" maskUnits="userSpaceOnUse" x="${x}" y="${y}" width="${w}" height="${h}">${field}<path d="${shuttleD}" transform="${rotate}" ${cut}/></mask>`
    + `<mask id="${idPrefix}-shuttle" maskUnits="userSpaceOnUse" x="${x}" y="${y}" width="${w}" height="${h}">${field}${threadRect(geometry, 1, ` ${cut}`)}</mask>`
    + `</defs>`
    + `<g mask="url(#${idPrefix}-left)">${threadRect(geometry, -1, ` fill="${thread}"`)}</g>`
    + `<g mask="url(#${idPrefix}-shuttle)"><path d="${shuttleD}" transform="${rotate}" fill="${shuttleFill}"/></g>`
    + threadRect(geometry, 1, ` fill="${thread}"`);
}

function markSvg(geometry, shuttleD, paletteName, size) {
  const sizeAttrs = size ? ` width="${size}" height="${size}"` : "";
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${geometry.viewBox.join(" ")}"${sizeAttrs} role="img"><title>Shuttlegent - shuttle mark</title>`
    + markShapes(geometry, shuttleD, paletteName, "sg") + `</svg>\n`;
}

function lockupSvg(geometry, shuttleD, wordD, kind, paletteName, scale = 1) {
  const layout = geometry.lockups[kind];
  const [mx, my, ms] = layout.mark;
  const [wx, wy, ws] = layout.word;
  const word = geometry.colors[geometry.palettes[paletteName].word];
  const sizeAttrs = `width="${layout.width * scale}" height="${layout.height * scale}"`;
  return `<svg xmlns="http://www.w3.org/2000/svg" ${sizeAttrs} viewBox="0 0 ${layout.width} ${layout.height}" role="img"><title>Shuttlegent</title>`
    + `<g transform="translate(${mx} ${my}) scale(${ms})">${markShapes(geometry, shuttleD, paletteName, "sg")}</g>`
    + `<g transform="translate(${wx} ${wy}) scale(${ws}) ${WORD_OFFSET}"><path d="${wordD}" fill="${word}"/></g></svg>\n`;
}

function iconSvg(geometry, shuttleD) {
  const icon = geometry.icon;
  const inner = icon.size - icon.inset * 2;
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${icon.size}" height="${icon.size}" viewBox="0 0 ${icon.size} ${icon.size}">`
    + `<rect width="${icon.size}" height="${icon.size}" rx="${icon.radius}" fill="${geometry.colors[icon.tile]}"/>`
    + `<svg x="${icon.inset}" y="${icon.inset}" width="${inner}" height="${inner}" viewBox="${geometry.viewBox.join(" ")}"><title>Shuttlegent - shuttle mark</title>`
    + markShapes(geometry, shuttleD, icon.palette, "sg") + `</svg></svg>\n`;
}

function readWordPath(svgFile) {
  const match = readFileSync(svgFile, "utf8").match(/<path d="([^"]+)"/);
  if (!match) throw new Error(`No glyph path in ${svgFile}`);
  return match[1];
}

// Geometry for Resources/Web/mark.js and warp.js, with the shuttle path already built; warp.js parts lines by the profile
function webGeometry(geometry, shuttleD) {
  const data = {
    viewBox: geometry.viewBox,
    thread: geometry.thread,
    shuttle: { axisDeg: geometry.shuttle.axisDeg, halfLength: geometry.shuttle.profile.at(-1)[0], halfThickness: geometry.shuttle.profile[0][1], profile: geometry.shuttle.profile, d: shuttleD },
    colors: geometry.colors,
  };
  return "// Generated by Build/Brand/ExportBrand.ps1 from Build/Brand/brand-geometry.json. Do not edit.\n"
    + `window.ShuttlegentBrandGeometry = ${JSON.stringify(data, null, 2)};\n`;
}

function launchChrome(chromePath, profileDir) {
  const child = spawn(chromePath, [
    "--headless=new", "--disable-gpu", "--hide-scrollbars", "--no-first-run", "--no-default-browser-check",
    "--remote-debugging-port=0", `--user-data-dir=${profileDir}`, "about:blank",
  ], { stdio: ["ignore", "ignore", "pipe"] });
  return new Promise((resolveUrl, reject) => {
    let buffer = "";
    const timer = setTimeout(() => reject(new Error("Chrome did not report a DevTools endpoint within 15 s")), 15000);
    child.stderr.on("data", chunk => {
      buffer += chunk.toString();
      const match = buffer.match(/DevTools listening on (ws:\/\/\S+)/);
      if (match) { clearTimeout(timer); resolveUrl({ child, wsUrl: match[1] }); }
    });
    child.on("exit", code => { clearTimeout(timer); reject(new Error(`Chrome exited early with code ${code}`)); });
  });
}

function connect(wsUrl) {
  const socket = new WebSocket(wsUrl);
  let nextId = 1;
  const pending = new Map();
  socket.addEventListener("message", event => {
    const msg = JSON.parse(event.data);
    if (!msg.id || !pending.has(msg.id)) return;
    const { resolveCall, rejectCall } = pending.get(msg.id);
    pending.delete(msg.id);
    if (msg.error) rejectCall(new Error(`${msg.error.message} (${msg.error.code})`));
    else resolveCall(msg.result);
  });
  const send = (method, params = {}, sessionId) => new Promise((resolveCall, rejectCall) => {
    const id = nextId++;
    pending.set(id, { resolveCall, rejectCall });
    socket.send(JSON.stringify(sessionId ? { id, method, params, sessionId } : { id, method, params }));
  });
  return new Promise((resolveOpen, rejectOpen) => {
    socket.addEventListener("open", () => resolveOpen({ send, close: () => socket.close() }));
    socket.addEventListener("error", () => rejectOpen(new Error(`Cannot connect to ${wsUrl}`)));
  });
}

async function evaluate(cdp, sessionId, expression) {
  const result = await cdp.send("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true }, sessionId);
  if (result.exceptionDetails) throw new Error(`Page script failed: ${result.exceptionDetails.text} ${result.exceptionDetails.exception?.description ?? ""}`);
  return result.result.value;
}

// Shows one SVG at its own pixel size on a transparent page and captures it
async function renderPng(cdp, sessionId, svg, width, height) {
  await cdp.send("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: 1, mobile: false }, sessionId);
  await evaluate(cdp, sessionId, `document.body.innerHTML = ${JSON.stringify(svg)}; new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)))`);
  const shot = await cdp.send("Page.captureScreenshot", { format: "png", clip: { x: 0, y: 0, width, height, scale: 1 } }, sessionId);
  return Buffer.from(shot.data, "base64");
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const pluginRoot = resolve(args.plugin);
  const svgDir = join(pluginRoot, "Resources", "svg");
  const pngDir = join(pluginRoot, "Resources", "png");
  const geometry = JSON.parse(readFileSync(join(pluginRoot, "Build", "Brand", "brand-geometry.json"), "utf8"));
  const shuttleD = shuttlePathData(geometry.shuttle.profile);
  const wordD = readWordPath(join(svgDir, "wordmark-color.svg"));

  const write = (file, content) => { writeFileSync(file, content); console.log(`Wrote ${file.slice(pluginRoot.length + 1)}`); };

  for (const palette of MARK_PALETTES) write(join(svgDir, `mark-${palette}.svg`), markSvg(geometry, shuttleD, palette));
  for (const kind of Object.keys(geometry.lockups)) {
    for (const palette of LOCKUP_SVG_PALETTES) write(join(svgDir, `lockup-${kind}-${palette}.svg`), lockupSvg(geometry, shuttleD, wordD, kind, palette));
  }
  write(join(svgDir, "icon128.svg"), iconSvg(geometry, shuttleD));
  write(join(pluginRoot, "Resources", "Web", "brand-geometry.js"), webGeometry(geometry, shuttleD));

  const profileDir = mkdtempSync(join(tmpdir(), "shuttlegent-brand-"));
  const { child, wsUrl } = await launchChrome(args.chrome, profileDir);
  const cdp = await connect(wsUrl);
  try {
    const { targetId } = await cdp.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    await cdp.send("Emulation.setDefaultBackgroundColorOverride", { color: { r: 0, g: 0, b: 0, a: 0 } }, sessionId);
    await evaluate(cdp, sessionId, `document.head.innerHTML = '<style>html,body{margin:0;background:transparent}svg{display:block}</style>'`);

    const size = geometry.markPngSize;
    for (const palette of MARK_PALETTES) {
      write(join(pngDir, `mark-${palette}.png`), await renderPng(cdp, sessionId, markSvg(geometry, shuttleD, palette, size), size, size));
    }
    for (const [kind, palettes] of Object.entries(LOCKUP_PNG_PALETTES)) {
      const layout = geometry.lockups[kind];
      for (const palette of palettes) {
        const svg = lockupSvg(geometry, shuttleD, wordD, kind, palette, layout.pngScale);
        write(join(pngDir, `lockup-${kind}-${palette}.png`), await renderPng(cdp, sessionId, svg, layout.width * layout.pngScale, layout.height * layout.pngScale));
      }
    }
    const icon = await renderPng(cdp, sessionId, iconSvg(geometry, shuttleD), geometry.icon.size, geometry.icon.size);
    write(join(pngDir, "icon128.png"), icon);
    write(join(pluginRoot, "Resources", "Icon128.png"), icon);
  } finally {
    cdp.close();
    child.kill();
    try { rmSync(profileDir, { recursive: true, force: true }); } catch { /* Chrome may still hold the profile briefly */ }
  }
}

main().catch(error => { console.error(error.message); process.exit(1); });
