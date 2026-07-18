// =============================================================================
// FanAttendance — Fan Attendance + Dedicated Fans display
// BB2025 terminology: "Fan Attendance" (NOT "Fan Factor")
// =============================================================================

import { FanAttendance as FanAttendanceType } from '../../types/bloodbowl';

interface FanAttendanceProps {
  fanAttendance: FanAttendanceType;
  team1Name: string;
  team2Name: string;
  onFanRoll?: () => void;
}

export default function FanAttendance({
  fanAttendance,
  team1Name,
  team2Name,
}: FanAttendanceProps) {
  const { total, dedicatedFans } = fanAttendance;

  return (
    <div className="flex items-center gap-4 px-4">
      {/* Fan Attendance Display */}
      <div className="flex items-center gap-2">
        {/* Fan icon */}
        <div className="text-2xl">👥</div>
        <div className="flex flex-col">
          <span className="text-[10px] uppercase tracking-wider text-gray-400 font-semibold">
            Fan Attendance
          </span>
          <span className="text-2xl font-bold text-white leading-none">{total}</span>
        </div>
      </div>

      {/* Dedicated Fans Breakdown */}
      <div className="flex items-center gap-3 pl-3 border-l border-gray-600">
        {/* Team 1 Dedicated Fans */}
        <div className="flex items-center gap-1.5">
          <div className="w-3 h-3 rounded-full bg-green-600 border border-green-400" />
          <span className="text-xs text-gray-300">{team1Name}:</span>
          <span className="text-sm font-bold text-white">{dedicatedFans.team1}</span>
        </div>

        {/* Team 2 Dedicated Fans */}
        <div className="flex items-center gap-1.5">
          <div className="w-3 h-3 rounded-full bg-yellow-600 border border-yellow-400" />
          <span className="text-xs text-gray-300">{team2Name}:</span>
          <span className="text-sm font-bold text-white">{dedicatedFans.team2}</span>
        </div>
      </div>
    </div>
  );
}