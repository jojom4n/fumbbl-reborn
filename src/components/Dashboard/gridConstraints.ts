// =============================================================================
// gridConstraints — Dashboard grid constraint (REPLACES RGL's built-in defaults).
//
// Passing a `constraints` prop replaces react-grid-layout's default
// [gridBounds, minMaxSize] pair, so this object re-implements both:
//   1. grid bounds  — dragged/resized position and size clamped to the grid
//   2. min/max size — per-item minW/minH/maxW/maxH honoured
// plus a dashboard-specific rule:
//   3. neighbour stop — a resize handle can never cover another panel
//      (v1-style behaviour), so resizing cannot push a neighbouring panel
//      away or off-screen.
//
// NOTE: `LayoutConstraint` is not exported from the package root, so the
// object is typed structurally against the `constraints` prop.
// =============================================================================

import { LayoutItem, ResizeHandleAxis } from 'react-grid-layout';

interface ConstraintContextLike {
  cols: number;
  maxRows: number;
  containerWidth: number;
  containerHeight: number;
  rowHeight: number;
  margin: readonly [number, number];
  layout: LayoutItem[];
}

export const dashboardConstraint = {
  name: 'dashboardBounds',

  // (1) gridBounds — keep the dragged panel inside the grid.
  constrainPosition(
    item: LayoutItem,
    x: number,
    y: number,
    { cols, maxRows }: ConstraintContextLike
  ): { x: number; y: number } {
    return {
      x: Math.min(Math.max(0, x), Math.max(0, cols - item.w)),
      y: Math.min(Math.max(0, y), Math.max(0, maxRows - item.h)),
    };
  },

  // (1) gridBounds + (2) minMaxSize + (3) neighbour stop.
  constrainSize(
    item: LayoutItem,
    w: number,
    h: number,
    handle: ResizeHandleAxis,
    { cols, maxRows, layout }: ConstraintContextLike
  ): { w: number; h: number } {
    // (1) cannot grow beyond the grid edges. The anchor (static) is exempt
    //     from the current-x width clamp: it may grow across the whole grid
    //     — the sanitizer re-centers it (x = (cols - w) / 2) on every
    //     compaction, so clamping to the current x would stall the growth.
    const maxW = item.static
      ? cols
      : handle === 'w' || handle === 'nw' || handle === 'sw'
        ? item.x + item.w
        : cols - item.x;
    const maxH =
      handle === 'n' || handle === 'nw' || handle === 'ne' ? item.y + item.h : maxRows - item.y;
    let nw = Math.min(Math.max(1, w), Math.max(1, maxW));
    let nh = Math.min(Math.max(1, h), Math.max(1, maxH));

    // (2) per-item limits
    nw = Math.min(Math.max(item.minW ?? 1, nw), item.maxW ?? Infinity);
    nh = Math.min(Math.max(item.minH ?? 1, nh), item.maxH ?? Infinity);

    // (3) neighbour stop — implemented for the 'se' handle (the only one in
    //     resizeConfig). v1-style rules, deterministic and conservative:
    //     - right limit: items overlapping the item's CURRENT row span stop
    //       the width at their left edge (also resolves a sliver overlap by
    //       shrinking back to the neighbour's edge);
    //     - bottom limit: items overlapping the (right-limited) column span
    //       stop the height at their top edge.
    //     Never applied to the anchor: it grows over the side regions and the
    //     sanitizer refits the panels living there. Static panels count as
    //     neighbours — the field's edge stops any other resize.
    if (handle === 'se' && !item.static) {
      let rightLimit = Infinity;
      let bottomLimit = Infinity;
      for (const other of layout) {
        if (other.i === item.i) continue;
        const rowsOverlap = other.y < item.y + item.h && item.y < other.y + other.h;
        const colsOverlap = other.x < item.x + nw && item.x < other.x + other.w;
        if (rowsOverlap && other.x >= item.x) {
          rightLimit = Math.min(rightLimit, other.x - item.x);
        }
        // strictly below the top edge: same-row panels are horizontal
        // neighbours (handled by rightLimit), not vertical ones
        if (colsOverlap && other.y > item.y) {
          bottomLimit = Math.min(bottomLimit, other.y - item.y);
        }
      }
      nw = Math.min(nw, rightLimit);
      nh = Math.min(nh, bottomLimit);
      // neighbour clamps must never shrink an item below its minimum size
      nw = Math.max(nw, item.minW ?? 1);
      nh = Math.max(nh, item.minH ?? 1);
    }

    return { w: nw, h: nh };
  },
};
