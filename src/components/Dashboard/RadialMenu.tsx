// =============================================================================
// RadialMenu — Dynamic radial action menu around selected player
// BB2025: 9 official actions arranged in 8 slots + center
// =============================================================================

import { useEffect, useRef } from 'react';
import { BloodBowlAction, Player, RadialMenuItem } from '../../types/bloodbowl';

interface RadialMenuProps {
  player: Player;
  items: RadialMenuItem[];
  onActionSelect: (action: BloodBowlAction) => void;
  onClose: () => void;
}

// Action metadata for display
const ACTION_LABELS: Record<BloodBowlAction, string> = {
  move: 'Move',
  secureBall: 'Secure',
  block: 'Block',
  blitz: 'Blitz',
  pass: 'Pass',
  handOff: 'Hand-Off',
  throwTeamMate: 'Throw',
  foul: 'Foul',
  special: 'Special',
};

const ACTION_HOTKEYS: Record<BloodBowlAction, string> = {
  move: 'M',
  secureBall: 'S',
  block: 'B',
  blitz: 'L',
  pass: 'P',
  handOff: 'H',
  throwTeamMate: 'T',
  foul: 'F',
  special: 'E',
};

export default function RadialMenu({
  player,
  items,
  onActionSelect,
  onClose,
}: RadialMenuProps) {
  const menuRef = useRef<HTMLDivElement>(null);
  const radius = 70; // Distance from center
  const menuItemSize = 36;

  // Close on outside click
  useEffect(() => {
    const handleClick = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        onClose();
      }
    };
    document.addEventListener('mousedown', handleClick);
    return () => document.removeEventListener('mousedown', handleClick);
  }, [onClose]);

  // Keyboard shortcuts
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      const key = e.key.toUpperCase();
      const item = items.find(i => i.hotkey === key && i.available);
      if (item) {
        onActionSelect(item.action);
      }
      if (e.key === 'Escape') {
        onClose();
      }
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [items, onActionSelect, onClose]);

  return (
    <div ref={menuRef} className="absolute z-30">
      {/* Center — Player Token */}
      <div
        className="absolute flex items-center justify-center"
        style={{
          width: 44,
          height: 44,
          left: `calc(50% - 22px)`,
          top: `calc(50% - 22px)`,
        }}
      >
        <div
          className={`
            w-11 h-11 rounded-full border-2 flex items-center justify-center
            text-xs font-bold text-white shadow-xl
            ${player.race === 'Orc'
              ? 'bg-green-700 border-green-400'
              : 'bg-yellow-600 border-yellow-400'
            }
          `}
        >
          {player.number}
        </div>
      </div>

      {/* Action Items */}
      {items.map((item, index) => {
        // Calculate position around the circle
        const angle = (item.angle * Math.PI) / 180;
        const x = Math.cos(angle) * radius;
        const y = Math.sin(angle) * radius;

        return (
          <button
            key={`${item.action}-${index}`}
            onClick={() => item.available && onActionSelect(item.action)}
            disabled={!item.available}
            className={`
              absolute flex flex-col items-center justify-center
              transition-all duration-150
              ${item.available
                ? 'cursor-pointer hover:scale-110'
                : 'cursor-not-allowed opacity-30'
              }
            `}
            style={{
              width: menuItemSize,
              height: menuItemSize,
              left: `calc(50% + ${x}px - ${menuItemSize / 2}px)`,
              top: `calc(50% + ${y}px - ${menuItemSize / 2}px)`,
            }}
          >
            {/* Action button */}
            <div
              className={`
                w-9 h-9 rounded-lg border-2 flex items-center justify-center
                text-xs font-bold shadow-lg transition-colors
                ${item.available
                  ? item.action === 'block'
                    ? 'bg-red-700 border-red-400 text-white hover:bg-red-600'
                    : item.action === 'blitz'
                      ? 'bg-orange-700 border-orange-400 text-white hover:bg-orange-600'
                      : 'bg-gray-700 border-gray-400 text-white hover:bg-gray-600'
                  : 'bg-gray-800 border-gray-600 text-gray-500'
                }
              `}
            >
              {/* Hotkey */}
              <span className="text-[10px]">{ACTION_HOTKEYS[item.action]}</span>
            </div>

            {/* Label */}
            <span className={`
              absolute -bottom-4 text-[9px] font-medium whitespace-nowrap
              ${item.available ? 'text-gray-300' : 'text-gray-600'}
            `}>
              {ACTION_LABELS[item.action]}
            </span>
          </button>
        );
      })}

      {/* Background overlay (dim) */}
      <div className="absolute inset-0 bg-black/20 rounded-full pointer-events-none"
        style={{
          width: radius * 2 + 60,
          height: radius * 2 + 60,
          left: `calc(50% - ${(radius * 2 + 60) / 2}px)`,
          top: `calc(50% - ${(radius * 2 + 60) / 2}px)`,
        }}
      />
    </div>
  );
}