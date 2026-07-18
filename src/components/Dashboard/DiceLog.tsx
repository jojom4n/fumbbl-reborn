// =============================================================================
// DiceLog — Dice log panel for bottom bar
// =============================================================================

import { DiceLogEntry } from '../../types/bloodbowl';

interface DiceLogProps {
  entries: DiceLogEntry[];
  onClear?: () => void;
}

export default function DiceLog({ entries, onClear }: DiceLogProps) {
  return (
    <div className="w-1/3 bg-gray-900 border-r border-gray-700 flex flex-col rounded-lg overflow-hidden">
      {/* Header */}
      <div className="px-3 py-2 border-b border-gray-700 flex items-center justify-between">
        <div className="flex items-center gap-2">
          <span className="text-lg">🎲</span>
          <span className="text-sm font-bold text-white">Dice Log</span>
          <span className="text-[10px] text-gray-500">({entries.length})</span>
        </div>
        {entries.length > 0 && (
          <button
            onClick={onClear}
            className="text-[10px] text-gray-500 hover:text-gray-300 transition-colors"
          >
            Clear
          </button>
        )}
      </div>

      {/* Entries */}
      <div className="flex-1 overflow-y-auto scrollbar-thin scrollbar-thumb-gray-700 scrollbar-track-gray-900 p-2 space-y-1">
        {entries.length > 0 ? (
          [...entries].reverse().map((entry) => (
            <div
              key={entry.id}
              className={`text-xs px-2 py-1 rounded border-l-2 ${
                entry.type === 'casualty'
                  ? 'bg-red-900/30 border-l-red-500'
                  : entry.result === 'success'
                    ? 'bg-green-900/20 border-l-green-500'
                    : entry.type === 'block_roll' || entry.type === 'armor'
                      ? 'bg-yellow-900/20 border-l-yellow-500'
                      : 'bg-gray-800/50 border-l-gray-600'
              }`}
            >
              {/* Turn indicator */}
              {entry.turn && (
                <span className="text-[10px] text-gray-500 mr-1">
                  [T{entry.turn}]
                </span>
              )}

              {/* Main text */}
              <span className={entry.color || 'text-gray-300'}>
                {entry.text}
              </span>

              {/* Dice display */}
              {entry.dice && entry.dice.length > 0 && (
                <div className="flex items-center gap-1 mt-0.5">
                  {entry.dice.map((die, i) => (
                    <span
                      key={i}
                      className={`
                        text-[10px] font-mono px-1 py-0.5 rounded
                        ${die === 6 ? 'bg-green-600/30 text-green-300' : ''}
                        ${die === 1 ? 'bg-red-600/30 text-red-300' : ''}
                        ${![1, 6].includes(die) ? 'bg-gray-700/50 text-gray-400' : ''}
                      `}
                    >
                      {die}
                    </span>
                  ))}
                  {entry.target && (
                    <span className="text-[10px] text-gray-500 ml-1">
                      vs {entry.target}
                    </span>
                  )}
                </div>
              )}
            </div>
          ))
        ) : (
          <div className="text-center text-gray-500 py-4 text-xs">
            No dice rolls yet
          </div>
        )}
      </div>
    </div>
  );
}