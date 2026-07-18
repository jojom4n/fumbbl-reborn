// =============================================================================
// GameField — 17×10 Blood Bowl field with player tokens
// =============================================================================

import { FieldPosition, Player } from '../../types/bloodbowl';

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
  // Filter active players (on field)
  const team1Active = team1Players.filter(p => p.status === 'active' && p.fieldX !== undefined && p.fieldY !== undefined);
  const team2Active = team2Players.filter(p => p.status === 'active' && p.fieldX !== undefined && p.fieldY !== undefined);

  // Get player at position
  const getPlayerAt = (x: number, y: number): Player | undefined => {
    return [...team1Active, ...team2Active].find(p => p.fieldX === x && p.fieldY === y);
  };


  return (
    <div className="relative bg-gray-900 rounded-lg overflow-hidden border border-gray-700 w-full h-full">
      {/* Field background */}
      <div className="absolute inset-0 bg-linear-to-b from-green-900 via-green-800 to-green-900" />

      {/* Field grid - 17x10 fills available space */}
      <div className="absolute inset-2">
        {/* 17×10 Grid - uses aspect-ratio on container for correct proportions */}
        <div
          className="relative w-full h-full grid grid-cols-17 grid-rows-10 gap-px"
          style={{ aspectRatio: '17/10' }}
        >
          {Array.from({ length: 10 }).map((_, row) =>
            Array.from({ length: 17 }).map((_, col) => {
              const player = getPlayerAt(col, row);
              const isBall = ballPosition.x === col && ballPosition.y === row;
              const playerTeam = player?.race === 'Orc' ? 'team1' : 'team2';

              return (
                <div
                  key={`${col}-${row}`}
                  className={`
                    relative border border-green-700/30 flex items-center justify-center
                    ${col % 2 === 0 ? 'bg-green-900/40' : 'bg-green-800/40'}
                    ${col === 0 || col === 16 ? 'bg-green-950/60' : ''}
                    ${row === 0 || row === 9 ? 'bg-green-950/60' : ''}
                    ${row === 4 || row === 5 ? 'border-green-600/50' : ''}
                  `}
                >
                  {/* End zone labels */}
                  {row === 0 && col >= 6 && col <= 10 && (
                    <span className="absolute top-0.5 left-1/2 -translate-x-1/2 text-[8px] text-green-600/50 font-bold">
                      EZ
                    </span>
                  )}
                  {row === 9 && col >= 6 && col <= 10 && (
                    <span className="absolute bottom-0.5 left-1/2 -translate-x-1/2 text-[8px] text-green-600/50 font-bold">
                      EZ
                    </span>
                  )}

                  {/* 10 yard line markers */}
                  {col === 5 && (
                    <div className="absolute left-0 top-0 bottom-0 w-0.5 bg-yellow-600/30" />
                  )}
                  {col === 11 && (
                    <div className="absolute left-0 top-0 bottom-0 w-0.5 bg-yellow-600/30" />
                  )}

                  {/* Player token */}
                  {player && (
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        onPlayerSelect(player);
                      }}
                      className={`
                        relative w-8 h-8 rounded-full border-2 flex items-center justify-center
                        text-xs font-bold transition-all duration-150 z-10
                        ${playerTeam === 'team1'
                          ? 'bg-green-700 border-green-400 text-white hover:bg-green-600'
                          : 'bg-yellow-600 border-yellow-400 text-white hover:bg-yellow-500'
                        }
                        ${selectedPlayer?.id === player.id
                          ? 'ring-2 ring-white ring-offset-1 ring-offset-green-900 scale-110'
                          : ''
                        }
                        ${player.status === 'injured' ? 'opacity-60' : ''}
                        shadow-lg hover:shadow-xl hover:scale-110
                      `}
                    >
                      {player.number}
                      {/* Status indicator */}
                      {player.status === 'injured' && (
                        <span className="absolute -top-1 -right-1 w-3 h-3 bg-red-500 rounded-full text-[8px] flex items-center justify-center">
                          !
                        </span>
                      )}
                      {/* Ball carrier indicator */}
                      {player.hasBall && (
                        <span className="absolute -bottom-0.5 -right-0.5 w-3 h-3 bg-orange-400 rounded-full border border-orange-200" />
                      )}
                    </button>
                  )}

                  {/* Ball token */}
                  {!player && isBall && (
                    <div className="w-4 h-4 rounded-full bg-orange-400 border-2 border-orange-200 shadow-lg z-10" />
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
              if (x > 16) return null;
              return (
                <div
                  key={`range-${i}`}
                  className="absolute w-2 h-2 bg-white/20 rounded-full"
                  style={{
                    left: `${(x / 17) * 100}%`,
                    top: `${(((selectedPlayer.fieldY as number) + 0.5) / 10) * 100}%`,
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