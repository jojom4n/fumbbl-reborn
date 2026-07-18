// =============================================================================
// PlayerInfo — Player stats card for bottom bar
// =============================================================================

import { Player } from '../../types/bloodbowl';

interface PlayerInfoProps {
  player: Player | null;
}

// Skill full name mapping
const SKILL_FULL_NAMES: Record<string, string> = {
  AG: 'Dodge',
  D: 'Dodge',
  B: 'Block',
  ST: 'Mighty Blows',
  C: 'Catch',
  AN: 'Anchor',
  NK: 'No Kicks Left',
  SV: 'Sure Hands',
  K: 'Keeper',
  T: 'Tackle',
  M: 'Move',
  S: 'Secure Ball',
  L: 'Blitz',
  H: 'Hand-Off',
  P: 'Pass',
  F: 'Foul',
  E: 'Special',
  AP: 'Assassin',
  PD: 'Piledriver',
  RH: 'Rags to Riches',
  W: 'Wicked Claw',
  UF: 'Unfailing',
  FF: 'Frenzy of the Fallen',
  KV: 'Killer Hand',
  SS: 'Stiff Spring',
  CL: 'Claw',
  SP: 'Special',
  EX: 'Extra',
  BD: 'Bloodlust',
  BT: "Brawler's Tenacity",
  DR: 'Dark Rituals',
  FB: 'Frenzy',
  GW: "Guardian's Ward",
  HC: 'Hard Contact',
  IW: 'Iron Will',
  JC: "Juggernaut's Constitution",
  LM: 'Lifeblood',
  MA: 'Magic Resistance',
  NC: 'Never Catch',
  OT: 'On the Ball',
  PA: 'Pass',
  PK: 'Pick Up',
  RC: 'Raging Catch',
  SD: 'Sure Hands Dodge',
  SK: 'Sure Hands Kick',
  TB: 'Tough Bastard',
  UK: 'Unkillable',
  VA: 'Vitality',
  WD: 'Wound Duration',
};

export default function PlayerInfo({ player }: PlayerInfoProps) {
  if (!player) {
    return (
      <div className="w-1/3 bg-gray-900 border-r border-gray-700 flex items-center justify-center">
        <div className="text-center text-gray-500">
          <div className="text-4xl mb-2">👤</div>
          <div className="text-sm">Select a player</div>
        </div>
      </div>
    );
  }

  // Status color mapping
  const STATUS_COLORS: Record<string, string> = {
    active: 'text-green-400',
    injured: 'text-red-400',
    dead: 'text-gray-500',
    rotd: 'text-yellow-400',
    doubtful: 'text-orange-400',
    missing: 'text-gray-500',
  };

  const statusColor = STATUS_COLORS[player.status] || STATUS_COLORS.active;
  const statusLabel = player.status.charAt(0).toUpperCase() + player.status.slice(1);

  return (
    <div className="w-1/3 bg-gray-900 border-r border-gray-700 flex flex-col rounded-lg overflow-hidden">
      {/* Player Header */}
      <div className="px-3 py-2 border-b border-gray-700 flex items-center gap-3">
        {/* Portrait placeholder */}
        <div
          className="w-12 h-12 rounded-lg border-2 flex items-center justify-center text-xl font-bold"
          style={{
            backgroundColor: player.race === 'Orc' ? '#4a7c3f' : '#c4a35a',
            borderColor: player.race === 'Orc' ? '#6abf5e' : '#e8d490',
          }}
        >
          {player.name.charAt(0)}
        </div>

        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2">
            <span className="text-sm font-bold text-white truncate">{player.name}</span>
            <span className={`text-[10px] font-medium ${statusColor}`}>{statusLabel}</span>
          </div>
          <span className="text-[10px] text-gray-400">
            #{player.number} · {player.race} · {player.position.toUpperCase()}
          </span>
        </div>
      </div>

      {/* Stats */}
      <div className="px-3 py-2 border-b border-gray-800">
        <div className="grid grid-cols-5 gap-2">
          {/* MA */}
          <div className="flex flex-col items-center bg-gray-800 rounded px-2 py-1">
            <span className="text-[9px] text-gray-400 uppercase">MA</span>
            <span className="text-sm font-bold text-white">{player.ma}</span>
          </div>
          {/* ST */}
          <div className="flex flex-col items-center bg-gray-800 rounded px-2 py-1">
            <span className="text-[9px] text-gray-400 uppercase">ST</span>
            <span className="text-sm font-bold text-white">{player.st}</span>
          </div>
          {/* AG */}
          <div className="flex flex-col items-center bg-gray-800 rounded px-2 py-1">
            <span className="text-[9px] text-gray-400 uppercase">AG</span>
            <span className="text-sm font-bold text-white">{player.ag}</span>
          </div>
          {/* PA */}
          <div className="flex flex-col items-center bg-gray-800 rounded px-2 py-1">
            <span className="text-[9px] text-gray-400 uppercase">PA</span>
            <span className="text-sm font-bold text-white">{player.pa}</span>
          </div>
          {/* AV */}
          <div className="flex flex-col items-center bg-gray-800 rounded px-2 py-1">
            <span className="text-[9px] text-gray-400 uppercase">AV</span>
            <span className="text-sm font-bold text-white">{player.av}</span>
          </div>
        </div>
      </div>

      {/* Skills */}
      <div className="flex-1 px-3 py-2 overflow-y-auto">
        <span className="text-[10px] text-gray-400 uppercase tracking-wider">Skills</span>
        <div className="flex flex-wrap gap-1 mt-1">
          {player.skills.length > 0 ? (
            player.skills.map((skill, i) => (
              <div
                key={i}
                className="px-2 py-0.5 bg-gray-800 rounded text-[10px] font-medium text-gray-300 border border-gray-700"
                title={SKILL_FULL_NAMES[skill] || skill}
              >
                {skill}
              </div>
            ))
          ) : (
            <span className="text-xs text-gray-500 italic">No skills</span>
          )}
        </div>
      </div>
    </div>
  );
}