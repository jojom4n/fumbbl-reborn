// =============================================================================
// DashboardLayout — Fixed layout container for the FUMBBL Reborn Dashboard
// This is the ONLY component that knows where each piece is placed
// =============================================================================

import { useReducer } from 'react';

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

// Blood Bowl field dimensions
const FIELD_WIDTH = 26;
const FIELD_HEIGHT = 15;

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

  // Radial menu state — stores field position for correct placement
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

  // Handle player selection from field — shows radial menu
  const handleFieldPlayerSelect = (player: Player, position: FieldPosition) => {
    selectPlayer(player);
    setRadialMenu({ type: 'show', player, position });
  };

  // Handle player selection from roster — only selects player, no radial menu
  const handleRosterPlayerSelect = (player: Player) => {
    selectPlayer(player);
  };

  // Handle radial menu action
  const handleActionSelect = (action: BloodBowlAction) => {
    if (radialMenu.player) {
      sendAction(action, Number(radialMenu.player.id));
    }
  };

  // Close radial menu
  const handleRadialMenuClose = () => {
    setRadialMenu({ type: 'hide' });
  };

  // Generate radial menu items with angles
  const radialItems: RadialMenuItem[] = DEFAULT_RADIAL_ITEMS.map((item, index) => ({
    ...item,
    angle: (index * 360) / DEFAULT_RADIAL_ITEMS.length - 90, // Start from top
    available: true, // TODO: Filter based on context
  }));

  return (
    <div className="h-screen flex flex-col bg-gray-950 text-white">
      {/* Header */}
      <Header gameState={gameState} onSettingsClick={onLogout} username={username} />

      {/* Main Content */}
      <div className="flex-1 flex gap-2 p-2 overflow-hidden min-h-0">
        {/* Debug Toggle Button */}
        {onToggleDebug && (
          <button
            onClick={onToggleDebug}
            className={`px-3 py-1 rounded text-xs font-bold transition-colors ${
              isDebugEnabled
                ? 'bg-yellow-600 hover:bg-yellow-700 text-white'
                : 'bg-gray-700 hover:bg-gray-600 text-gray-300'
            }`}
            title="Toggle Debug Panel"
          >
            {isDebugEnabled ? '🔧 Debug ON' : '🔧 Debug'}
          </button>
        )}
        {/* Left Sidebar — Team 1 Roster */}
        <TeamRoster
          team={gameState.team1Players}
          teamName={gameState.team1.name}
          teamColor={gameState.team1.color}
          onPlayerSelect={handleRosterPlayerSelect}
          selectedPlayerId={gameState.selectedPlayer?.id}
        />

        {/* Center — Game Field */}
        <div className="relative flex-1 min-h-0">
          <GameField
            team1Players={gameState.team1Players}
            team2Players={gameState.team2Players}
            ballPosition={gameState.ballPosition}
            selectedPlayer={gameState.selectedPlayer}
            onPlayerSelect={handleFieldPlayerSelect}
          />

          {/* Radial Menu (positioned over the selected player) */}
          {radialMenu.visible && radialMenu.player && (
            <div
              className="absolute z-30"
              style={{
                left: `${((radialMenu.position.x + 0.5) / FIELD_WIDTH) * 100}%`,
                top: `${((radialMenu.position.y + 0.5) / FIELD_HEIGHT) * 100}%`,
                transform: 'translate(-50%, -50%)',
              }}
            >
              <RadialMenu
                player={radialMenu.player}
                items={radialItems}
                onActionSelect={handleActionSelect}
                onClose={handleRadialMenuClose}
              />
            </div>
          )}
        </div>

        {/* Right Sidebar — Team 2 Roster */}
        <TeamRoster
          team={gameState.team2Players}
          teamName={gameState.team2.name}
          teamColor={gameState.team2.color}
          onPlayerSelect={handleRosterPlayerSelect}
          selectedPlayerId={gameState.selectedPlayer?.id}
        />
      </div>

      {/* Bottom Bar */}
      <div className="h-36 flex gap-2 p-2">
        <PlayerInfo player={gameState.selectedPlayer} />
        <DiceLog entries={gameState.diceLog} onClear={clearDiceLog} />
        <Chat
          messages={gameState.chatMessages}
          onSend={(text) => {
            const now = Date.now();
            addChatMessage({
              id: generateChatId(),
              sender: 'You',
              senderColor: 'text-blue-400',
              text,
              timestamp: now,
              type: 'general' as const,
            });
          }}
        />
      </div>
    </div>
  );
}