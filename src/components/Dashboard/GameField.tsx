// =============================================================================
// GameField — 26×15 Blood Bowl field with player tokens
// Standard BB field: 26 squares long (X: 0-25) × 15 squares wide (Y: 0-14)
// Box coordinates (off-field): X=-1(reserve), X=-2(ko), X=-3(bh), X=-4(si),
//   X=-5(rip), X=-6(banned), X=-7(missing), X=30-36(same for away team)
// =============================================================================

import { FieldPosition, Player } from '../../types/bloodbowl';

// Blood Bowl field dimensions (matching server FieldCoordinate.java)
const FIELD_WIDTH = 26;  // X: 0-25
const FIELD_HEIGHT = 15; // Y: 0-14

/**
 * Check if a coordinate is a "box" coordinate (off-field).
 * Copied from FieldCoordinate.isBoxCoordinate() in ffb-common.
 * Box coordinates: X in {-1,-2,-3,-4,-5,-6,-7, 30,31,32,33,34,35,36}
 */
function isBoxCoordinate(x: number, _y?: number): boolean {
  return x === -1 || x === -2 || x === -3 || x === -4 || x === -5 ||
         x === -6 || x === -7 || x >= 30;
}

/**
 * Check if a player is on the field (not in box coordinates).
 */
function isPlayerOnField(p: Player): boolean {
  if (p.fieldX === undefined || p.fieldY === undefined) return false;
  return !isBoxCoordinate(p.fieldX, p.fieldY);
}

interface GameFieldProps {
  team1Players: Player[];
  team2Players: Player[];
  ballPosition: FieldPosition;
  selectedPlayer: Player | null;
  onPlayerSelect: (player: Player) => void;
  onFieldClick?: (position: FieldPosition) => void;
}

export default function GameField({
  team1Players,
  team2Players,
  ballPosition,
  selectedPlayer,
  onPlayerSelect,
}: GameFieldProps) {
  // Filter players that are on the field (not in box coordinates)
  // Players with box coordinates are in reserve/KO/casualty and should NOT render on field
  const team1Active = team1Players.filter(isPlayerOnField);
  const team2Active = team2Players.filter(isPlayerOnField);

  // Get player at position
  const getPlayerAt = (x: number, y: number): Player | undefined => {
    return [...team1Active, ...team2Active].find(p => p.fieldX === x && p.fieldY === y);
  };


  return (
    <div className="relative bg-gray-900 rounded-lg overflow-hidden border border-gray-700 w-full h-full">
      {/* Field background */}
      <div className="absolute inset-0 bg-linear-to-b from-green-900 via-green-800 to-green-900" />

      {/* Field grid - 26x15 Blood Bowl field */}
      <div className="absolute inset-2">
        {/* 26×15 Grid - standard Blood Bowl field dimensions */}
        <div
          className="relative w-full h-full grid gap-px"
          style={{
            gridTemplateColumns: `repeat(${FIELD_WIDTH}, 1fr)`,
            gridTemplateRows: `repeat(${FIELD_HEIGHT}, 1fr)`,
            aspectRatio: `${FIELD_WIDTH}/${FIELD_HEIGHT}`,
          }}
        >
          {Array.from({ length: FIELD_HEIGHT }).map((_, row) =>
            Array.from({ length: FIELD_WIDTH }).map((_, col) => {
              const player = getPlayerAt(col, row);
              // Only show ball when it's on the field (valid coordinates, not in box)
              const isBall = (
                !isBoxCoordinate(ballPosition.x, ballPosition.y) &&
                ballPosition.x === col && ballPosition.y === row
              );
              // Determine team by checking which team's player list contains this player
              const team1Ids = new Set(team1Players.map(p => String(p.id)));
              const playerTeam = team1Ids.has(String(player?.id)) ? 'team1' : 'team2';

              // Field markings
              const isEndZone = col <= 1 || col >= 24;
              const isHomeEndZone = col <= 1;
              const isAwayEndZone = col >= 24;
              const isCenterLine = col === 12 || col === 13;
              const isSideline = row === 0 || row === FIELD_HEIGHT - 1;

              return (
                <div
                  key={`${col}-${row}`}
                  className={`
                    relative border border-green-700/20 flex items-center justify-center
                    ${(col + row) % 2 === 0 ? 'bg-green-900/40' : 'bg-green-800/40'}
                    ${isEndZone ? 'bg-green-950/50' : ''}
                    ${isSideline ? 'border-green-600/40' : ''}
                  `}
                >
                  {/* End zone labels */}
                  {row === Math.floor(FIELD_HEIGHT / 2) && isHomeEndZone && (
                    <span className="absolute top-0.5 left-1/2 -translate-x-1/2 text-[6px] text-green-500/40 font-bold">
                      EZ
                    </span>
                  )}
                  {row === Math.floor(FIELD_HEIGHT / 2) && isAwayEndZone && (
                    <span className="absolute top-0.5 left-1/2 -translate-x-1/2 text-[6px] text-green-500/40 font-bold">
                      EZ
                    </span>
                  )}

                  {/* Center line */}
                  {isCenterLine && (
                    <div className="absolute top-0 bottom-0 w-0.5 bg-yellow-600/20" />
                  )}

                  {/* Player token */}
                  {player && (
                    <button
                      key={`player-btn-${player.id}`}
                      onClick={(e) => {
                        e.stopPropagation();
                        onPlayerSelect(player);
                      }}
                      className={`
                        relative w-5 h-5 rounded-full border flex items-center justify-center
                        text-[8px] font-bold transition-all duration-150 z-10
                        ${playerTeam === 'team1'
                          ? 'bg-green-700 border-green-400 text-white hover:bg-green-600'
                          : 'bg-yellow-600 border-yellow-400 text-white hover:bg-yellow-500'
                        }
                        ${selectedPlayer && String(selectedPlayer.id) === String(player.id)
                          ? 'ring-2 ring-white ring-offset-1 ring-offset-green-900 scale-125'
                          : ''
                        }
                        ${player.status === 'ko' ? 'opacity-50' : ''}
                        shadow-lg hover:shadow-xl hover:scale-125
                      `}
                    >
                      {player.number}
                      {/* Ball carrier indicator */}
                      {player.hasBall && (
                        <span className="absolute -top-1 -right-1 text-[8px] drop-shadow-lg" title="Has ball">
                          🏈
                        </span>
                      )}
                    </button>
                  )}

                  {/* Ball token — visible when ball is on a square without a player */}
                  {!player && isBall && (
                    <div className="flex flex-col items-center justify-center z-10">
                      <span className="text-xs drop-shadow-lg">🏈</span>
                    </div>
                  )}
                </div>
              );
            })
          )}
        </div>
      </div>

      {/* Field overlay for action arrows (placeholder) */}
      {selectedPlayer && selectedPlayer.fieldX !== undefined && selectedPlayer.fieldY !== undefined && (
        <div className="absolute inset-0 pointer-events-none z-20">
          {/* Movement range indicators */}
          <>
            {Array.from({ length: selectedPlayer.ma }).map((_, i) => {
              const x = (selectedPlayer.fieldX as number) + i + 1;
              if (x >= FIELD_WIDTH) return null;
              return (
                <div
                  key={`range-${i}`}
                  className="absolute w-2 h-2 bg-white/20 rounded-full"
                  style={{
                    left: `${(x / FIELD_WIDTH) * 100}%`,
                    top: `${(((selectedPlayer.fieldY as number) + 0.5) / FIELD_HEIGHT) * 100}%`,
                    transform: 'translate(-50%, -50%)',
                  }}
                />
              );
            })}
          </>
        </div>
      )}
    </div>
  );
}
