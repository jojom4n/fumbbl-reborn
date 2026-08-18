// =============================================================================
// compactor — Bounded vertical compactor for the dashboard grid.
//
// Wraps react-grid-layout's standard vertical compactor with the bounded
// sanitizer from layoutConfig: after the usual gap-filling, any panel the
// drag/resize collision cascade pushed beyond the last row is relocated
// (swap-first with the dragged panel, then nearest free slot). This
// guarantees every panel stays inside the GRID_COLS x LAYOUT_TOTAL_ROWS grid
// at all times — nothing is ever rendered outside the window.
//
// `dragSourceRef` must hold the pre-drag position of the dragged panel while
// a drag is in progress (see DashboardLayout's onDragStart/onDragStop).
// =============================================================================

import { Compactor, Layout, verticalCompactor } from 'react-grid-layout';
import {
  DragSource,
  GRID_COLS,
  LAYOUT_TOTAL_ROWS,
  sanitizeBoundedLayout,
} from './layoutConfig';

export function createBoundedCompactor(
  cols = GRID_COLS,
  rows = LAYOUT_TOTAL_ROWS,
  dragSourceRef?: { current: DragSource | null }
): Compactor {
  return {
    type: 'vertical',
    allowOverlap: false,
    compact(layout: Layout, gridCols: number): Layout {
      const compacted = verticalCompactor.compact(layout, gridCols);
      return sanitizeBoundedLayout(
        compacted,
        cols,
        rows,
        dragSourceRef?.current ?? null
      ) as Layout;
    },
  };
}
