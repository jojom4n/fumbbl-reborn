// =============================================================================
// verify-dashboard.mjs — End-to-end verification of the react-grid-layout v2
// dashboard (real Chromium, real built app, faked FUMBBL auth endpoints).
//
// Verifies:
//   1. All 7 panels render as grid items
//   2. Resize handles are real hit targets (20x20, absolute, se-resize cursor)
//   3. The grid fills the whole window height (no dead space at the bottom)
//   4. Drag & drop WORKS (synthetic mousedown/mousemove/mouseup on a title bar
//      actually moves the panel — this catches the swallowed onMouseDown bug)
//   4b. Resize WORKS (synthetic mouse events on the SE handle) and STOPS at
//       the neighbouring panel (never pushes panels off-screen)
//   4a. Drag-onto-neighbour: the covered panel swaps into the vacated slot,
//       everything stays inside the 12x16 grid (no overlap, nothing off-screen)
//   4b2. Drag past the grid edge: panels are clamped on-screen (bounded)
//   4c. Resize shrink+grow: the SE handle stops at neighbours/grid edge
//   4d. Corrupted persisted layout (panel below the grid) is sanitized on load
//   5. The new layout is persisted (localStorage) and survives a reload
//   6. Reset restores the default layout and clears persistence
//
// Usage:  npm run build && node scripts/verify-dashboard.mjs
// (Starts a static server on dist/ itself, on port 4199.)
// =============================================================================

import { createRequire } from 'node:module';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const DIST = path.join(ROOT, 'dist');
const PORT = 4199;
const BASE = `http://127.0.0.1:${PORT}`;

if (!fs.existsSync(path.join(DIST, 'index.html'))) {
  console.error('dist/index.html not found — run `npm run build` first.');
  process.exit(1);
}

// ---------------------------------------------------------------------------
// Locate playwright in the npx cache (avoids adding a project dependency)
// ---------------------------------------------------------------------------
function findPlaywright() {
  const npxRoot = path.join(os.homedir(), '.npm', '_npx');
  if (fs.existsSync(npxRoot)) {
    for (const dir of fs.readdirSync(npxRoot)) {
      const p = path.join(npxRoot, dir, 'node_modules', 'playwright', 'index.mjs');
      if (fs.existsSync(p)) return p;
    }
  }
  const require = createRequire(import.meta.url);
  return require.resolve('playwright');
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
let failures = 0;
let passes = 0;
function check(name, cond, detail = '') {
  if (cond) {
    passes++;
    console.log(`  ✅ ${name}`);
  } else {
    failures++;
    console.error(`  ❌ ${name}${detail ? ` — ${detail}` : ''}`);
  }
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Dashboard grid dimensions (must match layoutConfig.ts)
const COLS = 12;
const ROWS = 16;
const DEFAULT_COORDS = {
  header: { x: 0, y: 0, w: 12, h: 2 },
  team1: { x: 0, y: 2, w: 2, h: 10 },
  field: { x: 2, y: 2, w: 8, h: 10 },
  team2: { x: 10, y: 2, w: 2, h: 10 },
  'player-info': { x: 0, y: 12, w: 3, h: 4 },
  'dice-log': { x: 3, y: 12, w: 5, h: 4 },
  chat: { x: 8, y: 12, w: 4, h: 4 },
};

// Read the persisted layout from localStorage (debounced save, 500ms)
async function readLayout(page) {
  return page.evaluate(() => {
    const raw = localStorage.getItem('layout');
    return raw ? JSON.parse(raw) : null;
  });
}

// Every panel inside the COLS x ROWS grid (the "never off-screen" invariant)
function inBounds(layout) {
  return (
    !!layout &&
    Array.isArray(layout) &&
    layout.length === 7 &&
    layout.every(
      (it) => it.x >= 0 && it.y >= 0 && it.x + it.w <= COLS && it.y + it.h <= ROWS
    )
  );
}

// No two panels overlap
function noOverlap(layout) {
  return (
    !!layout &&
    !layout.some(
      (a, i) =>
        layout.some(
          (b, j) =>
            i < j &&
            a.x < b.x + b.w &&
            b.x < a.x + a.w &&
            a.y < b.y + b.h &&
            b.y < a.y + a.h
        )
    )
  );
}

function differsFromDefault(layout) {
  return (
    !!layout &&
    layout.some(
      (it) =>
        !DEFAULT_COORDS[it.i] ||
        it.x !== DEFAULT_COORDS[it.i].x ||
        it.y !== DEFAULT_COORDS[it.i].y ||
        it.w !== DEFAULT_COORDS[it.i].w ||
        it.h !== DEFAULT_COORDS[it.i].h
    )
  );
}

// Pixel-level check: no panel rendered outside the grid container
async function anyPanelOffScreen(page) {
  return page.evaluate(() => {
    const container = document.querySelector('div.react-grid-layout');
    if (!container) return true;
    const cr = container.getBoundingClientRect();
    return [...document.querySelectorAll('div.react-grid-item')].some((el) => {
      const r = el.getBoundingClientRect();
      return (
        r.bottom > cr.bottom + 1 ||
        r.right > cr.right + 1 ||
        r.top < cr.top - 1 ||
        r.left < cr.left - 1
      );
    });
  });
}

// Read a grid item's position by its title text (case-insensitive)
async function getPanelRect(page, titleText) {
  return page.evaluate((t) => {
    const items = [...document.querySelectorAll('div.react-grid-item')];
    const item = items.find((el) => {
      const bar = el.querySelector('.panel-title');
      return bar && bar.textContent.toUpperCase().includes(t.toUpperCase());
    });
    if (!item) return null;
    const r = item.getBoundingClientRect();
    return {
      left: r.left,
      top: r.top,
      width: r.width,
      height: r.height,
      transform: getComputedStyle(item).transform,
    };
  }, titleText);
}

// Dispatch a full mouse drag (mousedown on the target, stepped mousemove +
// mouseup on document) — exactly what react-draggable listens for.
//
// IMPORTANT: every event is dispatched in its OWN task (separate
// page.evaluate). React flushes the setDragging(true) update only after the
// mousedown dispatch finishes; mousemoves dispatched in the same synchronous
// task would hit stale onDrag/onDragStop closures (dragging=false) and the
// whole drag would be silently ignored. Real mice also deliver events in
// separate tasks, so this matches production behavior.
async function mouseDrag(page, { root, title, targetSel }, dx, dy) {
  const { x, y } = await page.evaluate(
    ({ root, title, targetSel }) => {
      const items = [...document.querySelectorAll(root)];
      const item = items.find((el) => {
        const bar = el.querySelector('.panel-title');
        return bar && bar.textContent.toUpperCase().includes(title.toUpperCase());
      });
      if (!item) throw new Error('panel not found: ' + title);
      const target = item.querySelector(targetSel);
      if (!target) throw new Error('drag target not found: ' + targetSel);
      const r = target.getBoundingClientRect();
      return { x: r.left + r.width / 2, y: r.top + r.height / 2 };
    },
    { root, title, targetSel }
  );

  await page.evaluate(
    ({ x, y, root, title, targetSel }) => {
      const items = [...document.querySelectorAll(root)];
      const item = items.find((el) => {
        const bar = el.querySelector('.panel-title');
        return bar && bar.textContent.toUpperCase().includes(title.toUpperCase());
      });
      const target = item.querySelector(targetSel);
      target.dispatchEvent(
        new MouseEvent('mousedown', {
          bubbles: true,
          cancelable: true,
          view: window,
          button: 0,
          clientX: x,
          clientY: y,
        })
      );
    },
    { x, y, root, title, targetSel }
  );
  await sleep(50);

  const steps = 6;
  for (let i = 1; i <= steps; i++) {
    await page.evaluate(
      ({ cx, cy }) => {
        document.dispatchEvent(
          new MouseEvent('mousemove', {
            bubbles: true,
            cancelable: true,
            view: window,
            button: 0,
            clientX: cx,
            clientY: cy,
          })
        );
      },
      { cx: x + (dx * i) / steps, cy: y + (dy * i) / steps }
    );
    await sleep(25);
  }

  await page.evaluate(
    ({ cx, cy }) => {
      document.dispatchEvent(
        new MouseEvent('mouseup', {
          bubbles: true,
          cancelable: true,
          view: window,
          button: 0,
          clientX: cx,
          clientY: cy,
        })
      );
    },
    { cx: x + dx, cy: y + dy }
  );
  await sleep(300); // let React commit the post-drag layout
}
// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
async function main() {
  console.log('⬆️  Starting static server for dist/ on port', PORT);
  const server = spawn(
    'python3',
    ['-m', 'http.server', String(PORT), '--directory', DIST, '--bind', '127.0.0.1'],
    { stdio: 'ignore', detached: true }
  );
  await sleep(700);

  const pw = await import(findPlaywright());
  const { chromium } = pw;

  let browser;
  try {
    browser = await chromium.launch({ headless: true });
  } catch (e) {
    console.log('  (default browser not found, falling back to /usr/bin/chromium)');
    browser = await chromium.launch({ headless: true, executablePath: '/usr/bin/chromium' });
  }

  const page = await browser.newPage({ viewport: { width: 1740, height: 930 } });

  const pageErrors = [];
  const consoleErrors = [];
  page.on('pageerror', (err) => pageErrors.push(String(err)));
  page.on('console', (msg) => {
    if (msg.type() === 'error') consoleErrors.push(msg.text());
  });

  // Fake FUMBBL auth so the app reaches the dashboard without real credentials
  await page.route('**/fumbbl.com/**', async (route) => {
    const url = route.request().url();
    let body = '{}';
    if (url.includes('/api/oauth/token')) {
      body = JSON.stringify({ access_token: 'fake-access-token', token_type: 'Bearer', expires_in: 3600 });
    } else if (url.includes('/api/auth/getToken')) {
      // The API returns a JSON-ENCODED string (the app does JSON.parse(text))
      body = '"fake-session-token"';
    } else if (url.includes('/api/auth/verify')) {
      body = JSON.stringify({ valid: true });
    } else if (url.includes('/api/oauth/identity')) {
      body = JSON.stringify({ username: 'SmokeTester' });
    } else if (url.includes('/api/user/')) {
      body = '[]';
    }
    await route.fulfill({ status: 200, contentType: 'application/json', body });
  });

  // Seed saved credentials (LoginScreen pre-fills and we click "Connetti")
  await page.addInitScript(() => {
    localStorage.setItem(
      'fumbbl_credentials',
      JSON.stringify({ clientId: 'smoke', clientSecret: 'smoke', username: 'SmokeTester' })
    );
  });

  console.log('\n1️⃣  Loading app and logging in...');
  await page.goto(BASE, { waitUntil: 'domcontentloaded' });
  await page.evaluate(() => localStorage.removeItem('layout')); // start from default layout
  await page.waitForSelector('button:has-text("Connetti")', { timeout: 15000 });
  await page.click('button:has-text("Connetti")');
  await page.waitForSelector('div.react-grid-item', { timeout: 20000 });
  await sleep(400); // ResizeObserver height measurement + rowHeight re-render

  console.log('\n2️⃣  Structure checks');
  const itemCount = await page.locator('div.react-grid-item').count();
  check('7 grid panels rendered', itemCount === 7, `got ${itemCount}`);

  // Resize handle must be a real, clickable hit target (CSS imported)
  const handleInfo = await page.evaluate(() => {
    const h = document.querySelector('div.react-grid-item .react-resizable-handle-se');
    if (!h) return null;
    const cs = getComputedStyle(h);
    return { width: cs.width, height: cs.height, position: cs.position, cursor: cs.cursor };
  });
  check(
    'resize handle is a 20x20 absolute hit target',
    !!handleInfo &&
      handleInfo.width === '20px' &&
      handleInfo.height === '20px' &&
      handleInfo.position === 'absolute' &&
      handleInfo.cursor === 'se-resize',
    JSON.stringify(handleInfo)
  );

  // Screen filling: bottom-row panels must reach the container bottom
  const fill = await page.evaluate(() => {
    const container = document.querySelector('div.react-grid-layout');
    const cr = container.getBoundingClientRect();
    const items = [...document.querySelectorAll('div.react-grid-item')].map((el) => el.getBoundingClientRect());
    const maxBottom = Math.max(...items.map((r) => r.bottom));
    return { containerH: cr.height, gapBelow: cr.bottom - maxBottom, viewportH: window.innerHeight };
  });
  check(
    'grid fills window height (no dead space at the bottom)',
    fill.containerH >= fill.viewportH - 4 && fill.gapBelow >= -1 && fill.gapBelow <= 12,
    JSON.stringify(fill)
  );

  const initialDice = await getPanelRect(page, 'Dice Log');
  check('initial Dice Log rect captured', !!initialDice);
  await page.screenshot({ path: '/tmp/verify-dashboard-1-initial.png' });

  console.log('\n3️⃣  Drag & drop (real mouse events on the title bar)');
  await mouseDrag(page, { root: 'div.react-grid-item', title: 'Dice Log', targetSel: '.panel-title' }, 180, 0);
  const draggedDice = await getPanelRect(page, 'Dice Log');
  const dragDx = draggedDice ? draggedDice.left - initialDice.left : NaN;
  check('panel actually moved right by ~180px', Math.abs(dragDx) >= 100, `dx=${dragDx.toFixed(1)}px`);

  console.log('\n4️⃣  Resize (real mouse events on the SE handle)');
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Dice Log', targetSel: '.react-resizable-handle-se' },
    150,
    60
  );
  const resizedDice = await getPanelRect(page, 'Dice Log');
  check('resize interaction ran without errors (rect captured)', !!resizedDice);
  // In the default tiling the dice-log's right neighbour (chat) sits inside
  // the requested growth: the neighbour-stop constraint must clamp the resize
  // at chat's left edge instead of pushing chat off-screen.
  await sleep(900); // debounced save
  const afterResize = await readLayout(page);
  const diceA = afterResize.find((i) => i.i === 'dice-log');
  const chatA = afterResize.find((i) => i.i === 'chat');
  check(
    'resize stopped at the neighbouring panel (dice-log right edge <= chat left edge)',
    !!diceA && !!chatA && diceA.x + diceA.w <= chatA.x,
    `dice=(${diceA?.x},${diceA?.w}) chat=(${chatA?.x},${chatA?.w})`
  );
  console.log('     state:', JSON.stringify(afterResize.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('resize did not push any panel off the grid', inBounds(afterResize));
  check('no panel rendered outside the container (after resize)', !(await anyPanelOffScreen(page)));
  await page.screenshot({ path: '/tmp/verify-dashboard-2-after-drag-resize.png' });

  console.log('\n4a) Collision: drag Field onto Team 2 (swap, stay in-grid)');
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Field', targetSel: '.panel-title' },
    600,
    0
  );
  await sleep(900);
  const afterSwap = await readLayout(page);
  const fieldA = afterSwap.find((i) => i.i === 'field');
  const team2A = afterSwap.find((i) => i.i === 'team2');
  check('dragged field moved right', !!fieldA && fieldA.x > 2, `field.x=${fieldA?.x}`);
  check(
    'covered panel (team2) moved into the vacated slot',
    !!team2A && team2A.x === 2 && team2A.y === 2,
    JSON.stringify(team2A)
  );
  console.log('     state:', JSON.stringify(afterSwap.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('after swap: all panels inside the 12x16 grid', inBounds(afterSwap));
  check('after swap: no two panels overlap', noOverlap(afterSwap));
  check('after swap: no panel rendered outside the container', !(await anyPanelOffScreen(page)));

  console.log('\n4b) Bounded: drag header past the bottom edge (stays on-screen)');
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Match', targetSel: '.panel-title' },
    0,
    800
  );
  await sleep(900);
  const afterEdge = await readLayout(page);
  const headerA = afterEdge.find((i) => i.i === 'header');
  check(
    'header clamped to the grid (y + h <= 16)',
    !!headerA && headerA.y + headerA.h <= ROWS,
    JSON.stringify(headerA)
  );
  console.log('     state:', JSON.stringify(afterEdge.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('after edge-drag: all panels inside the 12x16 grid', inBounds(afterEdge));
  check('after edge-drag: no panel rendered outside the container', !(await anyPanelOffScreen(page)));
  await page.screenshot({ path: '/tmp/verify-dashboard-3-collision.png' });

  console.log('\n4c) Neighbour stop: shrink field, then grow it back');
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Field', targetSel: '.react-resizable-handle-se' },
    -150,
    -60
  );
  await sleep(900);
  const shrunk = await readLayout(page);
  const fieldS = shrunk.find((i) => i.i === 'field');
  console.log('     state:', JSON.stringify(shrunk.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check(
    'field actually shrank (w < 8 or h < 10)',
    !!fieldS && (fieldS.w < 8 || fieldS.h < 10),
    JSON.stringify(fieldS)
  );
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Field', targetSel: '.react-resizable-handle-se' },
    400,
    400
  );
  await sleep(900);
  const grown = await readLayout(page);
  const fieldG = grown.find((i) => i.i === 'field');
  // NOTE: after 4b the header sits at the bottom, so the compactor pulled the
  // bottom row up into the freed top rows — the field stops at the
  // (pulled-up) neighbour's edge: exactly 8 wide (grid edge) and 9 tall
  // (bottom row now at y10).
  check(
    'field grew back but stopped exactly at neighbours (8 wide x 9 tall)',
    !!fieldG && fieldG.w === 8 && fieldG.h === 9,
    JSON.stringify(fieldG)
  );
  console.log('     state:', JSON.stringify(grown.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('after grow: all panels inside the 12x16 grid', inBounds(grown));
  check('after grow: no two panels overlap', noOverlap(grown));
  check('after grow: no panel rendered outside the container', !(await anyPanelOffScreen(page)));

  console.log('\n4d) Corrupted persisted layout is sanitized on load');
  await page.evaluate(() => {
    const raw = localStorage.getItem('layout');
    const arr = JSON.parse(raw);
    const chat = arr.find((i) => i.i === 'chat');
    chat.y = 20; // below the 16-row grid
    localStorage.setItem('layout', JSON.stringify(arr));
  });
  await page.reload({ waitUntil: 'domcontentloaded' });
  await page.waitForSelector('button:has-text("Connetti")', { timeout: 15000 });
  await page.click('button:has-text("Connetti")');
  await page.waitForSelector('div.react-grid-item', { timeout: 20000 });
  await sleep(900);
  const sanitized = await readLayout(page);
  const chatS = sanitized.find((i) => i.i === 'chat');
  check(
    'chat pulled back on-screen (y + h <= 16)',
    !!chatS && chatS.y + chatS.h <= ROWS,
    JSON.stringify(chatS)
  );
  console.log('     state:', JSON.stringify(sanitized.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('sanitized layout inside the grid', inBounds(sanitized));
  check('after sanitize: no panel rendered outside the container', !(await anyPanelOffScreen(page)));

  console.log('\n5️⃣  Persistence (debounced save → localStorage)');
  await sleep(900);
  const saved = await readLayout(page);
  check('layout persisted with 7 panels', !!saved && saved.length === 7, `entries=${saved?.length}`);
  check('persisted layout is valid (in bounds)', inBounds(saved));
  check('persisted layout differs from the untouched default', differsFromDefault(saved));

  console.log('\n6️⃣  Persistence across reload');
  const preReloadDice = await getPanelRect(page, 'Dice Log');
  await page.reload({ waitUntil: 'domcontentloaded' });
  // Auth state lives in memory — log in again (credentials are re-seeded by addInitScript)
  await page.waitForSelector('button:has-text("Connetti")', { timeout: 15000 });
  await page.click('button:has-text("Connetti")');
  await page.waitForSelector('div.react-grid-item', { timeout: 20000 });
  await sleep(400);
  const reloadedDice = await getPanelRect(page, 'Dice Log');
  check(
    'dice-log restored at its pre-reload position after reload',
    !!preReloadDice &&
      !!reloadedDice &&
      Math.abs(reloadedDice.left - preReloadDice.left) < 3 &&
      Math.abs(reloadedDice.width - preReloadDice.width) < 3,
    JSON.stringify(reloadedDice)
  );

  console.log('\n7️⃣  Reset layout');
  await page.click('button[title="Ripristina layout predefinito"]');
  await sleep(400);
  const resetDice = await getPanelRect(page, 'Dice Log');
  check(
    'dice-log back at default position after reset',
    !!resetDice &&
      Math.abs(resetDice.left - initialDice.left) < 3 &&
      Math.abs(resetDice.width - initialDice.width) < 3,
    JSON.stringify(resetDice)
  );
  await sleep(600);
  // Reset persists the default layout (RGL re-emits onLayoutChange with a
  // normalized copy, so the key holds the default values — not a custom layout)
  const afterReset = await page.evaluate(() => {
    const raw = localStorage.getItem('layout');
    if (!raw) return null;
    const dice = JSON.parse(raw).find((i) => i.i === 'dice-log');
    return dice ? { x: dice.x, y: dice.y, w: dice.w, h: dice.h } : null;
  });
  check(
    'storage holds the DEFAULT layout after reset (no custom layout left)',
    !!afterReset && afterReset.x === 3 && afterReset.y === 12 && afterReset.w === 5 && afterReset.h === 4,
    JSON.stringify(afterReset)
  );

  console.log('\n8️⃣  Runtime errors');
  check('no uncaught page errors', pageErrors.length === 0, pageErrors.join(' | '));
  if (consoleErrors.length) {
    console.log(`  ℹ️  ${consoleErrors.length} console.error(s) (non-fatal, mostly faked-API fallout):`);
    for (const e of consoleErrors.slice(0, 5)) console.log('     -', e.slice(0, 160));
  }

  await browser.close();

  console.log(
    `\n${failures === 0 ? '🎉 ALL CHECKS PASSED' : `💥 ${failures} CHECK(S) FAILED`} (${passes} passed, ${failures} failed)`
  );
  console.log('Screenshots: /tmp/verify-dashboard-1-initial.png, /tmp/verify-dashboard-2-after-drag-resize.png');

  try {
    process.kill(-server.pid);
  } catch {
    /* already dead */
  }
  process.exit(failures === 0 ? 0 : 1);
}

main().catch((err) => {
  console.error('Verification crashed:', err);
  process.exit(1);
});
