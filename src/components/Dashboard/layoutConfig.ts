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
      return saved;
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
