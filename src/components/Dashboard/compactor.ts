// =============================================================================
// compactor — bounded vertical compactor for the dashboard grid
// =============================================================================
//
// react-grid-layout's default verticalCompactor pulls every item up to the
// first free row and never pushes items back inside the grid. On a fixed
// 12x16 grid with a full bottom row, that pushes panels below the last row
// (off the window). We wrap it with a sanitizer that restores the in-bounds
// + no-overlap invariants after every compaction (see layoutConfig).
//
// The sanitizer also keeps the anchor (field) horizontally centered and
// fitted with its side regions, and enforces the dragged panel's region
// target (side slot / band) so the panel previews its final shape live.
//
// The pre-drag position of the dragged panel (`dragSourceRef`) and the
// pre-resize geometry of the anchor (`anchorRef`) are passed via refs
// because the compactor is created once (useMemo) while those change on
// every interaction.
// =============================================================================

import { Compactor, Layout, verticalCompactor } from 'react-grid-layout';
import {
  AnchorState,
  DragSource,
  GRID_COLS,
  LAYOUT_TOTAL_ROWS,
  detectDragTarget,
  sanitizeBoundedLayout,
} from './layoutConfig';

export function createBoundedCompactor(
  cols = GRID_COLS,
  rows = LAYOUT_TOTAL_ROWS,
  dragSourceRef?: { current: DragSource | null },
  anchorRef?: { current: AnchorState | null }
): Compactor {
  return {
    type: 'vertical',
    allowOverlap: false,
    compact(layout: Layout, gridCols: number): Layout {
      // The region target is detected on the PRE-compaction layout: the
      // vertical compactor would already have pushed the dragged panel out
      // of the side slot (or pulled it up out of the band), hiding the
      // cursor's intent.
      const target = detectDragTarget(
        layout,
        cols,
        rows,
        dragSourceRef?.current ?? null,
        anchorRef?.current ?? null
      );
      const compacted = verticalCompactor.compact(layout, gridCols);
      return sanitizeBoundedLayout(
        compacted,
        cols,
        rows,
        dragSourceRef?.current ?? null,
        anchorRef?.current ?? null,
        target
      ) as Layout;
    },
  };
}
