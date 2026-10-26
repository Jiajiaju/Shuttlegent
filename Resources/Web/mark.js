/*
 * Brand mark animation (VI version D "经纬织成"), played from the sprite sheets in Resources/anim/red-white.
 * The editor sends the sheet manifest (mark-anim.json) once the page is ready; until then marks show the static image.
 * Large marks rest on the last intro frame, small marks on the last done frame. Playback stops while the page is hidden,
 * when reduced motion is requested, and when the editor setting Animate Mark is off.
 */
(function () {
  'use strict';

  // Sheets are exported at 2x; one sheet pixel is half a CSS pixel
  const SHEET_SCALE = 0.5;

  // Rest scene of each tier: its last frame is pixel-identical to the master mark
  const REST = { l: 'intro', s: 'done' };

  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  const marks = new Set();
  let sheets = null;
  let fps = 0;
  let enabled = true;
  let frameRequest = 0;
  let lastDraw = 0;

  function sheetOf(tier, motion) {
    return sheets ? sheets[tier + '-' + motion] || null : null;
  }

  function canAnimate() {
    return enabled && !reducedMotion.matches && !document.hidden && sheets !== null;
  }

  function drawFrame(mark, sheet, frame) {
    const cellWidth = (sheet.frameWidth + sheets.padding) * SHEET_SCALE;
    const cellHeight = (sheet.frameHeight + sheets.padding) * SHEET_SCALE;
    mark.element.style.backgroundImage = 'url("' + sheet.url + '")';
    mark.element.style.backgroundSize = sheet.width * SHEET_SCALE + 'px ' + sheet.height * SHEET_SCALE + 'px';
    mark.element.style.backgroundPosition = -(frame % sheet.columns) * cellWidth + 'px ' + -Math.floor(frame / sheet.columns) * cellHeight + 'px';
  }

  /** Draws every mark; returns true while any of them still moves. */
  function draw(now) {
    let moving = false;
    for (const mark of marks) {
      let sheet = mark.motion ? sheetOf(mark.tier, mark.motion) : null;
      let frame = 0;
      if (sheet && canAnimate()) {
        const elapsed = Math.max(0, Math.floor((now - mark.startedAt) * fps / 1000));
        frame = sheet.loop ? elapsed % sheet.frameCount : Math.min(elapsed, sheet.frameCount - 1);
        if (!sheet.loop && elapsed >= sheet.frameCount - 1) mark.motion = null;
        else moving = true;
      } else {
        mark.motion = sheet && sheet.loop ? mark.motion : null;
        sheet = null;
      }

      if (!sheet) {
        sheet = sheetOf(mark.tier, REST[mark.tier]);
        if (!sheet) continue;
        frame = sheet.frameCount - 1;
      }
      drawFrame(mark, sheet, frame);
    }
    return moving;
  }

  function tick(now) {
    frameRequest = 0;
    if (!canAnimate()) {
      draw(now);
      return;
    }
    if (now - lastDraw >= 1000 / fps) {
      lastDraw = now;
      if (!draw(now)) return;
    }
    frameRequest = window.requestAnimationFrame(tick);
  }

  function wake() {
    window.cancelAnimationFrame(frameRequest);
    frameRequest = 0;
    draw(performance.now());
    if (canAnimate()) frameRequest = window.requestAnimationFrame(tick);
  }

  /** Registers an element as a mark of tier 'l' (large) or 's' (small). */
  function attach(element, tier) {
    const mark = { element: element, tier: tier, motion: null, startedAt: 0 };
    marks.add(mark);
    element.classList.add('mark', 'mark-' + tier);
    element.addEventListener('mouseenter', () => {
      if (!mark.motion) play(mark, 'hover');
    });
    wake();
    return mark;
  }

  function detach(mark) {
    marks.delete(mark);
  }

  function play(mark, motion) {
    mark.motion = sheetOf(mark.tier, motion) ? motion : null;
    mark.startedAt = performance.now();
    wake();
  }

  function rest(mark) {
    mark.motion = null;
    wake();
  }

  /** Manifest of Resources/anim/<palette>/mark-anim.json, and the sheets' directory relative to the page. */
  function setSheets(manifest, directory) {
    sheets = { padding: manifest.padding || 0 };
    fps = manifest.fps || 30;
    for (const sheet of manifest.sheets || []) {
      sheets[sheet.tier + '-' + sheet.motion] = Object.assign({ url: directory + sheet.file }, sheet);
    }
    document.documentElement.classList.add('mark-sheets');
    wake();
  }

  function setEnabled(value) {
    if (enabled === value) return;
    enabled = value;
    wake();
  }

  document.addEventListener('visibilitychange', wake);
  reducedMotion.addEventListener('change', wake);

  window.ShuttlegentMark = { attach: attach, detach: detach, play: play, rest: rest, setSheets: setSheets, setEnabled: setEnabled };
})();
