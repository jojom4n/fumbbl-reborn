// =============================================================================
// FumbblDebugPanel — Debug/Testing Panel for FUMBBL Services
// =============================================================================

import { useState, useEffect, useRef } from 'react';
import { useGameState } from '../../contexts/GameContext';

// -----------------------------------------------------------------------------
// Unique ID generator (avoids Date.now() duplicates)
// -----------------------------------------------------------------------------
let _debugIdCounter = 0;
const generateDebugId = (): number => {
  _debugIdCounter += 1;
  return Number(`${Date.now()}${String(_debugIdCounter).padStart(6, '0')}`);
};

// -----------------------------------------------------------------------------
// Debug Log Entry
// -----------------------------------------------------------------------------

interface DebugLogEntry {
  id: number;
  timestamp: string;
  level: 'info' | 'warn' | 'error' | 'success';
  message: string;
}

// -----------------------------------------------------------------------------
// CollapsibleLog — scrollable log section within the main page
// -----------------------------------------------------------------------------

function CollapsibleLog({
  logs,
  onClear,
  isOpen,
  onToggle,
}: {
  logs: DebugLogEntry[];
  onClear: () => void;
  isOpen: boolean;
  onToggle: () => void;
}) {
  const logEndRef = useRef<HTMLDivElement>(null);

  const logColors: Record<string, string> = {
    info: 'text-blue-400',
    warn: 'text-yellow-400',
    error: 'text-red-400',
    success: 'text-green-400',
  };

  const logIcons: Record<string, string> = {
    info: 'ℹ️',
    warn: '⚠️',
    error: '❌',
    success: '✅',
  };

  // Auto-scroll to bottom when new logs are added
  useEffect(() => {
    if (isOpen && logEndRef.current) {
      logEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, isOpen]);

  if (!isOpen) {
    return (
      <button
        onClick={onToggle}
        className="w-full bg-gray-800 hover:bg-gray-700 border border-gray-700 rounded-lg p-3 text-left text-sm text-gray-300 transition-colors"
      >
        <span className="flex items-center justify-between">
          <span>📋 Debug Log ({logs.length})</span>
          <span className="text-xs">▼</span>
        </span>
      </button>
    );
  }

  return (
    <div className="bg-gray-800 border border-gray-700 rounded-lg overflow-hidden">
      <div className="flex items-center justify-between px-3 py-2 bg-gray-900 border-b border-gray-700">
        <h3 className="text-sm font-bold text-yellow-300">
          📋 Debug Log ({logs.length})
        </h3>
        <div className="flex gap-2">
          <button
            onClick={onClear}
            className="text-xs text-gray-400 hover:text-white transition-colors"
          >
            Pulisci
          </button>
          <button
            onClick={onToggle}
            className="text-xs text-gray-400 hover:text-white transition-colors"
          >
            ✕ Chiudi
          </button>
        </div>
      </div>
      <div className="max-h-64 overflow-y-auto overflow-x-hidden bg-black p-2 font-mono text-xs custom-scrollbar">
        {logs.length === 0 ? (
          <span className="text-gray-600">Nessun log...</span>
        ) : (
          logs.map(log => (
            <div key={log.id} className={`${logColors[log.level]} py-0.5 whitespace-nowrap overflow-hidden text-ellipsis`}>
              <span className="text-gray-500">[{log.timestamp}]</span>{' '}
              <span>{logIcons[log.level]}</span>{' '}
              <span className="inline-block max-w-[80%] truncate align-bottom">{log.message}</span>
            </div>
          ))
        )}
        <div ref={logEndRef} />
      </div>
    </div>
  );
}

// -----------------------------------------------------------------------------
// FumbblDebugPanel Component
// -----------------------------------------------------------------------------

export function FumbblDebugPanel() {
  const {
    gameState,
    fumbblService,
    isServiceConnected,
    isAuthenticated,
    setState,
    setCredentials,
    connectToGame,
    disconnect,
    sendChatMessage,
    requestReroll,
    confirmDecision,
    declineDecision,
  } = useGameState();

  // Form state
  const [clientId, setClientId] = useState('');
  const [clientSecret, setClientSecret] = useState('');
  const [fumbblUsername, setFumbblUsername] = useState('');
  const [gameId, setGameId] = useState('');
  const [teamId, setTeamId] = useState('');
  const [spectate, setSpectate] = useState(false);
  const [chatMessage, setChatMessage] = useState('');

  // Simulation state
  const [simulatedGameState, setSimulatedGameState] = useState(false);

  // Collapsible sections
  const [collapsedSections, setCollapsedSections] = useState<Record<string, boolean>>({
    auth: true,
    connection: true,
    gameState: true,
    actions: true,
    log: true,
  });

  // Debug log
  const [logs, setLogs] = useState<DebugLogEntry[]>([]);

  const addLog = (message: string, level: 'info' | 'warn' | 'error' | 'success' = 'info') => {
    const now = new Date();
    const timestamp = now.toTimeString().slice(0, 8);
    setLogs(prev => [...prev, {
      id: generateDebugId(),
      timestamp,
      level,
      message,
    }]);
  };

  // Watch connection state changes
  useEffect(() => {
    if (fumbblService) {
      addLog('FUMBBL Service initialized', 'info');
    }
  }, [fumbblService]);

  useEffect(() => {
    if (isServiceConnected) {
      addLog('WebSocket connected', 'success');
    } else if (fumbblService) {
      addLog('WebSocket disconnected', 'warn');
    }
  }, [isServiceConnected, fumbblService]);

  // Watch game state changes
  useEffect(() => {
    if (gameState.team1.name || gameState.team2.name) {
      addLog(`Game loaded: ${gameState.team1.name} vs ${gameState.team2.name}`, 'success');
    }
  }, [gameState.team1.name, gameState.team2.name]);

  // ---------------------------------------------------------------------------
  // Handlers
  // ---------------------------------------------------------------------------

  const toggleSection = (section: string) => {
    setCollapsedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  const handleAuthenticate = async () => {
    if (!clientId || !clientSecret) {
      addLog('clientId e clientSecret richiesti', 'error');
      return;
    }
    try {
      addLog(`Autenticazione in corso...`, 'info');
      await setCredentials(clientId, clientSecret);
      addLog('Autenticazione OAuth2 completata con successo', 'success');
    } catch (error) {
      addLog(`Errore autenticazione: ${(error as Error).message}`, 'error');
    }
  };

  const handleConnect = async () => {
    if (!gameId) {
      addLog('gameId richiesto', 'error');
      return;
    }
    if (!fumbblUsername) {
      addLog('Username FUMBBL richiesto (campo "Username FUMBBL")', 'error');
      return;
    }
    try {
      if (spectate) {
        addLog(`Connessione spettatore al gioco ${gameId} come ${fumbblUsername}...`, 'info');
        await connectToGame(parseInt(gameId), undefined, true, fumbblUsername);
        addLog('Connessione spettatore avviata', 'info');
      } else {
        addLog(`Connessione al gioco ${gameId} come ${fumbblUsername}...`, 'info');
        await connectToGame(parseInt(gameId), teamId ? parseInt(teamId) : undefined, false, fumbblUsername);
        addLog('Connessione avviata', 'info');
      }
    } catch (error) {
      addLog(`Errore: ${(error as Error).message}`, 'error');
    }
  };

  const handleDisconnect = () => {
    disconnect();
    addLog('Disconnessione dal gioco', 'warn');
  };

  const handleSendChat = () => {
    if (!chatMessage.trim()) return;
    sendChatMessage(chatMessage);
    addLog(`Chat inviata: "${chatMessage}"`, 'info');
    setChatMessage('');
  };

  const handleRequestReroll = () => {
    requestReroll();
    addLog('Reroll richiesto', 'info');
  };

  const handleConfirmDecision = () => {
    confirmDecision(true);
    addLog('Decisione confermata: TRUE', 'info');
  };

  const handleDeclineDecision = () => {
    declineDecision();
    addLog('Decisione declinata', 'info');
  };

  const handleSimulateState = () => {
    setSimulatedGameState(prev => !prev);
    if (!simulatedGameState) {
      addLog('Simulazione stato gioco: ON', 'success');
      setState({
        score: { team1: 2, team2: 1 },
        turn: 6,
        phase: 'regular',
        reRolls: { team1: 2, team2: 1 },
        timer: 81,
        isLive: true,
        team1: {
          id: 'orc',
          name: 'Orcs',
          race: 'Orc',
          players: [],
          color: '#4a7c3f',
        },
        team2: {
          id: 'elf',
          name: 'Elves',
          race: 'Elf',
          players: [],
          color: '#c4a35a',
        },
      });
    } else {
      addLog('Simulazione stato gioco: OFF', 'warn');
      setState({
        score: { team1: 0, team2: 0 },
        turn: 0,
        phase: 'setup',
        reRolls: { team1: 0, team2: 0 },
        timer: 120,
        isLive: false,
        team1: { id: '', name: '', race: '', players: [], color: '#4a7c3f' },
        team2: { id: '', name: '', race: '', players: [], color: '#c4a35a' },
      });
    }
  };

  const handleClearLogs = () => {
    setLogs([]);
  };

  // ---------------------------------------------------------------------------
  // Helper
  // ---------------------------------------------------------------------------

  const formatTime = (seconds: number) => {
    const m = Math.floor(seconds / 60);
    const s = seconds % 60;
    return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
  };

  const sectionArrow = (section: string) => collapsedSections[section] ? '▶' : '▼';

  // ---------------------------------------------------------------------------
  // Render
  // ---------------------------------------------------------------------------

  return (
    <div className="w-full bg-gray-900 border border-gray-700 rounded-lg my-2 overflow-hidden">
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-2 bg-gray-950 border-b border-gray-700">
        <h2 className="text-base font-bold text-yellow-400 flex items-center gap-2">
          <span>🔧</span> FUMBBL Debug Panel
        </h2>
        <span className={`text-xs px-2 py-0.5 rounded ${isServiceConnected ? 'bg-green-900 text-green-300' : 'bg-red-900 text-red-300'}`}>
          {isServiceConnected ? '● CONNESSO' : '● DISCONNESSO'}
        </span>
      </div>

      {/* Collapsible Sections */}
      <div className="p-3 space-y-2 max-h-[60vh] overflow-y-auto custom-scrollbar">
        {/* Auth Section */}
        <div className="bg-gray-800 border border-gray-700 rounded">
          <button
            onClick={() => toggleSection('auth')}
            className="w-full flex items-center justify-between px-3 py-2 text-left text-sm"
          >
            <span className="text-yellow-300 font-bold">🔐 Autenticazione OAuth2</span>
            <span className="text-xs text-gray-400">{sectionArrow('auth')}</span>
          </button>
          {!collapsedSections.auth && (
            <div className="px-3 pb-3 space-y-2">
              <div className="grid grid-cols-2 gap-2">
                <input
                  type="text"
                  placeholder="clientId (da fumbbl.com/settings)"
                  value={clientId}
                  onChange={e => setClientId(e.target.value)}
                  className="bg-gray-900 border border-gray-600 rounded px-3 py-1.5 text-gray-200 text-xs"
                />
                <input
                  type="password"
                  placeholder="clientSecret (da fumbbl.com/settings)"
                  value={clientSecret}
                  onChange={e => setClientSecret(e.target.value)}
                  className="bg-gray-900 border border-gray-600 rounded px-3 py-1.5 text-gray-200 text-xs"
                />
              </div>
              <div>
                <input
                  type="text"
                  placeholder="Username FUMBBL (il tuo account coach)"
                  value={fumbblUsername}
                  onChange={e => setFumbblUsername(e.target.value)}
                  className="bg-gray-900 border border-gray-600 rounded px-3 py-1.5 text-gray-200 text-xs w-full"
                />
                <span className="text-[10px] text-gray-500">Questo è il tuo username FUMBBL, usato come "coach" nel clientJoin</span>
              </div>
              <div className="flex gap-2">
                <button
                  onClick={handleAuthenticate}
                  className="bg-blue-600 hover:bg-blue-700 text-white px-3 py-1.5 rounded text-xs transition-colors"
                >
                  Autentic OAuth2
                </button>
                <span className={`text-xs self-center ${isAuthenticated ? 'text-green-400' : 'text-gray-500'}`}>
                  {isAuthenticated ? '✓ OAuth2' : 'Non autenticato'}
                </span>
              </div>
            </div>
          )}
        </div>

        {/* Connection Section */}
        <div className="bg-gray-800 border border-gray-700 rounded">
          <button
            onClick={() => toggleSection('connection')}
            className="w-full flex items-center justify-between px-3 py-2 text-left text-sm"
          >
            <span className="text-yellow-300 font-bold">🔌 Connessione Gioco</span>
            <span className="text-xs text-gray-400">{sectionArrow('connection')}</span>
          </button>
          {!collapsedSections.connection && (
            <div className="px-3 pb-3 space-y-2">
              <div className="grid grid-cols-2 gap-2">
                <input
                  type="number"
                  placeholder="gameId"
                  value={gameId}
                  onChange={e => setGameId(e.target.value)}
                  className="bg-gray-900 border border-gray-600 rounded px-3 py-1.5 text-gray-200 text-xs"
                />
                <input
                  type="number"
                  placeholder="teamId (non necessario per spettatore)"
                  value={teamId}
                  onChange={e => setTeamId(e.target.value)}
                  disabled={spectate}
                  className="bg-gray-900 border border-gray-600 rounded px-3 py-1.5 text-gray-200 text-xs disabled:opacity-50 disabled:cursor-not-allowed"
                />
              </div>
              {fumbblUsername && (
                <div className="text-xs text-green-400">
                  Username FUMBBL: <span className="font-bold">{fumbblUsername}</span> (usato come "coach" nel clientJoin)
                </div>
              )}
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={spectate}
                  onChange={e => setSpectate(e.target.checked)}
                  className="w-4 h-4 accent-blue-500"
                />
                <span className="text-xs text-gray-300">Modalità Spettatore (nessun teamId richiesto)</span>
              </label>
              <div className="flex gap-2">
                <button
                  onClick={handleConnect}
                  className="bg-green-600 hover:bg-green-700 text-white px-3 py-1.5 rounded text-xs transition-colors"
                >
                  Connetti
                </button>
                <button
                  onClick={handleDisconnect}
                  disabled={!isServiceConnected}
                  className="bg-red-600 hover:bg-red-700 disabled:bg-gray-700 disabled:cursor-not-allowed text-white px-3 py-1.5 rounded text-xs transition-colors"
                >
                  Disconnetti
                </button>
              </div>
            </div>
          )}
        </div>

        {/* Game State Section */}
        <div className="bg-gray-800 border border-gray-700 rounded">
          <button
            onClick={() => toggleSection('gameState')}
            className="w-full flex items-center justify-between px-3 py-2 text-left text-sm"
          >
            <span className="text-yellow-300 font-bold">🎮 Stato di Gioco</span>
            <span className="text-xs text-gray-400">{sectionArrow('gameState')}</span>
          </button>
          {!collapsedSections.gameState && (
            <div className="px-3 pb-3">
              <div className="grid grid-cols-2 gap-x-4 gap-y-1.5 text-xs">
                <span className="text-gray-400">Fase:</span>
                <span className="text-white capitalize">{gameState.phase}</span>

                <span className="text-gray-400">Turno:</span>
                <span className="text-white">{gameState.turn}</span>

                <span className="text-gray-400">Score:</span>
                <span className="text-white font-bold">{gameState.score.team1} - {gameState.score.team2}</span>

                <span className="text-gray-400">Timer:</span>
                <span className="text-white">{formatTime(gameState.timer)}</span>

                <span className="text-gray-400">ReRolls T1:</span>
                <span className="text-white">{gameState.reRolls.team1}</span>

                <span className="text-gray-400">ReRolls T2:</span>
                <span className="text-white">{gameState.reRolls.team2}</span>

                <span className="text-gray-400">Team 1:</span>
                <span className="text-white">{gameState.team1.name || '(vuoto)'}</span>

                <span className="text-gray-400">Team 2:</span>
                <span className="text-white">{gameState.team2.name || '(vuoto)'}</span>

                <span className="text-gray-400">Is Live:</span>
                <span className={gameState.isLive ? 'text-green-400' : 'text-red-400'}>
                  {gameState.isLive ? 'SÌ' : 'NO'}
                </span>

                <span className="text-gray-400">Ultimo update:</span>
                <span className="text-gray-500">{new Date(gameState.lastUpdate).toLocaleTimeString()}</span>
              </div>
            </div>
          )}
        </div>

        {/* Actions Section */}
        <div className="bg-gray-800 border border-gray-700 rounded">
          <button
            onClick={() => toggleSection('actions')}
            className="w-full flex items-center justify-between px-3 py-2 text-left text-sm"
          >
            <span className="text-yellow-300 font-bold">⚡ Azioni</span>
            <span className="text-xs text-gray-400">{sectionArrow('actions')}</span>
          </button>
          {!collapsedSections.actions && (
            <div className="px-3 pb-3 space-y-2">
              <div className="flex flex-wrap gap-2">
                <input
                  type="text"
                  placeholder="Messaggio chat..."
                  value={chatMessage}
                  onChange={e => setChatMessage(e.target.value)}
                  onKeyDown={e => e.key === 'Enter' && handleSendChat()}
                  className="bg-gray-900 border border-gray-600 rounded px-3 py-1.5 text-gray-200 text-xs flex-1 min-w-37.5"
                />
                <button
                  onClick={handleSendChat}
                  disabled={!chatMessage.trim()}
                  className="bg-purple-600 hover:bg-purple-700 disabled:bg-gray-700 text-white px-3 py-1.5 rounded text-xs"
                >
                  💬 Chat
                </button>
                <button
                  onClick={handleRequestReroll}
                  className="bg-orange-600 hover:bg-orange-700 text-white px-3 py-1.5 rounded text-xs"
                >
                  🎲 Reroll
                </button>
                <button
                  onClick={handleConfirmDecision}
                  className="bg-green-600 hover:bg-green-700 text-white px-3 py-1.5 rounded text-xs"
                >
                  ✓ Conferma
                </button>
                <button
                  onClick={handleDeclineDecision}
                  className="bg-red-600 hover:bg-red-700 text-white px-3 py-1.5 rounded text-xs"
                >
                  ✕ Nega
                </button>
                <button
                  onClick={handleSimulateState}
                  className={`${simulatedGameState ? 'bg-yellow-600 hover:bg-yellow-700' : 'bg-gray-600 hover:bg-gray-700'} text-white px-3 py-1.5 rounded text-xs`}
                >
                  {simulatedGameState ? '⏹ Ferma Sim' : '▶ Simula Stato'}
                </button>
              </div>
              <span className="text-gray-500 text-xs block">
                Simula Stato: inietta un gameState mock per testare la UI senza connessione FUMBBL
              </span>
            </div>
          )}
        </div>

        {/* Debug Log (scrollable within main page) */}
        <CollapsibleLog
          logs={logs}
          onClear={handleClearLogs}
          isOpen={!collapsedSections.log}
          onToggle={() => toggleSection('log')}
        />
      </div>
    </div>
  );
}