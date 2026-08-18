// =============================================================================
// Panel — Standard dashboard panel wrapper for the resizable grid layout
//
// IMPORTANT (react-grid-layout v2): GridItem clones the child element directly
// and merges the "react-grid-item" class, positioning styles, element ref and
// the mouse handlers used by react-draggable (onMouseDown / onMouseUp /
// onTouchEnd, injected through the react-resizable wrapper) onto it — there is
// no extra wrapper div like in v1. This component MUST therefore forward
// className, style, ref AND all remaining DOM props to its root element.
// Swallowing the mouse handlers makes dragging silently impossible.
//
// The title bar is the ONLY drag handle (class "panel-title") so clicks and
// drags inside the panel content (players, chat input, ...) never move the
// panel. react-grid-layout appends the resize handle as the LAST child of
// {children} (i.e. inside the content wrapper); with the imported
// react-resizable CSS it is positioned absolutely at the bottom-right corner.
// =============================================================================

import { HTMLAttributes, ReactNode, Ref } from 'react';

interface PanelProps extends Omit<HTMLAttributes<HTMLDivElement>, 'title'> {
  /** Panel title shown in the title bar */
  title: string;
  /** Optional icon rendered before the title */
  icon?: ReactNode;
  /** Optional action buttons rendered on the right side of the title bar */
  actions?: ReactNode;
  /** Panel content (fills the remaining space) */
  children?: ReactNode;
  /** Forwarded by react-grid-layout (element ref) */
  ref?: Ref<HTMLDivElement>;
}

export default function Panel({
  title,
  icon,
  actions,
  children,
  className,
  style,
  ref,
  ...rest
}: PanelProps) {
  return (
    <div
      ref={ref}
      style={style}
      {...rest}
      className={`h-full w-full flex flex-col bg-gray-900 border border-gray-700 rounded-lg overflow-hidden ${className ?? ''}`}
    >
      {/* Title bar — drag handle (react-grid-layout dragConfig.handle = '.panel-title') */}
      <div className="panel-title flex items-center gap-2 px-2 h-7 bg-gray-800/90 border-b border-gray-700 cursor-move select-none shrink-0">
        {icon && <span className="text-xs leading-none">{icon}</span>}
        <span className="text-[11px] font-semibold uppercase tracking-wider text-gray-300 truncate">
          {title}
        </span>
        {actions && <div className="ml-auto flex items-center gap-1">{actions}</div>}
      </div>

      {/* Content */}
      <div className="flex-1 min-h-0 min-w-0">{children}</div>
    </div>
  );
}
