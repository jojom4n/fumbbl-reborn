// =============================================================================
// DashboardLayout — Fixed layout container for the FUMBBL Reborn Dashboard
// This is the ONLY component that knows where each piece is placed
// =============================================================================

import { useReducer } from 'react';
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
  Player,
  RadialMenuItem,
} from '../../types/bloodbowl';

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

export default function DashboardLayout() {
  const { gameState, selectPlayer, sendAction, addChatMessage, clearDiceLog } = useGameState();

  // Radial menu state — stores field position for correct placement
  type RadialAction =
    | { type: 'show'; player: Player; fieldX: number; fieldY: number }
    | { type: 'hide' }
    | { type: 'select'; action: BloodBowlAction };

  const [radialMenu, setRadialMenu] = useReducer(
    (state: { visible: boolean; player: Player | null; fieldX: number; fieldY: number }, action: RadialAction) => {
      switch (action.type) {
        case 'show':
          return { visible: true, player: action.player, fieldX: action.fieldX, fieldY: action.fieldY };
        case 'hide':
          return { visible: false, player: null, fieldX: 0, fieldY: 0 };
        case 'select':
          // Handle action selection (radial menu stays open for multiple actions)
          return state;
        default:
          return state;
      }
    },
    { visible: false, player: null, fieldX: 0, fieldY: 0 } as { visible: boolean; player: Player | null; fieldX: number; fieldY: number }
  );

  // Handle player selection from field or roster
  const handlePlayerSelect = (player: Player) => {
    selectPlayer(player);
    // Use field position if available, otherwise default to player's number-based position
    const fieldX = player.fieldX ?? 0;
    const fieldY = player.fieldY ?? 0;
    setRadialMenu({ type: 'show', player, fieldX, fieldY });
  };

  // Handle radial menu action
  const handleActionSelect = (action: BloodBowlAction) => {
    if (radialMenu.player) {
      sendAction(action, radialMenu.player.id);
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
      <Header gameState={gameState} />

      {/* Main Content */}
      <div className="flex-1 flex gap-2 p-2 overflow-hidden min-h-0">
        {/* Left Sidebar — Team 1 Roster */}
        <TeamRoster
          team={gameState.team1Players}
          teamName={gameState.team1.name}
          teamColor={gameState.team1.color}
          onPlayerSelect={handlePlayerSelect}
          selectedPlayerId={gameState.selectedPlayer?.id}
        />

        {/* Center — Game Field */}
        <div className="relative flex-1 min-h-0">
          <GameField
            team1Players={gameState.team1Players}
            team2Players={gameState.team2Players}
            ballPosition={gameState.ballPosition}
            selectedPlayer={gameState.selectedPlayer}
            onPlayerSelect={handlePlayerSelect}
          />

          {/* Radial Menu (positioned over the selected player) */}
          {radialMenu.visible && radialMenu.player && (
            <div
              className="absolute z-30"
              style={{
                left: `${((radialMenu.fieldX + 0.5) / 17) * 100}%`,
                top: `${((radialMenu.fieldY + 0.5) / 10) * 100}%`,
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
          onPlayerSelect={handlePlayerSelect}
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
            addChatMessage({
              id: Date.now(),
              sender: 'You',
              senderColor: 'text-blue-400',
              text,
              timestamp: Date.now(),
              type: 'general',
            });
          }}
        />
      </div>
    </div>
  );
}