// =============================================================================
// layoutConfig — Dashboard grid layout: panel ids, default layout, persistence
// Grid: 12 columns. Default layout replicates the original fixed layout:
//   header (full width) / rosters + field / player info + dice log + chat
//
// The game field (PANEL_IDS.field) is the grid's ANCHOR:
//   - it cannot be dragged and the compactor treats it as a static obstacle
//     (collision cascades never move it), but it IS resizable;
//   - it is always kept horizontally centered: x = round((cols - w) / 2);
//   - the panels occupying the side regions next to it (the "roster slots")
//     are fitted to those regions, so they grow/contract with the field
//     (live, during the field's resize).
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
  { i: PANEL_IDS.team1, x: 0, y: 2, w: 2, h: 10, minW: 1, minH: 4 },
  {
    i: PANEL_IDS.field,
    x: 2,
    y: 2,
    w: 8,
    h: 10,
    minW: 4,
    minH: 5,
    // Never wider than cols - 2: at full width a side region would collapse
    // below the roster's minW (1) and its panel would be stranded in the
    // band. Capping at 10 keeps at least a 1-column region on each side.
    maxW: 10,
    // The field is the grid's anchor: `static` makes the compactor treat it
    // as an immovable obstacle (collision cascades never move it); the
    // explicit flags then control the interactions — not draggable, but
    // still resizable (static alone would disable resizing as well).
    static: true,
    isDraggable: false,
    isResizable: true,
  },
  { i: PANEL_IDS.team2, x: 10, y: 2, w: 2, h: 10, minW: 1, minH: 4 },
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
// that invariant after every compaction, and keeps the anchor (field)
// horizontally centered with its side regions fitted:
//
//   anchor  — the field is re-centered (x = round((cols - w) / 2)) and the
//     non-dragged panels occupying the side regions next to it are fitted to
//     those regions (this is what makes the rosters grow/contract live while
//     the field is resized);
//   target  — the dragged panel's region target (side slot / band), computed
//     BEFORE the compactor runs, is enforced on it (live: the dragged item's
//     rendered size comes from this sanitized layout, so the user sees the
//     final shape during the drag);
//   swap    — while a drag is in progress, if the dragged panel covers a
//     panel that fits into the dragged panel's pre-drag slot, they swap
//     (the user asked for "spostarsi o invertire di posto");
//   relocate— any panel still off-grid or overlapping another is relocated
//     into the closest free slot (with a reduced height if the grid has no
//     room for its full size);
//   clamp   — last resort (grid over capacity): the panel is kept on-screen
//     and may overlap (never off-screen).
// -----------------------------------------------------------------------------
export interface DragSource {
  i: string;
  x: number;
  y: number;
  w: number;
  h: number;
}

/** Geometry of the anchor (field) at the moment its resize started. */
export interface AnchorState {
  x: number;
  y: number;
  w: number;
  h: number;
}

/** Final shape and position a dragged panel snaps to (side slot or band). */
export interface RegionTarget {
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

/** Nearest slot for the item: first at its current size; if the grid has no
 *  room for the full size, the nearest cell where it fits with a reduced
 *  height — a slot at the panel's minimum height always beats a sub-minH
 *  sliver, which is only used when the grid is over capacity (the user can
 *  restore the size with the resize handle). Proximity first, then tallest
 *  fit. */
function findNearestFreeSlotShrinkable(
  items: LayoutItem[],
  item: LayoutItem,
  cols: number,
  rows: number
): { x: number; y: number; h: number } | null {
  const full = findNearestFreeSlot(items, item, cols, rows);
  if (full) return { ...full, h: item.h };
  const others = items.filter((o) => o.i !== item.i);
  const sx = Math.min(Math.max(item.x, 0), Math.max(0, cols - item.w));
  const sy = Math.min(Math.max(item.y, 0), rows - 1);
  let best: { x: number; y: number; h: number } | null = null;
  let bestKey = Infinity;
  for (let y = 0; y < rows; y++) {
    for (let x = 0; x <= cols - item.w; x++) {
      // Largest height fitting at (x, y) (consecutive free rows).
      let h = 0;
      for (let r = y; r < rows; r++) {
        if (others.some((o) => collideAt(o, x, r, item.w, 1))) break;
        h++;
      }
      if (h < 1) continue;
      const d = Math.abs(x - sx) + Math.abs(y - sy);
      // A slot that can host the panel at its minimum height always ranks
      // before a sub-minH sliver (only acceptable when the grid is over
      // capacity); then nearest, then tallest fit.
      const key = (h < (item.minH ?? 1) ? 100_000 : 0) + d * 1000 - h;
      if (key < bestKey) {
        bestKey = key;
        best = { x, y, h };
      }
    }
  }
  return best;
}

// ---------------------------------------------------------------------------
// Anchor geometry: centering + side regions
// ---------------------------------------------------------------------------

/**
 * The two side regions next to the anchor. Detection bounds (dx0/dx1) use
 * the anchor's pre-resize geometry when a resize of the anchor is in
 * progress: by the time the compactor has run, the side panels it displaced
 * have left their original slots, so the current (grown) zones would no
 * longer contain them. Placement (px/pw) always uses the current
 * (re-centered) anchor.
 */
function sideZones(
  anchor: LayoutItem,
  anchorState: AnchorState | null,
  cols: number
): { dx0: number; dx1: number; px: number; pw: number }[] {
  const za = anchorState ?? anchor;
  return [
    { dx0: 0, dx1: za.x, px: 0, pw: anchor.x },
    { dx0: za.x + za.w, dx1: cols, px: anchor.x + anchor.w, pw: cols - anchor.x - anchor.w },
  ];
}

/** The panel is aligned with the anchor's span (same height) and either
 *  vertically overlaps it or sits in the band below it (the compactor may
 *  have pushed a displaced side panel there). */
function isSideSlotPanel(it: LayoutItem, anchor: LayoutItem): boolean {
  if (it.h !== anchor.h) return false;
  const bandStart = anchor.y + anchor.h;
  return (it.y < bandStart && it.y + it.h > anchor.y) || it.y >= bandStart;
}

/** Keep the anchor horizontally centered: x = round((cols - w) / 2). */
function recenterAnchor(anchor: LayoutItem, cols: number): void {
  anchor.x = Math.max(0, Math.min(cols - anchor.w, Math.round((cols - anchor.w) / 2)));
}

/**
 * Fit the side-slot panels to their region. While the anchor is being
 * resized the panels are force-fitted (the region is split evenly between
 * the panels on a side) — this is what makes the rosters grow/contract
 * live. In steady state only panels wider than their region are repaired
 * (a user-chosen width is kept).
 */
function fitSideRegions(
  items: LayoutItem[],
  anchor: LayoutItem,
  zones: { dx0: number; dx1: number; px: number; pw: number }[],
  draggedId: string | null,
  activeResize: boolean
): void {
  for (const zone of zones) {
    if (zone.pw <= 0) continue;
    const cands = items
      .filter((it) => it.i !== anchor.i && it.i !== draggedId && !it.static)
      .filter((it) => it.x >= zone.dx0 && it.x + it.w <= zone.dx1)
      .filter((it) => isSideSlotPanel(it, anchor))
      .sort((a, b) => a.x - b.x);
    if (cands.length === 0) continue;
    if (activeResize) {
      const base = Math.floor(zone.pw / cands.length);
      const extra = zone.pw % cands.length;
      let offset = 0;
      cands.forEach((it, k) => {
        const w = base + (k < extra ? 1 : 0);
        if (w >= (it.minW ?? 1) && anchor.h >= (it.minH ?? 1)) {
          it.x = zone.px + offset;
          it.w = w;
          it.y = anchor.y;
          it.h = anchor.h;
        }
        offset += w;
      });
    } else {
      for (const it of cands) {
        if (it.w > zone.pw) {
          it.x = zone.px;
          it.w = zone.pw;
        }
      }
    }
  }
}

/**
 * Where the dragged panel should end up, decided BEFORE the compactor runs
 * (from its cursor position — the compactor would already have pushed it
 * out of the side slot, hiding the user's intent):
 *   - a side region next to the field → the full slot (fitted to the
 *     field's span); the panel is rendered at that size during the drag, so
 *     the user sees the final shape as a preview;
 *   - the band below the field → same width, snapped to the band top, height
 *     fitted to the band;
 *   - otherwise → null (free placement: compactor + sanitizer decide).
 */
export function detectDragTarget(
  layout: readonly LayoutItem[],
  cols: number,
  rows: number,
  dragSource: DragSource | null,
  anchorState: AnchorState | null
): RegionTarget | null {
  if (!dragSource) return null;
  const items = layout.map((item) => ({ ...item }));
  const dragged = items.find((it) => it.i === dragSource.i);
  const anchor = items.find((it) => it.i === PANEL_IDS.field);
  if (!dragged || !anchor || dragged.i === anchor.i) return null;
  recenterAnchor(anchor, cols);
  const zones = sideZones(anchor, anchorState, cols);
  const bandStart = anchor.y + anchor.h;
  const bandH = rows - bandStart;

  for (const zone of zones) {
    if (
      zone.pw >= (dragged.minW ?? 1) &&
      zone.pw <= (dragged.maxW ?? Infinity) &&
      anchor.h >= (dragged.minH ?? 1) &&
      anchor.h <= (dragged.maxH ?? Infinity) &&
      dragged.x < zone.dx1 &&
      dragged.x + dragged.w > zone.dx0 &&
      dragged.y < bandStart &&
      dragged.y + dragged.h > anchor.y
    ) {
      return { x: zone.px, y: anchor.y, w: zone.pw, h: anchor.h };
    }
  }
  if (bandH > 0 && dragged.y + dragged.h > bandStart) {
    const h = Math.min(dragged.h, bandH);
    if (h >= (dragged.minH ?? 1)) {
      return {
        x: Math.max(0, Math.min(dragged.x, Math.max(0, cols - dragged.w))),
        y: bandStart,
        w: dragged.w,
        h,
      };
    }
  }
  return null;
}

/**
 * Returns a copy of `layout` in which every panel is inside the
 * cols x rows grid, the anchor is centered with its side regions fitted,
 * and no panel overlaps another (unless the total area exceeds the grid
 * capacity). Idempotent: a valid layout comes back unchanged.
 */
export function sanitizeBoundedLayout(
  layout: readonly LayoutItem[],
  cols = GRID_COLS,
  rows = LAYOUT_TOTAL_ROWS,
  dragSource: DragSource | null = null,
  anchorState: AnchorState | null = null,
  dragTarget: RegionTarget | null = null
): LayoutItem[] {
  const items = layout.map((item) => ({ ...item }));
  for (const item of items) {
    if (item.x < 0) item.x = 0;
    if (item.x + item.w > cols) item.x = Math.max(0, cols - item.w);
  }

  const isOffGrid = (it: LayoutItem) =>
    it.x < 0 || it.y < 0 || it.x + it.w > cols || it.y + it.h > rows;
  const overlapsAny = (it: LayoutItem) =>
    items.some((o) => o.i !== it.i && collideAt(o, it.x, it.y, it.w, it.h));

  // 1) Anchor: normalize flags (layouts saved before the anchor feature
  //    lack them), re-center, fit the side regions.
  const anchor = items.find((it) => it.i === PANEL_IDS.field);
  if (anchor) {
    anchor.static = true;
    anchor.isDraggable = false;
    anchor.isResizable = true;
    // A side region can never collapse below the roster's minW (1): clamp
    // the width (also repairs layouts persisted by older versions, which
    // allowed the field to span the whole grid).
    anchor.w = Math.min(anchor.w, cols - 2);
    recenterAnchor(anchor, cols);
    const activeResize = Boolean(anchorState);
    fitSideRegions(
      items,
      anchor,
      sideZones(anchor, anchorState, cols),
      dragSource?.i ?? null,
      activeResize
    );
  }

  // 2) Enforce the dragged panel's region target (side slot / band).
  const dragged = dragSource ? items.find((it) => it.i === dragSource.i) : null;
  if (dragged && dragTarget && (!anchor || dragged.i !== anchor.i)) {
    dragged.x = dragTarget.x;
    dragged.y = dragTarget.y;
    dragged.w = dragTarget.w;
    dragged.h = dragTarget.h;
  }

  const interactive = Boolean(dragSource || anchorState);
  const maxPasses = items.length * rows + 2;
  for (let pass = 0; pass < maxPasses; pass++) {
    // 3) Swap-first: the dragged panel takes the hovered slot, the covered
    //    panel goes back to the dragged panel's pre-drag slot.
    if (dragSource && dragged && (isOffGrid(dragged) || overlapsAny(dragged))) {
      const covered = items.filter(
        (o) =>
          o.i !== dragged.i &&
          !o.static &&
          collideAt(o, dragged.x, dragged.y, dragged.w, dragged.h)
      );
      for (const other of covered) {
        if (fitsAt(items, other, dragSource.x, dragSource.y, cols, rows)) {
          other.x = dragSource.x;
          other.y = dragSource.y;
          break;
        }
      }
    }

    // 4) Relocate every panel that is off-grid or still overlapping. The
    //    anchor is only repaired when no drag/resize is in progress
    //    (load-time sanitization); live it is the immovable reference.
    const offenders = items
      .filter((it) => it.i !== dragSource?.i && (!it.static || !interactive))
      .filter((it) => isOffGrid(it) || overlapsAny(it))
      .sort((a, b) => b.y + b.h - (a.y + a.h) || b.y - a.y || b.x - a.x);
    const draggedBad = dragged && (isOffGrid(dragged) || overlapsAny(dragged));
    if (offenders.length === 0 && !draggedBad) break;
    if (draggedBad) {
      // The dragged panel follows the mouse: clamp it on-screen (overlap
      // allowed, off-screen never) and keep repairing the panels it
      // displaces — the swap above handles the pre-drag slot, the
      // relocation below handles everything else.
      dragged.x = Math.max(0, Math.min(dragged.x, Math.max(0, cols - dragged.w)));
      dragged.y = Math.max(0, Math.min(dragged.y, Math.max(0, rows - dragged.h)));
    }
    const item = offenders[0];
    if (!item) break;
    const slot = findNearestFreeSlotShrinkable(items, item, cols, rows);
    if (slot) {
      item.x = slot.x;
      item.y = slot.y;
      item.h = slot.h;
    } else {
      // 5) Last resort (grid over capacity): keep the panel on-screen.
      item.x = Math.max(0, Math.min(item.x, Math.max(0, cols - item.w)));
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
  return sanitizeBoundedLayout(layout, cols, rows, null, null, null);
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
