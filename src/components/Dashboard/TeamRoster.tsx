// =============================================================================
// TeamRoster — Generic team roster with full player state display
// Shows ALL roster players grouped by status:
//   - On Field (active players with field coordinates)
//   - Reserve (players not yet set up)
//   - Casualty (KO, Badly Hurt, SI, RIP, Banned)
// =============================================================================

import { Player, PlayerStatus } from '../../types/bloodbowl';

interface TeamRosterProps {
  team: Player[];
  teamName: string;
  teamColor: string;
  onPlayerSelect: (player: Player) => void;
  selectedPlayerId?: string | number;
}

// -----------------------------------------------------------------------------
// Skill badge display
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Player status icons and styles
// Mapped from official ffb PlayerState.java
// -----------------------------------------------------------------------------
const STATUS_CONFIG: Record<PlayerStatus, { icon: string; label: string; color: string; bg: string; opacity?: number }> = {
  active: {
    icon: '●',
    label: 'Active',
    color: 'text-green-400',
    bg: 'bg-green-400/10',
  },
  ko: {
    icon: '💫',
    label: 'KO',
    color: 'text-yellow-400',
    bg: 'bg-yellow-400/10',
  },
  badly_hurt: {
    icon: '🤕',
    label: 'Badly Hurt',
    color: 'text-orange-400',
    bg: 'bg-orange-400/10',
    opacity: 0.7,
  },
  si: {
    icon: '🏥',
    label: 'Serious Injury',
    color: 'text-red-400',
    bg: 'bg-red-400/10',
    opacity: 0.6,
  },
  rip: {
    icon: '💀',
    label: 'RIP',
    color: 'text-gray-500',
    bg: 'bg-gray-500/10',
    opacity: 0.5,
  },
  reserve: {
    icon: '🔄',
    label: 'Reserve',
    color: 'text-blue-400',
    bg: 'bg-blue-400/10',
  },
  missing: {
    icon: '—',
    label: 'Missing',
    color: 'text-gray-500',
    bg: 'bg-gray-500/10',
    opacity: 0.5,
  },
  banned: {
    icon: '🚫',
    label: 'Banned',
    color: 'text-red-500',
    bg: 'bg-red-500/10',
    opacity: 0.6,
  },
};

// Status groups for section headers
const STATUS_SECTIONS = [
  { key: 'on-field', label: 'On Field', statuses: ['active'] as PlayerStatus[] },
  { key: 'reserve', label: 'Reserve', statuses: ['reserve'] as PlayerStatus[] },
  { key: 'casualty', label: 'Casualty', statuses: ['ko', 'badly_hurt', 'si', 'rip', 'banned', 'missing'] as PlayerStatus[] },
];

// -----------------------------------------------------------------------------
// Player Row Component
// -----------------------------------------------------------------------------
function PlayerRow({
  player,
  isSelected,
  onSelect,
}: {
  player: Player;
  isSelected: boolean;
  onSelect: (player: Player) => void;
}) {
  const status = STATUS_CONFIG[player.status] || STATUS_CONFIG.active;

  return (
    <button
      onClick={() => onSelect(player)}
      className={`
        w-full flex items-center px-2 py-1.5 border-b border-gray-800
        transition-colors text-left
        ${isSelected
          ? 'bg-gray-700/50 border-l-2 border-l-white'
          : 'hover:bg-gray-800/50 border-l-2 border-l-transparent'
        }
        ${status.opacity !== undefined ? '' : ''}
      `}
      style={{ opacity: status.opacity }}
      title={`${player.name} — ${status.label}`}
    >
      {/* Number */}
      <span className="w-6 text-center text-xs text-gray-400 font-mono">
        {player.number}
      </span>

      {/* Name + Status */}
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-1">
          <span className={`text-[10px] ${status.color}`} title={status.label}>
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
}

// -----------------------------------------------------------------------------
// Section Header Component
// -----------------------------------------------------------------------------
function SectionHeader({ label, count }: { label: string; count: number }) {
  if (count === 0) return null;
  return (
    <div className="flex items-center px-2 py-1 bg-gray-800/30 text-[10px] text-gray-500 uppercase tracking-wider">
      <span className="flex-1">{label}</span>
      <span>{count}</span>
    </div>
  );
}

// -----------------------------------------------------------------------------
// Main TeamRoster Component
// -----------------------------------------------------------------------------
export default function TeamRoster({
  team,
  teamName,
  teamColor,
  onPlayerSelect,
  selectedPlayerId,
}: TeamRosterProps) {
  // Group players by status category
  const grouped: Record<string, Player[]> = {};
  for (const section of STATUS_SECTIONS) {
    grouped[section.key] = team.filter(p => section.statuses.includes(p.status));
  }

  // Count on-field players for header
  const onFieldCount = grouped['on-field'].length;
  const totalCount = team.length;

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
        <span className="text-[10px] text-gray-400 ml-auto">{onFieldCount}/11</span>
      </div>

      {/* Column Headers */}
      <div className="flex items-center px-2 py-1 bg-gray-800/50 border-b border-gray-700 text-[10px] text-gray-400 uppercase tracking-wider">
        <span className="w-6 text-center">#</span>
        <span className="flex-1">Name</span>
        <span className="w-16 text-center">Skills</span>
        <span className="w-5 text-center">St</span>
      </div>

      {/* Player List grouped by status */}
      <div className="flex-1 overflow-y-auto scrollbar-thin scrollbar-thumb-gray-700 scrollbar-track-gray-900">
        {STATUS_SECTIONS.map(section => {
          const players = grouped[section.key];
          if (players.length === 0) return null;

          return (
            <div key={section.key}>
              <SectionHeader label={section.label} count={players.length} />
              {players.map((player) => (
                <PlayerRow
                  key={player.id}
                  player={player}
                  isSelected={String(selectedPlayerId) === String(player.id)}
                  onSelect={onPlayerSelect}
                />
              ))}
            </div>
          );
        })}

        {/* Empty state */}
        {totalCount === 0 && (
          <div className="px-3 py-4 text-center text-xs text-gray-500">
            No players loaded
          </div>
        )}
      </div>
    </div>
  );
}