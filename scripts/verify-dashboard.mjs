// =============================================================================
// verify-dashboard.mjs — End-to-end verification of the react-grid-layout v2
// dashboard (real Chromium, real built app, faked FUMBBL auth endpoints).
//
// Verifies:
//   1. All 7 panels render as grid items
//   2. Resize handles are real hit targets (20x20, absolute, se-resize cursor)
//      and the field is static (not draggable) but resizable
//   3. The grid fills the whole window height (no dead space at the bottom)
//   4. Drag & drop WORKS (synthetic mousedown/mousemove/mouseup on a title bar
//      actually moves the panel — this catches the swallowed onMouseDown bug)
//   4b. Resize WORKS (synthetic mouse events on the SE handle) and STOPS at
//       the neighbouring panel (never pushes panels off-screen)
//   4a. The field cannot be dragged (anchor) — it stays put and centered
//   4b2. Field shrink/grow: it re-centers and the side panels grow/contract
//        to fill the side regions (live, during the resize)
//   4c. A side roster dragged downward snaps into the bottom band (h fitted)
//   4d. A bottom panel dragged into a side region snaps to the field's span
//       and PREVIEWs its final height live during the drag
//   4e. Horizontal resize: works (band panel grows to the grid edge) and
//       stops at the field's edge for side panels
//   4f. Drag past the grid edge: panels are clamped on-screen (bounded),
//       bottom band items shrink to keep a clean tiling
//   4g. Corrupted persisted layout (panel below the grid) is sanitized on load
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

// Pixel geometry of the grid (must match DashboardLayout: margin 8, row
// height scaled to fill the container: (H - 8 * (ROWS + 1)) / ROWS)
async function getGridGeom(page) {
  return page.evaluate(() => {
    const c = document.querySelector('div.react-grid-layout').getBoundingClientRect();
    const M = 8;
    const colWidth = (c.width - M * 11 - M * 2) / 12;
    const rowHeight = (c.height - M * 17) / 16;
    return { colWidth, rowHeight, colPitch: colWidth + M, rowPitch: rowHeight + M };
  });
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
//
// opts.midProbe: async (page) => ... called after the FINAL mousemove and
// BEFORE the mouseup — the dragged panel is still rendered at the cursor
// with its post-compaction size (the "shaded" final shape).
// opts.pos: expected rendered rect of the panel ({left, top, width, height}
// in px, from getGridGeom) — matches the item by position instead of by
// title (the roster panels have an EMPTY title until real team data is
// loaded, so a title search cannot find them).
async function mouseDrag(page, { root, title, targetSel, pos }, dx, dy, opts = {}) {
  const { x, y } = await page.evaluate(
    ({ root, title, targetSel, pos }) => {
      const items = [...document.querySelectorAll(root)];
      const item = pos
        ? items.find((el) => {
            const r = el.getBoundingClientRect();
            return Math.abs(r.left - pos.left) < 6 && Math.abs(r.top - pos.top) < 6;
          })
        : items.find((el) => {
            const bar = el.querySelector('.panel-title');
            return bar && bar.textContent.toUpperCase().includes(title.toUpperCase());
          });
      if (!item) throw new Error('panel not found: ' + (pos ? 'pos ' + JSON.stringify(pos) : title));
      const target = item.querySelector(targetSel);
      if (!target) throw new Error('drag target not found: ' + targetSel);
      const r = target.getBoundingClientRect();
      return { x: r.left + r.width / 2, y: r.top + r.height / 2 };
    },
    { root, title, targetSel, pos }
  );

  await page.evaluate(
    ({ x, y, root, title, targetSel, pos }) => {
      const items = [...document.querySelectorAll(root)];
      const item = pos
        ? items.find((el) => {
            const r = el.getBoundingClientRect();
            return Math.abs(r.left - pos.left) < 6 && Math.abs(r.top - pos.top) < 6;
          })
        : items.find((el) => {
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
    { x, y, root, title, targetSel, pos }
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
    // Live-preview probe (after the final mousemove, before the mouseup).
    // opts.settleMs waits first so headless event coalescing can settle and
    // the live (sanitized) preview shape is final before we measure it.
    if (opts.midProbe && i === steps) {
      if (opts.settleMs) await sleep(opts.settleMs);
      await opts.midProbe(page);
    }
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

// Resize a panel's SE corner by steering the mouse with FEEDBACK on the live
// rendered size (re-read from the DOM after every step) instead of assuming a
// fixed pixel delta. Headless browsers coalesce synthetic mousemove events,
// so the library's internal deltas can double-count — chasing the actual
// rendered width/height makes the end state deterministic: once the mouse is
// up the size snaps to the target column/row counts (px targets are the
// column/row centers, far inside the ±colWidth/2 snap band).
async function mouseResizeTo(page, title, targetWpx, targetHpx) {
  // Self-contained item lookup (must be inlined in every page.evaluate: the
  // callback runs in the page scope, where Node helpers don't exist).
  const handlePos = await page.evaluate((tt) => {
    const items = [...document.querySelectorAll('div.react-grid-item')];
    const item = items.find((el) => {
      const bar = el.querySelector('.panel-title');
      return bar && bar.textContent.toUpperCase().includes(tt.toUpperCase());
    });
    if (!item) throw new Error('panel not found: ' + tt);
    const r = item.querySelector('.react-resizable-handle-se').getBoundingClientRect();
    return { x: r.left + r.width / 2, y: r.top + r.height / 2 };
  }, title);
  await page.evaluate(
    ({ tt, x, y }) => {
      const items = [...document.querySelectorAll('div.react-grid-item')];
      const item = items.find((el) => {
        const bar = el.querySelector('.panel-title');
        return bar && bar.textContent.toUpperCase().includes(tt.toUpperCase());
      });
      item
        .querySelector('.react-resizable-handle-se')
        .dispatchEvent(
          new MouseEvent('mousedown', { bubbles: true, cancelable: true, view: window, button: 0, clientX: x, clientY: y })
        );
    },
    { tt: title, x: handlePos.x, y: handlePos.y }
  );
  await sleep(80);
  let mx = handlePos.x;
  let my = handlePos.y;
  for (let i = 0; i < 60; i++) {
    const rect = await page.evaluate((tt) => {
      const items = [...document.querySelectorAll('div.react-grid-item')];
      const item = items.find((el) => {
        const bar = el.querySelector('.panel-title');
        return bar && bar.textContent.toUpperCase().includes(tt.toUpperCase());
      });
      const r = item.getBoundingClientRect();
      return { width: r.width, height: r.height };
    }, title);
    const errW = targetWpx - rect.width;
    const errH = targetHpx - rect.height;
    if (Math.abs(errW) < 8 && Math.abs(errH) < 8) break;
    mx += Math.max(-15, Math.min(15, errW));
    my += Math.max(-15, Math.min(15, errH));
    await page.evaluate(
      ({ cx, cy }) => {
        document.dispatchEvent(
          new MouseEvent('mousemove', { bubbles: true, cancelable: true, view: window, button: 0, clientX: cx, clientY: cy })
        );
      },
      { cx: mx, cy: my }
    );
    await sleep(60);
  }
  await page.evaluate(
    ({ cx, cy }) => {
      document.dispatchEvent(
        new MouseEvent('mouseup', { bubbles: true, cancelable: true, view: window, button: 0, clientX: cx, clientY: cy })
      );
    },
    { cx: mx, cy: my }
  );
  await sleep(300); // let React commit the post-resize layout
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

  // The field is the grid anchor: static (not draggable) but resizable
  const fieldFlags = await page.evaluate(() => {
    const items = [...document.querySelectorAll('div.react-grid-item')];
    const item = items.find((el) => {
      const bar = el.querySelector('.panel-title');
      return bar && bar.textContent.toUpperCase().includes('FIELD');
    });
    if (!item) return null;
    return {
      static: item.classList.contains('static'),
      draggable: item.classList.contains('react-draggable'),
      resizable: !!item.querySelector('.react-resizable-handle-se'),
    };
  });
  check(
    'field is static (not draggable) but resizable (SE handle present)',
    !!fieldFlags && fieldFlags.static === true && fieldFlags.draggable === false && fieldFlags.resizable === true,
    JSON.stringify(fieldFlags)
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

  console.log('\n4a) Anchor: the field cannot be dragged (stays put, stays centered)');
  const fieldBefore = await getPanelRect(page, 'Field');
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Field', targetSel: '.panel-title' },
    200,
    0
  );
  const fieldAfterDrag = await getPanelRect(page, 'Field');
  check(
    'dragging the field does nothing (rect unchanged)',
    !!fieldBefore &&
      !!fieldAfterDrag &&
      Math.abs(fieldAfterDrag.left - fieldBefore.left) < 1 &&
      Math.abs(fieldAfterDrag.width - fieldBefore.width) < 1,
    `before=${JSON.stringify(fieldBefore)} after=${JSON.stringify(fieldAfterDrag)}`
  );

  console.log('\n4b) Anchor: shrink the field -> re-centered, side panels grow to fill');
  const geom = await getGridGeom(page);
  const sizePx = (w, h) => [w * geom.colWidth + (w - 1) * 8, h * geom.rowHeight + (h - 1) * 8];
  // Rendered rect (px) of the grid cell (x, y, w, h) — used to locate
  // title-less panels (rosters) by position.
  const posPx = (x, y, w, h) => ({ left: 8 + x * geom.colPitch, top: 8 + y * geom.rowPitch, width: sizePx(w, h)[0], height: sizePx(w, h)[1] });
  await mouseResizeTo(page, 'Field', sizePx(6, 10)[0], sizePx(6, 10)[1]);
  await sleep(900);
  const shrunk = await readLayout(page);
  const fieldS = shrunk.find((i) => i.i === 'field');
  const team1S = shrunk.find((i) => i.i === 'team1');
  const team2S = shrunk.find((i) => i.i === 'team2');
  check(
    'field shrank to 6 columns and re-centered (x=3, w=6)',
    !!fieldS && fieldS.x === 3 && fieldS.y === 2 && fieldS.w === 6 && fieldS.h === 10,
    JSON.stringify(fieldS)
  );
  check(
    'left roster grew to fill the widened left region (0,2,3,10)',
    !!team1S && team1S.x === 0 && team1S.y === 2 && team1S.w === 3 && team1S.h === 10,
    JSON.stringify(team1S)
  );
  check(
    'right roster grew to fill the widened right region (9,2,3,10)',
    !!team2S && team2S.x === 9 && team2S.y === 2 && team2S.w === 3 && team2S.h === 10,
    JSON.stringify(team2S)
  );
  console.log('     state:', JSON.stringify(shrunk.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('after shrink: all panels inside the 12x16 grid', inBounds(shrunk));
  check('after shrink: no two panels overlap', noOverlap(shrunk));
  check('after shrink: no panel rendered outside the container', !(await anyPanelOffScreen(page)));
  await page.screenshot({ path: '/tmp/verify-dashboard-4-field-shrink.png' });

  console.log('\n4c) Anchor: grow the field -> re-centered, side panels contract');
  await mouseResizeTo(page, 'Field', sizePx(10, 10)[0], sizePx(10, 10)[1]);
  await sleep(900);
  const grown = await readLayout(page);
  const fieldG = grown.find((i) => i.i === 'field');
  const team1G = grown.find((i) => i.i === 'team1');
  const team2G = grown.find((i) => i.i === 'team2');
  check(
    'field grew to 10 columns and re-centered (x=1, w=10)',
    !!fieldG && fieldG.x === 1 && fieldG.y === 2 && fieldG.w === 10 && fieldG.h === 10,
    JSON.stringify(fieldG)
  );
  check(
    'left roster contracted to 1 column (0,2,1,10)',
    !!team1G && team1G.x === 0 && team1G.y === 2 && team1G.w === 1 && team1G.h === 10,
    JSON.stringify(team1G)
  );
  check(
    'right roster contracted to 1 column (11,2,1,10)',
    !!team2G && team2G.x === 11 && team2G.y === 2 && team2G.w === 1 && team2G.h === 10,
    JSON.stringify(team2G)
  );
  console.log('     state:', JSON.stringify(grown.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('after grow: all panels inside the 12x16 grid', inBounds(grown));
  check('after grow: no two panels overlap', noOverlap(grown));
  check('after grow: no panel rendered outside the container', !(await anyPanelOffScreen(page)));
  await page.screenshot({ path: '/tmp/verify-dashboard-5-field-grow.png' });

  console.log('\n4d) Reset -> default layout restored (field centered again)');
  await page.click('button[title="Ripristina layout predefinito"]');
  await sleep(900);
  const afterReset0 = await readLayout(page);
  const fieldR0 = afterReset0.find((i) => i.i === 'field');
  check(
    'reset restores the default field (2,2,8,10)',
    !!fieldR0 && fieldR0.x === 2 && fieldR0.y === 2 && fieldR0.w === 8 && fieldR0.h === 10,
    JSON.stringify(fieldR0)
  );

  console.log('\n4e) Side panel draggable downward: roster snaps into the bottom band');
  // The roster has no title (fake auth -> empty team name): locate it by
  // its grid position (default slot 0,2,2,10).
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Team 1', targetSel: '.panel-title', pos: posPx(0, 2, 2, 10) },
    4 * geom.colPitch,
    5 * geom.rowPitch
  );
  await sleep(900);
  const rosterBand = await readLayout(page);
  const team1B = rosterBand.find((i) => i.i === 'team1');
  check(
    'roster moved into the bottom band (y=12, h=4, w=2, x near target)',
    !!team1B && team1B.y === 12 && team1B.w === 2 && team1B.h === 4 && team1B.x >= 3 && team1B.x <= 5,
    JSON.stringify(team1B)
  );
  console.log('     state:', JSON.stringify(rosterBand.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('roster band-fit: all panels inside the 12x16 grid', inBounds(rosterBand));
  check('roster band-fit: no panel rendered outside the container', !(await anyPanelOffScreen(page)));
  await page.screenshot({ path: '/tmp/verify-dashboard-6-roster-band.png' });

  console.log('\n4f) Zone snap: bottom panel dragged into a side region (live preview)');
  await page.click('button[title="Ripristina layout predefinito"]');
  await sleep(400);
  // Widen the side regions first (field 8 -> 6 cols) so the chat (minW=3) fits
  await mouseResizeTo(page, 'Field', sizePx(6, 10)[0], sizePx(6, 10)[1]);
  await sleep(900);
  let liveChatHeight = null;
  // -7.5 cols (not -7) so the item lands mid-zone even if headless event
  // coalescing skews the final mouse position by a column: the item's left
  // edge tracks mouse.x - w/2, so mouse col 2.5 -> item col ~0.5 in [0,2).
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Chat', targetSel: '.panel-title' },
    -7.5 * geom.colPitch,
    -8 * geom.rowPitch,
    {
      settleMs: 600,
      midProbe: async (pg) => {
        liveChatHeight = await pg.evaluate(() => {
          const items = [...document.querySelectorAll('div.react-grid-item')];
          const item = items.find((el) => {
            const bar = el.querySelector('.panel-title');
            return bar && bar.textContent.toUpperCase().includes('CHAT');
          });
          return item ? item.getBoundingClientRect().height : null;
        });
      },
    }
  );
  const expectedSideH = 10 * geom.rowHeight + 9 * 8; // 10 rows of the field span
  check(
    'dragged chat PREVIEWs its final height live (h=10 rows while dragging)',
    !!liveChatHeight && Math.abs(liveChatHeight - expectedSideH) < 10,
    `live=${liveChatHeight?.toFixed(1)}px expected=${expectedSideH.toFixed(1)}px`
  );
  await sleep(900);
  const zoneSnap = await readLayout(page);
  const chatZ = zoneSnap.find((i) => i.i === 'chat');
  const team1Z = zoneSnap.find((i) => i.i === 'team1');
  check(
    'chat snapped to the left side slot (0,2,3,10 — field span)',
    !!chatZ && chatZ.x === 0 && chatZ.y === 2 && chatZ.w === 3 && chatZ.h === 10,
    JSON.stringify(chatZ)
  );
  check(
    'evicted roster left the side slot (relocated somewhere legal)',
    !!team1Z && !(team1Z.x === 0 && team1Z.y === 2 && team1Z.w === 3 && team1Z.h === 10),
    JSON.stringify(team1Z)
  );
  console.log('     state:', JSON.stringify(zoneSnap.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('zone snap: all panels inside the 12x16 grid', inBounds(zoneSnap));
  check('zone snap: no two panels overlap', noOverlap(zoneSnap));
  check('zone snap: no panel rendered outside the container', !(await anyPanelOffScreen(page)));
  await page.screenshot({ path: '/tmp/verify-dashboard-7-zone-snap.png' });

  console.log('\n4g) Horizontal resize: works for a band panel (grows +1 column)');
  // player-info always owns the leftmost band slot after 4f: resize IT
  // (the roster's slot is compactor-dependent at this point).
  const gState = await readLayout(page);
  const player0 = gState.find((i) => i.i === 'player-info');
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Player', targetSel: '.react-resizable-handle-se', pos: posPx(player0.x, player0.y, player0.w, player0.h) },
    1 * geom.colPitch,
    0
  );
  await sleep(900);
  const hResize = await readLayout(page);
  const playerH = hResize.find((i) => i.i === 'player-info');
  check(
    'band panel grew at least one column, stayed in the band, bounded by neighbours',
    !!playerH &&
      playerH.x === player0.x &&
      playerH.y === 12 &&
      playerH.h === 4 &&
      playerH.w >= player0.w + 1 &&
      playerH.x + playerH.w <= 12,
    JSON.stringify(playerH)
  );
  console.log('     state:', JSON.stringify(hResize.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('h-resize: all panels inside the 12x16 grid', inBounds(hResize));
  check('h-resize: no two panels overlap', noOverlap(hResize));

  console.log('\n4h) Horizontal resize: side panel stops at the field edge');
  await page.click('button[title="Ripristina layout predefinito"]');
  await sleep(900);
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Team 1', targetSel: '.react-resizable-handle-se', pos: posPx(0, 2, 2, 10) },
    1 * geom.colPitch,
    0
  );
  await sleep(900);
  const stopResize = await readLayout(page);
  const team1Stop = stopResize.find((i) => i.i === 'team1');
  const fieldStop = stopResize.find((i) => i.i === 'field');
  check(
    'roster resize stopped at the field edge (stays 0,2,2,10)',
    !!team1Stop && team1Stop.x === 0 && team1Stop.y === 2 && team1Stop.w === 2 && team1Stop.h === 10,
    JSON.stringify(team1Stop)
  );
  check('field untouched by the roster resize (2,2,8,10)', !!fieldStop && fieldStop.x === 2 && fieldStop.w === 8 && fieldStop.h === 10, JSON.stringify(fieldStop));
  check('h-resize stop: no two panels overlap', noOverlap(stopResize));

  console.log('\n4i) Bounded: drag header past the bottom edge (stays on-screen)');
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
    'header snapped to the band top (y=12) and stayed in the grid',
    !!headerA && headerA.x === 0 && headerA.y === 12 && headerA.w === 12 && headerA.h === 2,
    JSON.stringify(headerA)
  );
  console.log('     state:', JSON.stringify(afterEdge.map((i) => [i.i, i.x, i.y, i.w, i.h])));
  check('after edge-drag: all panels inside the 12x16 grid', inBounds(afterEdge));
  check('after edge-drag: no two panels overlap', noOverlap(afterEdge));
  check('after edge-drag: no panel rendered outside the container', !(await anyPanelOffScreen(page)));
  await page.screenshot({ path: '/tmp/verify-dashboard-3-collision.png' });

  console.log('\n4j) Corrupted persisted layout is sanitized on load');
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
  const fieldSan = sanitized.find((i) => i.i === 'field');
  check(
    'field re-centered on load (x=2 for w=8)',
    !!fieldSan && fieldSan.x === 2 && fieldSan.w === 8,
    JSON.stringify(fieldSan)
  );
  check('after sanitize: no two panels overlap', noOverlap(sanitized));
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
    const arr = JSON.parse(raw);
    const dice = arr.find((i) => i.i === 'dice-log');
    const field = arr.find((i) => i.i === 'field');
    return {
      dice: dice ? { x: dice.x, y: dice.y, w: dice.w, h: dice.h } : null,
      field: field ? { x: field.x, y: field.y, w: field.w, h: field.h } : null,
    };
  });
  check(
    'storage holds the DEFAULT layout after reset (no custom layout left)',
    !!afterReset?.dice &&
      afterReset.dice.x === 3 && afterReset.dice.y === 12 && afterReset.dice.w === 5 && afterReset.dice.h === 4,
    JSON.stringify(afterReset?.dice)
  );
  check(
    'storage field is the default centered anchor (2,2,8,10)',
    !!afterReset?.field &&
      afterReset.field.x === 2 && afterReset.field.y === 2 && afterReset.field.w === 8 && afterReset.field.h === 10,
    JSON.stringify(afterReset?.field)
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
  console.log(
    'Screenshots: /tmp/verify-dashboard-{1-initial,2-after-drag-resize,3-collision,4-field-shrink,5-field-grow,6-roster-band,7-zone-snap}.png'
  );

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
