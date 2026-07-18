// =============================================================================
// Header — Top bar with team logos, score, turn, re-rolls, timer, weather, Fan Attendance
// =============================================================================

import { Settings } from 'lucide-react';
import FanAttendance from './FanAttendance';
import { GameState } from '../../types/bloodbowl';

interface HeaderProps {
  gameState: GameState;
  onSettingsClick?: () => void;
}

export default function Header({ gameState, onSettingsClick }: HeaderProps) {
  const { score, turn, reRolls, timer, weather, fanAttendance, team1, team2 } = gameState;

  // Format timer as MM:SS
  const formatTimer = (seconds: number): string => {
    const m = Math.floor(seconds / 60);
    const s = seconds % 60;
    return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
  };

  return (
    <header className="h-16 bg-linear-to-b from-gray-900 via-gray-800 to-gray-900 border-b border-gray-700 flex items-center px-4 gap-4">
      {/* Team 1 Logo + Score */}
      <div className="flex items-center gap-3">
        <div className="w-10 h-10 rounded-full bg-green-700 border-2 border-green-400 flex items-center justify-center text-sm font-bold text-white shadow-lg">
          {team1.name.charAt(0)}
        </div>
        <span className="text-xl font-bold text-white">{score.team1}</span>
        <span className="text-lg text-gray-400 font-light">-</span>
        <span className="text-xl font-bold text-white">{score.team2}</span>
        <div className="w-10 h-10 rounded-full bg-yellow-600 border-2 border-yellow-400 flex items-center justify-center text-sm font-bold text-white shadow-lg">
          {team2.name.charAt(0)}
        </div>
      </div>

      {/* Turn */}
      <div className="flex flex-col items-center px-4 py-1 bg-gray-800 rounded-lg border border-gray-600">
        <span className="text-[10px] uppercase tracking-wider text-gray-400">Turn</span>
        <span className="text-xl font-bold text-white">{turn}</span>
      </div>

      {/* Re-Rolls */}
      <div className="flex items-center gap-3 px-4 py-1 bg-gray-800 rounded-lg border border-gray-600">
        {/* Team 1 Re-Rolls (Gold) */}
        <div className="flex items-center gap-1">
          {Array.from({ length: reRolls.team1 }).map((_, i) => (
            <div key={`r1-${i}`} className="w-4 h-4 rounded-full bg-yellow-400 border border-yellow-200 shadow-sm" />
          ))}
          <span className="text-xs text-gray-400 ml-1">{reRolls.team1}/3</span>
        </div>
        {/* Team 2 Re-Rolls (Green/Elf) */}
        <div className="flex items-center gap-1">
          {Array.from({ length: reRolls.team2 }).map((_, i) => (
            <div key={`r2-${i}`} className="w-4 h-4 rounded-full bg-green-500 border border-green-300 shadow-sm" />
          ))}
          <span className="text-xs text-gray-400 ml-1">{reRolls.team2}/3</span>
        </div>
      </div>

      {/* Timer */}
      <div className="flex items-center gap-2 px-4 py-1 bg-gray-800 rounded-lg border border-gray-600">
        <span className="text-lg">💣</span>
        <span className={`text-lg font-mono font-bold ${timer <= 10 ? 'text-red-400 animate-pulse' : 'text-white'}`}>
          {formatTimer(timer)}
        </span>
      </div>

      {/* Weather */}
      <div className="flex items-center gap-2 px-4 py-1 bg-gray-800 rounded-lg border border-gray-600">
        <span className="text-lg">{weather.icon}</span>
        <span className="text-sm text-gray-300">{weather.description}</span>
      </div>

      {/* Fan Attendance (BB2025) */}
      <FanAttendance
        fanAttendance={fanAttendance}
        team1Name={team1.name}
        team2Name={team2.name}
      />

      {/* Settings Button */}
      <button
        onClick={onSettingsClick}
        className="ml-auto p-2 rounded-lg bg-gray-800 hover:bg-gray-700 border border-gray-600 text-gray-400 hover:text-white transition-colors"
      >
        <Settings size={20} />
      </button>
    </header>
  );
}