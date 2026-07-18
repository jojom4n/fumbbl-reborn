// =============================================================================
// TeamRoster — Generic team roster (position-independent)
// Used for both left and right sidebars
// =============================================================================

import { Player } from '../../types/bloodbowl';

interface TeamRosterProps {
  team: Player[];
  teamName: string;
  teamColor: string;
  onPlayerSelect: (player: Player) => void;
  selectedPlayerId?: number;
}

// Skill badge display
const SKILL_DISPLAY: Record<string, { label: string; color: string }> = {
  AG: { label: 'D', color: 'text-green-300' },
  D: { label: 'D', color: 'text-green-300' },
  B: { label: 'B', color: 'text-yellow-300' },
  ST: { label: 'ST', color: 'text-red-300' },
  C: { label: 'C', color: 'text-blue-300' },
  AN: { label: 'AN', color: 'text-purple-300' },
  NK: { label: 'NK', color: 'text-gray-400' },
  SV: { label: 'SH', color: 'text-cyan-300' },
  K: { label: 'K', color: 'text-orange-300' },
  T: { label: 'T', color: 'text-red-300' },
};

// Status icon mapping
const STATUS_ICONS: Record<string, { icon: string; bg: string }> = {
  active: { icon: '●', bg: 'text-green-400' },
  injured: { icon: '✕', bg: 'text-red-400' },
  dead: { icon: '☠', bg: 'text-gray-500' },
  rotd: { icon: 'D', bg: 'text-yellow-400' },
  doubtful: { icon: '?', bg: 'text-orange-400' },
  missing: { icon: '—', bg: 'text-gray-500' },
};

export default function TeamRoster({
  team,
  teamName,
  teamColor,
  onPlayerSelect,
  selectedPlayerId,
}: TeamRosterProps) {
  // Filter active players for main list
  const activePlayers = team.filter(p => p.status === 'active' && p.position !== 'sub' && p.position !== 'coach');
  const subPlayers = team.filter(p => p.position === 'sub');

  return (
    <div className="w-52 bg-gray-900 border-r border-gray-700 flex flex-col rounded-lg overflow-hidden">
      {/* Team Header */}
      <div
        className="px-3 py-2 border-b border-gray-700 flex items-center gap-2"
        style={{ backgroundColor: `${teamColor}30` }}
      >
        <div
          className="w-5 h-5 rounded-full border border-white/50"
          style={{ backgroundColor: teamColor }}
        />
        <span className="text-sm font-bold text-white">{teamName}</span>
        <span className="text-[10px] text-gray-400 ml-auto">{activePlayers.length}/11</span>
      </div>

      {/* Column Headers */}
      <div className="flex items-center px-2 py-1 bg-gray-800/50 border-b border-gray-700 text-[10px] text-gray-400 uppercase tracking-wider">
        <span className="w-6 text-center">#</span>
        <span className="flex-1">Name</span>
        <span className="w-16 text-center">Skills</span>
        <span className="w-5 text-center">St</span>
      </div>

      {/* Player List */}
      <div className="flex-1 overflow-y-auto scrollbar-thin scrollbar-thumb-gray-700 scrollbar-track-gray-900">
        {activePlayers.map((player) => {
          const status = STATUS_ICONS[player.status] || STATUS_ICONS.active;
          const isSelected = selectedPlayerId === player.id;

          return (
            <button
              key={player.id}
              onClick={() => onPlayerSelect(player)}
              className={`
                w-full flex items-center px-2 py-1.5 border-b border-gray-800
                transition-colors text-left
                ${isSelected
                  ? 'bg-gray-700/50 border-l-2 border-l-white'
                  : 'hover:bg-gray-800/50 border-l-2 border-l-transparent'
                }
                ${player.status === 'injured' ? 'opacity-60' : ''}
              `}
            >
              {/* Number */}
              <span className="w-6 text-center text-xs text-gray-400 font-mono">
                {player.number}
              </span>

              {/* Name + Status */}
              <div className="flex-1 min-w-0">
                <div className="flex items-center gap-1">
                  <span className={`text-xs font-medium truncate ${status.bg}`}>
                    {status.icon}
                  </span>
                  <span className="text-xs text-white truncate">{player.name}</span>
                </div>
                {/* Skills */}
                <div className="flex items-center gap-0.5 flex-wrap">
                  {player.skills.slice(0, 3).map((skill, i) => (
                    <span
                      key={i}
                      className={`text-[9px] font-bold ${SKILL_DISPLAY[skill]?.color || 'text-gray-400'}`}
                    >
                      {SKILL_DISPLAY[skill]?.label || skill}
                    </span>
                  ))}
                </div>
              </div>

              {/* Strength */}
              <span className="w-5 text-center text-[10px] text-gray-400">
                {player.st}
              </span>
            </button>
          );
        })}

        {/* Substitutes Section */}
        {subPlayers.length > 0 && (
          <>
            <div className="flex items-center px-2 py-1 bg-gray-800/30 text-[10px] text-gray-500 uppercase tracking-wider">
              <span className="flex-1">Substitutes</span>
            </div>
            {subPlayers.map((player) => (
              <button
                key={player.id}
                onClick={() => onPlayerSelect(player)}
                className={`
                  w-full flex items-center px-2 py-1.5 border-b border-gray-800
                  transition-colors text-left hover:bg-gray-800/30
                  ${selectedPlayerId === player.id
                    ? 'bg-gray-700/30 border-l-2 border-l-gray-400'
                    : 'border-l-2 border-l-transparent'
                  }
                `}
              >
                <span className="w-6 text-center text-xs text-gray-500 font-mono">
                  {player.number}
                </span>
                <span className="flex-1 text-xs text-gray-400 truncate">
                  {player.name}
                </span>
                <span className="w-5 text-center text-[10px] text-gray-500">
                  {player.st}
                </span>
              </button>
            ))}
          </>
        )}
      </div>
    </div>
  );
}