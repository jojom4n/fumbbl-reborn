// =============================================================================
// DashboardLayout — Resizable/draggable dashboard (react-grid-layout v2)
// Every panel can be dragged by its title bar and resized from the
// bottom-right handle. The layout is persisted (Tauri Store / localStorage)
// and can be reset to the default arrangement.
// =============================================================================

import { useEffect, useMemo, useReducer, useRef, useState } from 'react';
import { RotateCcw, Wrench } from 'lucide-react';
import { ResponsiveGridLayout, useContainerWidth } from 'react-grid-layout';
import { Layout, LayoutItem } from 'react-grid-layout';

// -----------------------------------------------------------------------------
// Unique ID generator (safe, no precision loss)
// CRITICAL: The previous implementation concatenated Date.now() + counter
// into a 19-digit number then converted with Number(), which exceeds
// Number.MAX_SAFE_INTEGER (2^53). This caused precision loss and React
// duplicate key warnings like "Encountered two children with the same key".
// Fix: Use a simple auto-incrementing counter that stays within safe range.
// -----------------------------------------------------------------------------
let _chatIdCounter = 0;
const generateChatId = (): number => {
  _chatIdCounter += 1;
  return _chatIdCounter;
};

import Panel from './Panel';
import Header from './Header';
import GameField from './GameField';
import RadialMenu from './RadialMenu';
import TeamRoster from './TeamRoster';
import PlayerInfo from './PlayerInfo';
import DiceLog from './DiceLog';
import Chat from './Chat';
import { useGameState } from '../../contexts/GameContext';
import {
  BloodBowlAction,
  FieldPosition,
  Player,
  RadialMenuItem,
} from '../../types/bloodbowl';
import {
  DEFAULT_LAYOUT,
  DragSource,
  GRID_COLS,
  LAYOUT_TOTAL_ROWS,
  loadLayout,
  saveLayout,
} from './layoutConfig';
import { createBoundedCompactor } from './compactor';
import { dashboardConstraint } from './gridConstraints';

// Blood Bowl field dimensions
const FIELD_WIDTH = 26;
const FIELD_HEIGHT = 15;

// Grid configuration (12 columns — see layoutConfig.GRID_COLS; the window
// min-width keeps it usable)
const GRID_MARGIN: [number, number] = [8, 8];
// Row height fallback used before the container height is first measured
const GRID_ROW_HEIGHT_FALLBACK = 36;
// Below this panels would become unusable (window min-height keeps it above)
const MIN_ROW_HEIGHT = 24;

// Default radial menu items (BB2025 actions)
const DEFAULT_RADIAL_ITEMS: Omit<RadialMenuItem, 'angle' | 'available'>[] = [
  { action: 'move', hotkey: 'M', label: 'Move' },
  { action: 'secureBall', hotkey: 'S', label: 'Secure' },
  { action: 'block', hotkey: 'B', label: 'Block' },
  { action: 'blitz', hotkey: 'L', label: 'Blitz' },
  { action: 'pass', hotkey: 'P', label: 'Pass' },
  { action: 'handOff', hotkey: 'H', label: 'Hand-Off' },
  { action: 'throwTeamMate', hotkey: 'T', label: 'Throw' },
  { action: 'foul', hotkey: 'F', label: 'Foul' },
  { action: 'special', hotkey: 'E', label: 'Special' },
];

interface DashboardLayoutProps {
  onToggleDebug?: () => void;
  isDebugEnabled?: boolean;
  onLogout?: () => void;
  username?: string;
}

export default function DashboardLayout({ onToggleDebug, isDebugEnabled, onLogout, username }: DashboardLayoutProps) {
  const { gameState, selectPlayer, sendAction, addChatMessage, clearDiceLog } = useGameState();

  // ---------------------------------------------------------------------------
  // Layout state + persistence (debounced)
  // ---------------------------------------------------------------------------
  const [layout, setLayout] = useState<Layout>(DEFAULT_LAYOUT);
  const layoutLoadedRef = useRef(false);
  const saveTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  // Pre-drag position of the panel being dragged. The bounded compactor
  // reads it so that a panel covered by the drag can swap back into the
  // vacated slot (instead of being pushed off the grid).
  const dragSourceRef = useRef<DragSource | null>(null);
  const gridCompactor = useMemo(
    () => createBoundedCompactor(GRID_COLS, LAYOUT_TOTAL_ROWS, dragSourceRef),
    []
  );

  // Load persisted layout once on mount
  useEffect(() => {
    loadLayout().then((loaded) => {
      layoutLoadedRef.current = true;
      setLayout(loaded);
    });
  }, []);

  // Debounced save of the current layout
  const scheduleLayoutSave = (next: Layout) => {
    if (saveTimerRef.current) clearTimeout(saveTimerRef.current);
    saveTimerRef.current = setTimeout(() => {
      saveLayout(next as LayoutItem[]);
    }, 500);
  };

  useEffect(() => {
    return () => {
      if (saveTimerRef.current) clearTimeout(saveTimerRef.current);
    };
  }, []);

  const handleLayoutChange = (next: Layout) => {
    setLayout(next);
    if (layoutLoadedRef.current) scheduleLayoutSave(next);
  };

  const handleResetLayout = () => {
    setLayout(DEFAULT_LAYOUT);
    // Persist the default layout instead of deleting the key: react-grid-layout
    // re-emits onLayoutChange (with a normalized copy) right after the layouts
    // prop changes, and a delete would race with that debounced re-save.
    // Saving the default is deterministic — both writes produce the same state.
    if (saveTimerRef.current) {
      clearTimeout(saveTimerRef.current);
      saveTimerRef.current = null;
    }
    saveLayout(DEFAULT_LAYOUT);
  };

  // ---------------------------------------------------------------------------
  // Radial menu state — stores field position for correct placement
  // ---------------------------------------------------------------------------
  type RadialAction =
    | { type: 'show'; player: Player; position: FieldPosition }
    | { type: 'hide' }
    | { type: 'select'; action: BloodBowlAction };

  const [radialMenu, setRadialMenu] = useReducer(
    (state: { visible: boolean; player: Player | null; position: FieldPosition }, action: RadialAction) => {
      switch (action.type) {
        case 'show':
          return { visible: true, player: action.player, position: action.position };
        case 'hide':
          return { visible: false, player: null, position: { x: 0, y: 0 } };
        case 'select':
          // Handle action selection (radial menu stays open for multiple actions)
          return state;
        default:
          return state;
      }
    },
    { visible: false, player: null, position: { x: 0, y: 0 } } as { visible: boolean; player: Player | null; position: FieldPosition }
  );

  const handleFieldPlayerSelect = (player: Player, position: FieldPosition) => {
    selectPlayer(player);
    setRadialMenu({ type: 'show', player, position });
  };

  const handleRosterPlayerSelect = (player: Player) => {
    selectPlayer(player);
    if (player.fieldX !== undefined && player.fieldY !== undefined) {
      setRadialMenu({ type: 'show', player, position: { x: player.fieldX, y: player.fieldY } });
    }
  };

  const handleActionSelect = (action: BloodBowlAction) => {
    if (!radialMenu.player) return;
    const { player } = radialMenu;
    setRadialMenu({ type: 'select', action });
    sendAction(action, Number(player.id));
  };

  const handleRadialMenuClose = () => {
    setRadialMenu({ type: 'hide' });
  };

  // Hide the radial menu as soon as a panel is dragged or resized (the field
  // panel moves and the menu anchor would become stale)
  const handleGridDragStart = (
    _layout: Layout,
    oldItem: LayoutItem | null,
    newItem: LayoutItem | null
  ) => {
    setRadialMenu({ type: 'hide' });
    // Remember where the panel started from: the bounded compactor uses this
    // to swap a covered panel back into the vacated slot during the drag.
    const source = oldItem ?? newItem;
    if (source) {
      dragSourceRef.current = {
        i: source.i,
        x: source.x,
        y: source.y,
        w: source.w,
        h: source.h,
      };
    }
  };
  const handleGridDragStop = () => {
    dragSourceRef.current = null;
  };
  const handleGridResizeStart = () => {
    setRadialMenu({ type: 'hide' });
  };

  // ---------------------------------------------------------------------------
  // Radial menu anchor — computed from the field box rect so the menu is
  // never clipped by the panel overflow and always follows the field panel
  // ---------------------------------------------------------------------------
  const fieldPanelRef = useRef<HTMLDivElement>(null);
  const [menuAnchor, setMenuAnchor] = useState<{ left: number; top: number } | null>(null);

  useEffect(() => {
    if (!radialMenu.visible || !radialMenu.player) {
      setMenuAnchor(null);
      return;
    }

    const compute = () => {
      const box = fieldPanelRef.current?.querySelector('[data-field-box]') as HTMLElement | null;
      if (!box) return;
      const rect = box.getBoundingClientRect();
      // Menu diameter is ~230px; keep it inside the viewport
      const M = 120;
      const left = Math.min(
        Math.max(rect.left + ((radialMenu.position.x + 0.5) / FIELD_WIDTH) * rect.width, M),
        Math.max(M, window.innerWidth - M)
      );
      const top = Math.min(
        Math.max(rect.top + ((radialMenu.position.y + 0.5) / FIELD_HEIGHT) * rect.height, M),
        Math.max(M, window.innerHeight - M)
      );
      setMenuAnchor({ left, top });
    };

    compute();
    window.addEventListener('resize', compute);
    return () => window.removeEventListener('resize', compute);
  }, [radialMenu.visible, radialMenu.player, radialMenu.position]);

  // ---------------------------------------------------------------------------
  // Radial menu items
  // ---------------------------------------------------------------------------
  const radialItems: RadialMenuItem[] = DEFAULT_RADIAL_ITEMS.map((item, index) => ({
    ...item,
    angle: (index * 360) / DEFAULT_RADIAL_ITEMS.length - 90, // Start from top
    available: true, // TODO: Filter based on context
  }));

  // ---------------------------------------------------------------------------
  // Header panel actions (debug toggle + reset layout)
  // ---------------------------------------------------------------------------
  const headerActions = (
    <>
      {onToggleDebug && (
        <button
          onClick={onToggleDebug}
          className={`flex items-center gap-1 px-2 py-0.5 rounded text-[10px] font-bold transition-colors ${
            isDebugEnabled
              ? 'bg-yellow-600 hover:bg-yellow-700 text-white'
              : 'bg-gray-700 hover:bg-gray-600 text-gray-300'
          }`}
          title="Toggle Debug Panel"
        >
          <Wrench size={10} />
          {isDebugEnabled ? 'Debug ON' : 'Debug'}
        </button>
      )}
      <button
        onClick={handleResetLayout}
        className="flex items-center gap-1 px-2 py-0.5 rounded text-[10px] font-bold bg-gray-700 hover:bg-gray-600 text-gray-300 transition-colors"
        title="Ripristina layout predefinito"
      >
        <RotateCcw size={10} />
        Reset
      </button>
    </>
  );

  // ---------------------------------------------------------------------------
  // Dashboard grid — container size measured (v2 API requires width; height is
  // measured too so the row height can be scaled to fill the whole window)
  // ---------------------------------------------------------------------------
  const { width: gridWidth, containerRef: gridContainerRef } = useContainerWidth();

  const [containerHeight, setContainerHeight] = useState(0);
  useEffect(() => {
    const node = gridContainerRef.current;
    if (!node) return;
    let rafId: number | null = null;
    const observer = new ResizeObserver((entries) => {
      const height = Math.round(entries[0]?.contentRect.height ?? 0);
      if (rafId !== null) cancelAnimationFrame(rafId);
      rafId = requestAnimationFrame(() => {
        rafId = null;
        setContainerHeight((prev) => (prev === height ? prev : height));
      });
    });
    observer.observe(node);
    return () => {
      if (rafId !== null) cancelAnimationFrame(rafId);
      observer.disconnect();
    };
  }, [gridContainerRef]);

  // Scale rows so LAYOUT_TOTAL_ROWS exactly fills the measured container
  // height — no dead space at the bottom on any window size
  const rowHeight =
    containerHeight > 0
      ? Math.max(
          MIN_ROW_HEIGHT,
          (containerHeight - GRID_MARGIN[1] * (LAYOUT_TOTAL_ROWS + 1)) / LAYOUT_TOTAL_ROWS
        )
      : GRID_ROW_HEIGHT_FALLBACK;

  return (
    <div className="h-screen flex flex-col bg-gray-950 text-white">
      <div ref={gridContainerRef} className="flex-1 min-h-0">
        <ResponsiveGridLayout
          width={gridWidth}
          breakpoint="lg"
          breakpoints={{ lg: 0 }}
          cols={{ lg: GRID_COLS }}
          layouts={{ lg: layout }}
          rowHeight={rowHeight}
          maxRows={LAYOUT_TOTAL_ROWS}
          margin={GRID_MARGIN}
          dragConfig={{ enabled: true, handle: '.panel-title' }}
          resizeConfig={{ enabled: true, handles: ['se'] }}
          compactor={gridCompactor}
          constraints={[dashboardConstraint]}
          onLayoutChange={handleLayoutChange}
          onDragStart={handleGridDragStart}
          onDragStop={handleGridDragStop}
          onResizeStart={handleGridResizeStart}
          className="flex-1 min-h-0"
          style={{ height: '100%' }}
        >
          {/* Header */}
          <Panel key="header" title="Match" icon="🏈" actions={headerActions}>
            <Header gameState={gameState} onSettingsClick={onLogout} username={username} />
          </Panel>

          {/* Left — Team 1 Roster */}
          <Panel key="team1" title={gameState.team1.name} icon="🛡️">
            <TeamRoster
              team={gameState.team1Players}
              teamName={gameState.team1.name}
              teamColor={gameState.team1.color}
              onPlayerSelect={handleRosterPlayerSelect}
              selectedPlayerId={gameState.selectedPlayer?.id}
            />
          </Panel>

          {/* Center — Game Field */}
          <Panel key="field" title="Field" icon="🎯">
            <div ref={fieldPanelRef} className="h-full w-full">
              <GameField
                team1Players={gameState.team1Players}
                team2Players={gameState.team2Players}
                ballPosition={gameState.ballPosition}
                selectedPlayer={gameState.selectedPlayer}
                onPlayerSelect={handleFieldPlayerSelect}
              />
            </div>
          </Panel>

          {/* Right — Team 2 Roster */}
          <Panel key="team2" title={gameState.team2.name} icon="🛡️">
            <TeamRoster
              team={gameState.team2Players}
              teamName={gameState.team2.name}
              teamColor={gameState.team2.color}
              onPlayerSelect={handleRosterPlayerSelect}
              selectedPlayerId={gameState.selectedPlayer?.id}
            />
          </Panel>

          {/* Bottom — Player Info */}
          <Panel key="player-info" title="Player" icon="👤">
            <PlayerInfo player={gameState.selectedPlayer} />
          </Panel>

          {/* Bottom — Dice Log */}
          <Panel key="dice-log" title="Dice Log" icon="🎲">
            <DiceLog entries={gameState.diceLog} onClear={clearDiceLog} />
          </Panel>

          {/* Bottom — Chat */}
          <Panel key="chat" title="Chat" icon="💬">
            <Chat
              messages={gameState.chatMessages}
              onSend={(text) => {
                addChatMessage({
                  id: generateChatId(),
                  sender: 'You',
                  senderColor: 'text-blue-400',
                  text,
                  timestamp: Date.now(),
                  type: 'general' as const,
                });
              }}
            />
          </Panel>
        </ResponsiveGridLayout>
      </div>

      {/* Radial Menu — fixed overlay anchored to the field (never clipped) */}
      {radialMenu.visible && radialMenu.player && menuAnchor && (
        <div className="fixed z-30" style={{ left: menuAnchor.left, top: menuAnchor.top }}>
          <RadialMenu
            player={radialMenu.player}
            items={radialItems}
            onActionSelect={handleActionSelect}
            onClose={handleRadialMenuClose}
          />
        </div>
      )}
    </div>
  );
}
