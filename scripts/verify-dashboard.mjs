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
//   5. Resize WORKS (synthetic mouse events on the SE handle grow the panel)
//   6. The new layout is persisted (localStorage) and survives a reload
//   7. Reset restores the default layout and clears persistence
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
  const preW = draggedDice.width;
  const preH = draggedDice.height;
  await mouseDrag(
    page,
    { root: 'div.react-grid-item', title: 'Dice Log', targetSel: '.react-resizable-handle-se' },
    150,
    60
  );
  const resizedDice = await getPanelRect(page, 'Dice Log');
  const dw = resizedDice ? resizedDice.width - preW : NaN;
  const dh = resizedDice ? resizedDice.height - preH : NaN;
  check('panel grew wider by ~150px', dw >= 80, `dw=${dw.toFixed(1)}px`);
  check('panel grew taller by ~60px', dh >= 30, `dh=${dh.toFixed(1)}px`);
  await page.screenshot({ path: '/tmp/verify-dashboard-2-after-drag-resize.png' });

  console.log('\n5️⃣  Persistence (debounced save → localStorage)');
  await sleep(900); // save debounce is 500ms
  const saved = await page.evaluate(() => {
    const raw = localStorage.getItem('layout');
    if (!raw) return null;
    const arr = JSON.parse(raw);
    return arr.find((i) => i.i === 'dice-log') || null;
  });
  const defaultDice = { x: 3, y: 12, w: 5, h: 4 };
  const changed =
    !!saved &&
    (saved.x !== defaultDice.x || saved.y !== defaultDice.y || saved.w !== defaultDice.w || saved.h !== defaultDice.h);
  check('layout saved to localStorage with modified dice-log entry', changed, JSON.stringify(saved));

  console.log('\n6️⃣  Persistence across reload');
  await page.reload({ waitUntil: 'domcontentloaded' });
  // Auth state lives in memory — log in again (credentials are re-seeded by addInitScript)
  await page.waitForSelector('button:has-text("Connetti")', { timeout: 15000 });
  await page.click('button:has-text("Connetti")');
  await page.waitForSelector('div.react-grid-item', { timeout: 20000 });
  await sleep(400);
  const reloadedDice = await getPanelRect(page, 'Dice Log');
  check(
    'dice-log restored at modified position after reload',
    !!reloadedDice &&
      Math.abs(reloadedDice.left - resizedDice.left) < 3 &&
      Math.abs(reloadedDice.width - resizedDice.width) < 3,
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
