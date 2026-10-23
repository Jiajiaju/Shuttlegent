// Exports the Shuttlegent mark animation (version D, "woven picks") as sprite sheets for Slate.
// Renders Build/MarkAnim/mark-anim.html in headless Chrome over the DevTools protocol and screenshots
// one transparent sheet per tier and motion, plus a manifest describing the frame grid.
//
// Usage: node ExportMarkAnim.mjs --chrome <chrome.exe> --plugin <plugin root> [--palette <name>]
// Colours come from Resources/svg/mark-<palette>.svg. The default palette "reverse" writes to Resources/anim,
// any other palette to Resources/anim/<palette>.

import { spawn } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync, mkdirSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const FPS = 30;
// Transparent gutter between cells so bilinear sampling never bleeds into the neighbour frame
const PADDING = 2;
// Upper bound for a sheet edge
const MAX_SHEET_EDGE = 2048;
// Palette the panel uses on the editor's dark UI; its sheets live directly in Resources/anim
const DEFAULT_PALETTE = "reverse";
// Mark view box is 344 x 224
const MARK_ASPECT = 224 / 344;

// Each tier only carries the motions it plays in the panel
const TIERS = [
  { name: "l", frameWidth: 256, picks: 8, motions: ["intro", "hover"] },
  { name: "s", frameWidth: 64, picks: 4, motions: ["working", "hover", "done"] },
];

function parseArgs(argv) {
  const args = {};
  for (let i = 0; i < argv.length; i += 2) args[argv[i].replace(/^--/, "")] = argv[i + 1];
  if (!args.chrome || !args.plugin) throw new Error("Usage: node ExportMarkAnim.mjs --chrome <chrome.exe> --plugin <plugin root> [--palette <name>]");
  if (!args.palette) args.palette = DEFAULT_PALETTE;
  if (!/^[a-z0-9-]+$/.test(args.palette)) throw new Error(`Invalid palette name "${args.palette}"`);
  return args;
}

// Warp colour is the fill of the groups holding the bars, shuttle colour the fill of the even-odd shuttle path
function readPaletteColors(svgFile) {
  const text = readFileSync(svgFile, "utf8");
  const warp = text.match(/<g fill="(#[0-9A-Fa-f]{6})"/);
  const shuttle = text.match(/fill="(#[0-9A-Fa-f]{6})" fill-rule="evenodd"/) ?? text.match(/fill-rule="evenodd" fill="(#[0-9A-Fa-f]{6})"/);
  if (!warp || !shuttle) throw new Error(`Cannot read warp and shuttle colours from ${svgFile}`);
  return { ink: warp[1], red: shuttle[1] };
}

function readMarkPaths(svgFile) {
  const text = readFileSync(svgFile, "utf8");
  const paths = [...text.matchAll(/<path d="([^"]*)"([^>]*)\/>/g)].map(m => ({ d: m[1], attrs: m[2] }));
  const stub = paths.find(p => !p.attrs.includes("rotate") && /^M -64 75/.test(p.d));
  const bar = paths.find(p => !p.attrs.includes("rotate") && /^M -48 -92/.test(p.d));
  const shuttle = paths.find(p => p.attrs.includes("evenodd"));
  if (!stub || !bar || !shuttle) throw new Error(`Unexpected mark structure in ${svgFile}`);
  return { stub: stub.d, bar: bar.d, shuttle: shuttle.d };
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
  const waiters = [];
  socket.addEventListener("message", event => {
    const msg = JSON.parse(event.data);
    if (msg.id && pending.has(msg.id)) {
      const { resolveCall, rejectCall } = pending.get(msg.id);
      pending.delete(msg.id);
      if (msg.error) rejectCall(new Error(`${msg.error.message} (${msg.error.code})`));
      else resolveCall(msg.result);
    } else if (msg.method) {
      for (const w of waiters.slice()) if (w.method === msg.method) { waiters.splice(waiters.indexOf(w), 1); w.resolveEvent(msg.params); }
    }
  });
  const send = (method, params = {}, sessionId) => new Promise((resolveCall, rejectCall) => {
    const id = nextId++;
    pending.set(id, { resolveCall, rejectCall });
    socket.send(JSON.stringify(sessionId ? { id, method, params, sessionId } : { id, method, params }));
  });
  const once = method => new Promise(resolveEvent => waiters.push({ method, resolveEvent }));
  return new Promise((resolveOpen, rejectOpen) => {
    socket.addEventListener("open", () => resolveOpen({ send, once, close: () => socket.close() }));
    socket.addEventListener("error", () => rejectOpen(new Error(`Cannot connect to ${wsUrl}`)));
  });
}

async function evaluate(cdp, sessionId, expression) {
  const result = await cdp.send("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true }, sessionId);
  if (result.exceptionDetails) throw new Error(`Page script failed: ${result.exceptionDetails.text} ${result.exceptionDetails.exception?.description ?? ""}`);
  return result.result.value;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const pluginRoot = resolve(args.plugin);
  const pagePath = join(pluginRoot, "Build", "MarkAnim", "mark-anim.html");
  const animRoot = join(pluginRoot, "Resources", "anim");
  const outDir = args.palette === DEFAULT_PALETTE ? animRoot : join(animRoot, args.palette);
  const paths = readMarkPaths(join(pluginRoot, "Resources", "svg", "mark-color.svg"));
  const colors = readPaletteColors(join(pluginRoot, "Resources", "svg", `mark-${args.palette}.svg`));
  console.log(`Palette ${args.palette}: warp ${colors.ink}, shuttle ${colors.red}`);
  mkdirSync(outDir, { recursive: true });

  const profileDir = mkdtempSync(join(tmpdir(), "shuttlegent-markanim-"));
  const { child, wsUrl } = await launchChrome(args.chrome, profileDir);
  const cdp = await connect(wsUrl);
  try {
    const { targetId } = await cdp.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    await cdp.send("Page.enable", {}, sessionId);
    const loaded = cdp.once("Page.loadEventFired");
    await cdp.send("Page.navigate", { url: pathToFileURL(pagePath).href }, sessionId);
    await loaded;
    await cdp.send("Emulation.setDefaultBackgroundColorOverride", { color: { r: 0, g: 0, b: 0, a: 0 } }, sessionId);
    await evaluate(cdp, sessionId, `setMarkPaths(${JSON.stringify(paths)})`);

    const manifest = { version: "D", palette: args.palette, fps: FPS, padding: PADDING, sheets: [] };
    for (const tier of TIERS) {
      const frameHeight = Math.round(tier.frameWidth * MARK_ASPECT);
      const columns = Math.floor((MAX_SHEET_EDGE + PADDING) / (tier.frameWidth + PADDING));
      for (const motion of tier.motions) {
        const spec = { motion, fps: FPS, frameWidth: tier.frameWidth, frameHeight, picks: tier.picks, columns, padding: PADDING, ink: colors.ink, red: colors.red };
        const sheet = await evaluate(cdp, sessionId, `renderSheet(${JSON.stringify(spec)})`);
        if (sheet.height > MAX_SHEET_EDGE) throw new Error(`Sheet ${tier.name}/${motion} is ${sheet.height} px tall, over ${MAX_SHEET_EDGE}`);
        await cdp.send("Emulation.setDeviceMetricsOverride", { width: sheet.width, height: sheet.height, deviceScaleFactor: 1, mobile: false }, sessionId);
        // Let the layout settle at the new viewport size before capturing
        await evaluate(cdp, sessionId, "new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)))");
        const shot = await cdp.send("Page.captureScreenshot", { format: "png", clip: { x: 0, y: 0, width: sheet.width, height: sheet.height, scale: 1 }, captureBeyondViewport: true }, sessionId);
        const file = `mark_${tier.name}_${motion}.png`;
        writeFileSync(join(outDir, file), Buffer.from(shot.data, "base64"));
        const info = await evaluate(cdp, sessionId, `motionInfo(${JSON.stringify(motion)})`);
        manifest.sheets.push({ tier: tier.name, motion, file, frameWidth: tier.frameWidth, frameHeight, frameCount: sheet.count, columns, rows: sheet.rows, width: sheet.width, height: sheet.height, picks: tier.picks, loop: info.loop, durationMs: info.durationMs });
        console.log(`Exported ${file}: ${sheet.count} frames, ${sheet.width}x${sheet.height}`);
      }
    }
    writeFileSync(join(outDir, "mark-anim.json"), JSON.stringify(manifest, null, 2) + "\n");
    console.log(`Wrote manifest to ${join(outDir, "mark-anim.json")}`);
  } finally {
    cdp.close();
    child.kill();
    try { rmSync(profileDir, { recursive: true, force: true }); } catch { /* Chrome may still hold the profile briefly */ }
  }
}

main().catch(error => { console.error(error.message); process.exit(1); });
