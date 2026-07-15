import React, { useState } from 'react';
import { MessageSquare, Dice5, Activity, Shield, Users, Settings, ChevronRight, Search, Send } from 'lucide-react';

// --- TIPI (Mockup Dati) ---
interface Player {
  id: number;
  name: string;
  number: number;
  status: 'active' | 'injured' | 'dead' | 'rotd';
  skills: string[];
  position: string;
}

const TEAM_1_PLAYERS: Player[] = [
  { id: 1, name: 'Player #1', number: 1, status: 'active', skills: [], position: 'AP' },
  { id: 2, name: 'Player #2', number: 2, status: 'active', skills: ['AG'], position: 'AP' },
  { id: 3, name: 'Player #3', number: 3, status: 'active', skills: [], position: 'BL' },
  { id: 4, name: 'Player #4', number: 4, status: 'active', skills: ['ST'], position: 'BL' },
  { id: 5, name: 'Player #5', number: 5, status: 'injured', skills: [], position: 'BL' },
  { id: 6, name: 'Player #6', number: 6, status: 'active', skills: ['T'], position: 'BL' },
  { id: 7, name: 'Player #7', number: 7, status: 'active', skills: [], position: 'BL' },
  { id: 8, name: 'Player #8', number: 8, status: 'active', skills: ['AG'], position: 'BL' },
  { id: 9, name: 'Lineman', number: 9, status: 'active', skills: [], position: 'BL' },
  { id: 10, name: 'Lineman', number: 10, status: 'active', skills: [], position: 'BL' },
  { id: 11, name: 'Lineman', number: 11, status: 'active', skills: [], position: 'BL' },
  { id: 12, name: 'Lineman', number: 12, status: 'active', skills: [], position: 'BL' },
  { id: 13, name: 'Stumen', number: 13, status: 'active', skills: [], position: 'ST' },
  { id: 14, name: 'Stumen', number: 14, status: 'active', skills: [], position: 'ST' },
  { id: 15, name: 'Lineman', number: 15, status: 'active', skills: [], position: 'BL' },
  { id: 16, name: 'Stumen', number: 16, status: 'active', skills: [], position: 'ST' },
];

const TEAM_2_PLAYERS: Player[] = [
  { id: 101, name: 'Elf Player', number: 1, status: 'active', skills: ['AG', 'D'], position: 'AP' },
  { id: 102, name: 'Elf Player', number: 2, status: 'active', skills: [], position: 'AP' },
  { id: 103, name: 'Elf Lineman', number: 3, status: 'active', skills: [], position: 'BL' },
  // ... altri giocatori
];

// --- COMPONENTI UI ---

const StatusIcon = ({ status }: { status: string }) => {
  // Placeholder per le icone di status (feriti, attivi, ecc.)
  if (status === 'injured') return <div className="w-3 h-3 bg-red-500 rounded-full" title="Injured" />;
  if (status === 'dead') return <div className="w-3 h-3 bg-red-800 rounded-full" title="Dead" />;
  return <div className="w-3 h-3 bg-green-500 rounded-full" title="Active" />;
};

const SkillIcon = ({ skill }: { skill: string }) => (
  <span className="text-[10px] bg-yellow-600 text-white px-1 rounded border border-yellow-400 mr-1">
    {skill}
  </span>
);

const PlayerRow = ({ player }: { player: Player }) => (
  <div className="flex items-center justify-between p-1 hover:bg-white/10 cursor-pointer border-b border-white/5 text-xs">
    <div className="flex items-center gap-2">
      <span className="text-gray-400 w-4 text-right">{player.number}</span>
      <span className="text-white font-medium">{player.name}</span>
    </div>
    <div className="flex items-center gap-2">
      <div className="flex gap-1">
        {player.skills.map(s => <SkillIcon key={s} skill={s} />)}
      </div>
      <StatusIcon status={player.status} />
    </div>
  </div>
);

const Header = () => (
  <header className="h-16 bg-[#1a1a1a] border-b border-white/10 flex items-center justify-between px-4 shrink-0">
    {/* Squadre e Punteggio */}
    <div className="flex items-center gap-4 w-1/4">
      <div className="flex items-center gap-2">
        <div className="w-10 h-10 bg-green-800 rounded-full border-2 border-white flex items-center justify-center text-xs font-bold">Orc</div>
        <span className="text-white font-bold text-lg">2</span>
      </div>
      <span className="text-2xl text-gray-500 font-bold">-</span>
      <div className="flex items-center gap-2">
        <span className="text-2xl text-gray-500 font-bold">1</span>
        <div className="w-10 h-10 bg-green-900 rounded-full border-2 border-white flex items-center justify-center text-xs font-bold">Elf</div>
      </div>
    </div>

    {/* Info Partita */}
    <div className="flex items-center justify-center gap-8 w-2/4">
      <div className="flex flex-col items-center">
        <span className="text-xs text-gray-400">TURN</span>
        <span className="text-xl font-bold text-white">6</span>
      </div>
      
      {/* Re-rolls */}
      <div className="flex flex-col items-center">
        <span className="text-xs text-gray-400">RE-ROLLS</span>
        <div className="flex items-center gap-1">
          <div className="w-5 h-5 rounded-full bg-yellow-500 border border-yellow-300 flex items-center justify-center text-[10px] text-black font-bold">2</div>
          <div className="w-5 h-5 rounded-full bg-green-500 border border-green-300 flex items-center justify-center text-[10px] text-black font-bold">1</div>
        </div>
      </div>

      {/* Timer */}
      <div className="flex flex-col items-center">
        <span className="text-xs text-gray-400">TIMER</span>
        <div className="text-xl font-bold text-red-500 flex items-center gap-1">
          <Activity size={16} /> 01:21
        </div>
      </div>

      {/* Meteo */}
      <div className="flex flex-col items-center">
        <span className="text-xs text-gray-400">WEATHER</span>
        <div className="text-sm text-blue-300 flex items-center gap-1">
          <span className="text-lg">🌧️</span> Raining
        </div>
      </div>
    </div>

    {/* Fan Attendance & Dedicated Fans */}
    <div className="flex items-center justify-end gap-6 w-1/4">
      <div className="flex flex-col items-end">
        <span className="text-xs text-gray-400">FAN ATTENDANCE</span>
        <span className="text-xl font-bold text-yellow-500">7</span>
        <span className="text-[10px] text-gray-500">
          Orc: 4 | Elf: 3 <span className="text-yellow-600">(Dedicated)</span>
        </span>
      </div>
      <Settings className="text-gray-400 cursor-pointer hover:text-white" size={20} />
    </div>
  </header>
);

const DiceLogPanel = () => (
  <div className="bg-[#1e1e1e] border border-white/10 rounded-lg p-2 h-full flex flex-col">
    <div className="text-xs text-gray-400 mb-2 font-bold uppercase flex items-center gap-2">
      <Dice5 size={14} /> Log dei Dadi
    </div>
    <div className="flex-1 overflow-y-auto text-xs space-y-2">
      {[
        { type: 'block', text: 'Orc Blitzer #5 uses Blitz', color: 'text-gray-300' },
        { type: 'roll', text: '[Block Roll] -> Selected', color: 'text-yellow-400' },
        { type: 'armor', text: 'Elf #8 Armor Roll: 9 -> Armor Broken!', color: 'text-red-400' },
        { type: 'injury', text: 'Elf #8 Injury Roll: 10 -> CASUALTY', color: 'text-red-500 font-bold' },
      ].map((log, i) => (
        <div key={i} className={`p-1 rounded ${log.color === 'text-red-500' ? 'bg-red-900/20' : ''}`}>
          <span className={log.color}>{log.text}</span>
        </div>
      ))}
    </div>
  </div>
);

const ChatPanel = () => (
  <div className="flex-1 flex flex-col bg-[#1e1e1e] border border-white/10 rounded-lg mt-2 overflow-hidden">
    <div className="bg-[#2a2a2a] p-2 text-xs font-bold text-gray-300 flex items-center justify-between">
      <span className="flex items-center gap-2"><MessageSquare size={12} /> Chat</span>
    </div>
    <div className="flex-1 p-2 overflow-y-auto text-xs space-y-2">
      <div className="text-green-400"><b>FUMBBL_Player:</b> Bel colpo!</div>
      <div className="text-red-400"><b>Orc_Admin:</b> RIP Sylvan Glader</div>
    </div>
    <div className="p-2 border-t border-white/10 flex gap-2">
      <input 
        type="text" 
        placeholder="Write a message..." 
        className="flex-1 bg-[#111] border border-white/10 rounded px-2 py-1 text-xs text-white focus:outline-none focus:border-blue-500"
      />
      <button className="p-1 bg-blue-600 rounded hover:bg-blue-500">
        <Send size={14} />
      </button>
    </div>
  </div>
);

const GameField = () => (
  <div className="relative w-full h-full bg-[#2d3a25] rounded-lg border-4 border-[#1a2015] overflow-hidden shadow-inner">
    {/* Griglia del campo */}
    <div className="absolute inset-0 grid grid-cols-12 grid-rows-8 gap-[1px] opacity-20">
      {Array.from({ length: 96 }).map((_, i) => (
        <div key={i} className="bg-white/5"></div>
      ))}
    </div>
    
    {/* Linea centrocampo */}
    <div className="absolute top-0 bottom-0 left-1/2 w-0.5 bg-white/30"></div>
    <div className="absolute top-0 bottom-0 left-1/4 w-0.5 border-r border-dashed border-white/20"></div>
    <div className="absolute top-0 bottom-0 right-1/4 w-0.5 border-l border-dashed border-white/20"></div>

    {/* Token Giocatori (Esempio) */}
    <div className="absolute top-[20%] left-[50%] transform -translate-x-1/2 -translate-y-1/2">
      <div className="w-10 h-10 rounded-full bg-yellow-500 border-2 border-white shadow-lg flex items-center justify-center text-black font-bold relative group cursor-pointer hover:scale-110 transition-transform">
        1
        {/* Icona Abilità */}
        <div className="absolute -top-2 -right-2 w-4 h-4 bg-red-600 rounded-full text-[8px] text-white flex items-center justify-center border border-white">AG</div>
      </div>
    </div>
    
    {/* Overlay Azioni (Action Wheel Placeholder) */}
    <div className="absolute top-[40%] left-[60%] transform -translate-x-1/2 -translate-y-1/2">
      <div className="w-12 h-12 rounded-full bg-green-600 border-2 border-white shadow-xl flex items-center justify-center text-white font-bold relative">
        5
        <div className="absolute -bottom-6 left-1/2 transform -translate-x-1/2 bg-black/80 text-white text-[10px] px-2 py-1 rounded whitespace-nowrap">
          Orc Blitzer #5
        </div>
      </div>
    </div>

    {/* Effetti Visivi (Dadi, POW) */}
    <div className="absolute top-[35%] left-[55%]">
        <div className="text-4xl animate-bounce">💥</div>
        <div className="absolute -top-2 -right-2 bg-red-600 text-white text-[10px] px-1 rounded font-bold">POW</div>
    </div>
  </div>
);

// --- LAYOUT PRINCIPALE ---

export default function DashboardLayout() {
  const [selectedPlayer, setSelectedPlayer] = useState<number | null>(null);

  return (
    <div className="flex flex-col h-screen w-screen bg-[#121212] text-white overflow-hidden font-sans">
      {/* 1. HEADER */}
      <Header />

      {/* 2. MAIN BODY */}
      <div className="flex flex-1 overflow-hidden p-2 gap-2">
        
        {/* SIDBAR SINISTRA: Team 1 Roster */}
        <div className="w-64 bg-[#1a1a1a] rounded-lg border border-white/10 flex flex-col overflow-hidden">
          <div className="bg-[#252525] p-2 text-xs font-bold text-center border-b border-white/10">
            TEAM ROSTERS - ORCS
          </div>
          <div className="flex-1 overflow-y-auto">
            {TEAM_1_PLAYERS.map(p => (
              <PlayerRow key={p.id} player={p} />
            ))}
          </div>
          <div className="p-2 border-t border-white/10 text-center">
             <button className="w-full bg-green-700 hover:bg-green-600 text-xs py-1 rounded font-bold">
               PLAYING
             </button>
          </div>
        </div>

        {/* CENTRO: Campo di Gioco */}
        <div className="flex-1 flex flex-col gap-2 min-w-0">
          {/* Area Campo */}
          <div className="flex-1 relative">
            <GameField />
            
            {/* Action Wheel (Appare al click, qui come esempio statico per layout) */}
            {selectedPlayer && (
              <div className="absolute top-1/2 left-1/2 transform -translate-x-1/2 -translate-y-1/2 z-50">
                <div className="bg-[#1e1e1e] border border-white/20 rounded-full p-4 shadow-2xl flex flex-col items-center gap-4 animate-in fade-in zoom-in duration-200">
                  <div className="text-xs text-gray-400 mb-2">AZIONI DISPONIBILI</div>
                  <div className="flex gap-4">
                    <button className="w-16 h-16 rounded-full bg-blue-600 hover:bg-blue-500 border-2 border-white flex flex-col items-center justify-center text-xs font-bold shadow-lg">
                      <Activity size={20} />
                      MOVE
                    </button>
                    <button className="w-16 h-16 rounded-full bg-red-600 hover:bg-red-500 border-2 border-white flex flex-col items-center justify-center text-xs font-bold shadow-lg">
                      <Shield size={20} />
                      BLOCK
                    </button>
                    <button className="w-16 h-16 rounded-full bg-gray-700 hover:bg-gray-600 border-2 border-white flex flex-col items-center justify-center text-xs font-bold shadow-lg">
                      <MessageSquare size={20} />
                      PASS
                    </button>
                  </div>
                </div>
              </div>
            )}
          </div>

          {/* BOTTOM AREA: Dice Log (Sostituisce la barra azioni statica) */}
          <div className="h-48 shrink-0">
            <DiceLogPanel />
          </div>
        </div>

        {/* SIDBAR DESTRA: Team 2 Roster + Chat */}
        <div className="w-64 flex flex-col gap-2">
          {/* Roster Team 2 */}
          <div className="h-[400px] bg-[#1a1a1a] rounded-lg border border-white/10 flex flex-col overflow-hidden">
            <div className="bg-[#252525] p-2 text-xs font-bold text-center border-b border-white/10">
              TEAM ROSTERS - ELVES
            </div>
            <div className="flex-1 overflow-y-auto">
              {TEAM_2_PLAYERS.map(p => (
                <PlayerRow key={p.id} player={p} />
              ))}
            </div>
          </div>

          {/* Chat */}
          <ChatPanel />
        </div>

      </div>
    </div>
  );
}
