/*
 * Warp lines behind the empty conversation. While the large mark's shuttle flies, every line it crosses parts around
 * the shuttle's body (odd lines one way, even lines the other, like a shed opening), then springs back with a short,
 * damped tremble once the shuttle has gone through. The pointer parts the lines it passes in the same way.
 * Nothing moves while the page is hidden, when reduced motion is requested, or when the editor setting Animate Mark is off.
 */
(function () {
  'use strict';

  const NS = 'http://www.w3.org/2000/svg';
  const GEOMETRY = window.ShuttlegentBrandGeometry;
  // Line spacing and the offset of the first line, in CSS px from the layer's left edge
  const SPACING = 64;
  const OFFSET = 63.5;
  // Line colour is ivory; resting lines are barely visible, parted lines light up
  const RGB = '232,223,207';
  const REST_ALPHA = 0.04;
  const LIT_ALPHA = 0.2;
  // Sideways parting at the shuttle's thickest station, as a share of its half thickness on screen
  const PART_SHARE = 0.55;
  // The pointer pushes lines within this many px aside, by at most POINTER_PUSH px, over POINTER_REACH px of the line
  const POINTER_RADIUS = 26;
  const POINTER_PUSH = 7;
  const POINTER_REACH = 44;
  // Spring of each line, per second
  const STIFFNESS = 320;
  const DAMPING = 7;
  // A line counts as at rest within these bounds, in px and px per second
  const REST_OFFSET = 0.05;
  const REST_VELOCITY = 0.5;

  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  let host = null;
  let layer = null;
  let lines = [];
  let follow = null;
  let pointer = null;
  let enabled = true;
  let frameRequest = 0;
  let lastTime = 0;

  function canAnimate() {
    return enabled && !reducedMotion.matches && !document.hidden;
  }

  function build() {
    const width = layer.clientWidth;
    layer.replaceChildren();
    lines = [];
    for (let x = OFFSET, index = 0; x < width; x += SPACING, ++index) {
      const path = document.createElementNS(NS, 'path');
      layer.appendChild(path);
      lines.push({ x: x, path: path, sign: index % 2 ? 1 : -1, offset: 0, velocity: 0, target: 0, centre: 0, reach: 0 });
    }
    lines.forEach(draw);
  }

  function draw(line) {
    const height = layer.clientHeight;
    const x = line.x;
    const lit = Math.min(1, Math.abs(line.offset) / 6);
    line.path.setAttribute('stroke', 'rgba(' + RGB + ',' + (REST_ALPHA + (LIT_ALPHA - REST_ALPHA) * lit).toFixed(3) + ')');
    if (Math.abs(line.offset) < REST_OFFSET) {
      line.path.setAttribute('d', 'M ' + x + ' 0 V ' + height);
      return;
    }
    const top = line.centre - line.reach;
    const bottom = line.centre + line.reach;
    const bent = x + line.offset;
    const half = line.reach * 0.55;
    line.path.setAttribute('d', 'M ' + x + ' 0 V ' + top.toFixed(1)
      + ' C ' + x + ' ' + (top + half).toFixed(1) + ' ' + bent.toFixed(1) + ' ' + (line.centre - half).toFixed(1) + ' ' + bent.toFixed(1) + ' ' + line.centre.toFixed(1)
      + ' C ' + bent.toFixed(1) + ' ' + (line.centre + half).toFixed(1) + ' ' + x + ' ' + (bottom - half).toFixed(1) + ' ' + x + ' ' + bottom.toFixed(1)
      + ' V ' + height);
  }

  // Half thickness of the shuttle at a share of its half length from the centre, from the master profile
  function thicknessAt(share) {
    const profile = GEOMETRY.shuttle.profile;
    const u = Math.min(1, Math.abs(share)) * GEOMETRY.shuttle.halfLength;
    for (let index = 1; index < profile.length; ++index) {
      if (u <= profile[index][0]) {
        const [u0, h0] = profile[index - 1];
        const [u1, h1] = profile[index];
        return (h0 + (h1 - h0) * (u - u0) / (u1 - u0)) / GEOMETRY.shuttle.halfThickness;
      }
    }
    return 0;
  }

  // Lines near the pointer lean away from it, the line under it towards the side the pointer came from
  function steerByPointer() {
    if (!pointer) return;
    for (const line of lines) {
      const distance = line.x - pointer.x;
      if (Math.abs(distance) >= POINTER_RADIUS) continue;
      const side = Math.abs(distance) > 1 ? Math.sign(distance) : pointer.direction;
      line.target = side * POINTER_PUSH * (1 - Math.abs(distance) / POINTER_RADIUS);
      line.centre = pointer.y;
      line.reach = POINTER_REACH;
    }
  }

  // Pushes each line the shuttle crosses towards the shuttle's body; it wins over the pointer
  function steerByShuttle() {
    if (!follow || performance.now() > follow.until) return;
    // A shuttle still fading in (the weave starts it transparent, off to the lower left) parts the lines only as much as it shows
    const presence = Number(getComputedStyle(follow.node.parentNode).opacity);
    if (presence < 0.05) return;
    const ctm = follow.node.getScreenCTM();
    if (!ctm) return;
    const box = layer.getBoundingClientRect();
    const at = (x, y) => [ctm.a * x + ctm.c * y + ctm.e - box.left, ctm.b * x + ctm.d * y + ctm.f - box.top];
    const [cx, cy] = at(0, 0);
    const [tx, ty] = at(GEOMETRY.shuttle.halfLength, 0);
    const [sx, sy] = at(0, GEOMETRY.shuttle.halfThickness);
    const axisX = tx - cx;
    const axisY = ty - cy;
    const halfThickness = Math.hypot(sx - cx, sy - cy);
    if (Math.abs(axisX) < 1) return;
    for (const line of lines) {
      const share = (line.x - cx) / axisX;
      if (Math.abs(share) > 1) continue;
      const thickness = thicknessAt(share);
      line.target = line.sign * halfThickness * PART_SHARE * thickness * presence;
      line.centre = cy + share * axisY;
      line.reach = halfThickness * (thickness + 0.6) + 18;
    }
  }

  function tick(now) {
    frameRequest = 0;
    if (!canAnimate()) {
      rest();
      return;
    }
    const step = Math.min(0.033, (now - lastTime) / 1000);
    lastTime = now;
    for (const line of lines) line.target = 0;
    steerByPointer();
    steerByShuttle();
    let moving = follow !== null && now < follow.until;
    for (const line of lines) {
      const acceleration = STIFFNESS * (line.target - line.offset) - 2 * DAMPING * line.velocity;
      line.velocity += acceleration * step;
      line.offset += line.velocity * step;
      if (Math.abs(line.offset - line.target) > REST_OFFSET || Math.abs(line.velocity) > REST_VELOCITY) moving = true;
      else {
        line.offset = line.target;
        line.velocity = 0;
      }
      draw(line);
    }
    if (moving) frameRequest = window.requestAnimationFrame(tick);
    else follow = null;
  }

  function wake() {
    if (frameRequest || !layer || !canAnimate()) return;
    // The layer may have been measured while hidden (display none, or a hidden panel tab)
    if (!lines.length) build();
    lastTime = performance.now();
    frameRequest = window.requestAnimationFrame(tick);
  }

  // Straightens every line at once
  function rest() {
    window.cancelAnimationFrame(frameRequest);
    frameRequest = 0;
    follow = null;
    pointer = null;
    for (const line of lines) {
      line.offset = line.velocity = line.target = 0;
      draw(line);
    }
  }

  function onPointerMove(event) {
    if (!host.classList.contains('is-empty')) return;
    const box = layer.getBoundingClientRect();
    const x = event.clientX - box.left;
    const direction = pointer && x !== pointer.x ? Math.sign(pointer.x - x) : pointer ? pointer.direction : 1;
    pointer = { x: x, y: event.clientY - box.top, direction: direction };
    wake();
  }

  function onPointerLeave() {
    pointer = null;
    wake();
  }

  /** Draws the warp lines into a layer at the start of element; they show only while element has class is-empty. */
  function attach(element) {
    host = element;
    layer = document.createElementNS(NS, 'svg');
    layer.classList.add('warp');
    layer.setAttribute('aria-hidden', 'true');
    host.prepend(layer);
    host.addEventListener('pointermove', onPointerMove);
    host.addEventListener('pointerleave', onPointerLeave);
    new ResizeObserver(build).observe(layer);
  }

  /** Lets the lines react to a shuttle node of the brand mark for duration ms. */
  function track(node, duration) {
    follow = { node: node, until: performance.now() + duration };
    wake();
  }

  function setEnabled(value) {
    enabled = value;
    if (!canAnimate()) rest();
  }

  document.addEventListener('visibilitychange', () => {
    if (!canAnimate()) rest();
  });
  reducedMotion.addEventListener('change', () => {
    if (!canAnimate()) rest();
  });

  window.ShuttlegentWarp = { attach: attach, track: track, setEnabled: setEnabled };
})();
