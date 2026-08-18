// =============================================================================
// layoutConfig — Dashboard grid layout: panel ids, default layout, persistence
// Grid: 12 columns. Default layout replicates the original fixed layout:
//   header (full width) / rosters + field / player info + dice log + chat
// =============================================================================

import { LayoutItem } from 'react-grid-layout';
import { getStorage } from '../../services/storage';

// -----------------------------------------------------------------------------
// Panel ids (must match the `key` of each child rendered in DashboardLayout)
// -----------------------------------------------------------------------------
export const PANEL_IDS = {
  header: 'header',
  team1: 'team1',
  field: 'field',
  team2: 'team2',
  playerInfo: 'player-info',
  diceLog: 'dice-log',
  chat: 'chat',
} as const;

export type PanelId = (typeof PANEL_IDS)[keyof typeof PANEL_IDS];

// -----------------------------------------------------------------------------
// Default layout — mirrors the original fixed flexbox dashboard
// -----------------------------------------------------------------------------
export const DEFAULT_LAYOUT: LayoutItem[] = [
  { i: PANEL_IDS.header, x: 0, y: 0, w: 12, h: 2, minW: 6, minH: 1 },
  { i: PANEL_IDS.team1, x: 0, y: 2, w: 2, h: 10, minW: 2, minH: 4 },
  { i: PANEL_IDS.field, x: 2, y: 2, w: 8, h: 10, minW: 4, minH: 5 },
  { i: PANEL_IDS.team2, x: 10, y: 2, w: 2, h: 10, minW: 2, minH: 4 },
  { i: PANEL_IDS.playerInfo, x: 0, y: 12, w: 3, h: 4, minW: 3, minH: 3 },
  { i: PANEL_IDS.diceLog, x: 3, y: 12, w: 5, h: 4, minW: 3, minH: 3 },
  { i: PANEL_IDS.chat, x: 8, y: 12, w: 4, h: 4, minW: 3, minH: 3 },
];

/**
 * Total number of rows spanned by the default layout (max y + h).
 * DashboardLayout uses it to scale the row height so the grid always fills
 * the available container height (no dead space at the bottom).
 */
export const LAYOUT_TOTAL_ROWS = Math.max(
  ...DEFAULT_LAYOUT.map((item) => item.y + item.h)
);

/** Number of columns in the dashboard grid. */
export const GRID_COLS = 12;

// -----------------------------------------------------------------------------
// Bounded layout sanitization
//
// The dashboard grid is exactly GRID_COLS x LAYOUT_TOTAL_ROWS and the window
// is sized to it: any panel pushed past the last row during a drag/resize
// collision cascade would render outside the window. The sanitizer restores
// that invariant after every compaction:
//   1. swap-first — while a drag is in progress, if the dragged panel covers
//      a panel that fits into the dragged panel's pre-drag slot, they swap
//      (the user asked for "spostarsi o invertire di posto");
//   2. nearest free slot — any panel still off-grid or overlapping another
//      is relocated into the closest free slot;
//   3. last resort — if the grid is over capacity (no free slot), the panel
//      is clamped on-screen and may overlap (never off-screen).
// -----------------------------------------------------------------------------
export interface DragSource {
  i: string;
  x: number;
  y: number;
  w: number;
  h: number;
}

function collideAt(
  item: { x: number; y: number; w: number; h: number },
  x: number,
  y: number,
  w: number,
  h: number
): boolean {
  return x < item.x + item.w && item.x < x + w && y < item.y + item.h && item.y < y + h;
}

function fitsAt(
  items: LayoutItem[],
  item: LayoutItem,
  x: number,
  y: number,
  cols: number,
  rows: number
): boolean {
  return (
    x >= 0 &&
    y >= 0 &&
    x + item.w <= cols &&
    y + item.h <= rows &&
    !items.some((o) => o.i !== item.i && collideAt(o, x, y, item.w, item.h))
  );
}

/** Closest free (x, y) slot to the item's current position (Manhattan
 *  distance, deterministic scan order), or null if the grid is full. */
function findNearestFreeSlot(
  items: LayoutItem[],
  item: LayoutItem,
  cols: number,
  rows: number
): { x: number; y: number } | null {
  const others = items.filter((o) => o.i !== item.i);
  const sx = Math.min(Math.max(item.x, 0), Math.max(0, cols - item.w));
  const sy = Math.min(Math.max(item.y, 0), Math.max(0, rows - item.h));
  let best: { x: number; y: number } | null = null;
  let bestD = Infinity;
  for (let y = 0; y <= rows - item.h; y++) {
    for (let x = 0; x <= cols - item.w; x++) {
      if (others.some((o) => collideAt(o, x, y, item.w, item.h))) continue;
      const d = Math.abs(x - sx) + Math.abs(y - sy);
      if (d < bestD) {
        bestD = d;
        best = { x, y };
        if (d === 0) return best;
      }
    }
  }
  return best;
}

/**
 * Returns a copy of `layout` in which every panel is inside the
 * cols x rows grid (and no panel overlaps another, unless the total area
 * exceeds the grid capacity). Idempotent: a valid layout comes back
 * unchanged.
 */
export function sanitizeBoundedLayout(
  layout: readonly LayoutItem[],
  cols = GRID_COLS,
  rows = LAYOUT_TOTAL_ROWS,
  dragSource: DragSource | null = null
): LayoutItem[] {
  const items = layout.map((item) => ({ ...item }));
  for (const item of items) {
    if (item.x < 0) item.x = 0;
    if (item.x + item.w > cols) item.x = Math.max(0, cols - item.w);
  }

  const isOffGrid = (it: LayoutItem) => it.y + it.h > rows;
  const overlapsAny = (it: LayoutItem) =>
    items.some((o) => o.i !== it.i && collideAt(o, it.x, it.y, it.w, it.h));

  const maxPasses = items.length * rows + 2;
  for (let pass = 0; pass < maxPasses; pass++) {
    // 1) Swap-first: the dragged panel takes the hovered slot, the covered
    //    panel goes back to the dragged panel's pre-drag slot.
    if (dragSource) {
      const dragged = items.find((it) => it.i === dragSource.i);
      if (dragged && (isOffGrid(dragged) || overlapsAny(dragged))) {
        const covered = items.filter(
          (o) => o.i !== dragged.i && collideAt(o, dragged.x, dragged.y, dragged.w, dragged.h)
        );
        for (const other of covered) {
          if (fitsAt(items, other, dragSource.x, dragSource.y, cols, rows)) {
            other.x = dragSource.x;
            other.y = dragSource.y;
            break;
          }
        }
      }
    }

    // 2) Relocate every panel that is off-grid or still overlapping.
    const offenders = items.filter((it) => isOffGrid(it) || overlapsAny(it));
    if (offenders.length === 0) break;
    if (dragSource && offenders.every((o) => o.i === dragSource.i)) break;
    const item = offenders
      .sort((a, b) => b.y + b.h - (a.y + a.h) || b.y - a.y || b.x - a.x)[0];
    if (dragSource && item.i === dragSource.i) {
      // The dragged panel follows the mouse (gridBounds already keeps it
      // in-bounds) — clamp it and leave the others to the next pass.
      item.x = Math.min(item.x, Math.max(0, cols - item.w));
      item.y = Math.min(item.y, Math.max(0, rows - item.h));
      continue;
    }
    const slot = findNearestFreeSlot(items, item, cols, rows);
    if (slot) {
      item.x = slot.x;
      item.y = slot.y;
    } else {
      // 3) Last resort (grid over capacity): keep the panel on-screen.
      item.x = Math.min(item.x, Math.max(0, cols - item.w));
      item.y = Math.max(0, rows - item.h);
      break;
    }
  }
  return items;
}

/** sanitizeBoundedLayout without drag context (used when loading layouts). */
export function sanitizeLayout(
  layout: readonly LayoutItem[],
  cols = GRID_COLS,
  rows = LAYOUT_TOTAL_ROWS
): LayoutItem[] {
  return sanitizeBoundedLayout(layout, cols, rows, null);
}

const VALID_IDS = new Set<string>(Object.values(PANEL_IDS));
const STORAGE_KEY = 'layout';

// -----------------------------------------------------------------------------
// Load persisted layout (returns default if missing or invalid)
// -----------------------------------------------------------------------------
export async function loadLayout(): Promise<LayoutItem[]> {
  try {
    const storage = await getStorage();
    const saved = await storage.get<LayoutItem[]>(STORAGE_KEY);
    if (
      Array.isArray(saved) &&
      saved.length === DEFAULT_LAYOUT.length &&
      saved.every(
        (item) =>
          item &&
          typeof item.i === 'string' &&
          VALID_IDS.has(item.i) &&
          [item.x, item.y, item.w, item.h].every((n) => Number.isFinite(n))
      )
    ) {
      // Sanitize: layouts persisted by older versions (or hand-edited) may
      // contain panels below the grid — pull them back on-screen before use.
      return sanitizeLayout(saved);
    }
  } catch (e) {
    console.warn('[layoutConfig] Failed to load layout, using default', e);
  }
  return DEFAULT_LAYOUT;
}

// -----------------------------------------------------------------------------
// Save layout (callers debounce)
// -----------------------------------------------------------------------------
export async function saveLayout(layout: LayoutItem[]): Promise<void> {
  try {
    const storage = await getStorage();
    await storage.set(STORAGE_KEY, layout);
  } catch (e) {
    console.warn('[layoutConfig] Failed to save layout', e);
  }
}

// -----------------------------------------------------------------------------
// Reset layout to default and clear persisted data
// -----------------------------------------------------------------------------
export async function resetLayout(): Promise<void> {
  try {
    const storage = await getStorage();
    await storage.delete(STORAGE_KEY);
  } catch (e) {
    console.warn('[layoutConfig] Failed to reset layout', e);
  }
}
