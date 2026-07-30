// =============================================================================
// GameContext — Global Game State Provider
// =============================================================================

import { createContext, useContext, useReducer, useCallback, useEffect, useRef, useState, ReactNode } from 'react';
import {
  GameState,
  Player,
  BloodBowlAction,
  DiceLogEntry,
  ChatMessage,
} from '../types/bloodbowl';

import { FumbblService, FumbblServiceConfig } from '../services/fumbblService';
import { parseReportsToDiceLogs } from '../services/reportParser';

// -----------------------------------------------------------------------------
// Unique ID generator (avoids duplicates)
// Uses simple counter to stay within JavaScript safe integer range.
// Date.now() concatenated with counter creates numbers too large for safe Number representation,
// causing precision loss and duplicate IDs (e.g., 1785112287757000000 for all calls).
// -----------------------------------------------------------------------------
let _idCounter = 0;
const generateUniqueId = (): number => {
  _idCounter += 1;
  // Simple counter-based ID - always unique, always within safe integer range
  return _idCounter;
};

// -----------------------------------------------------------------------------
// Default Initial State (no mock data — clean slate)
// -----------------------------------------------------------------------------

const createInitialState = (): GameState => ({
  score: { team1: 0, team2: 0 },
  turn: 0,
  phase: 'setup',
  reRolls: { team1: 0, team2: 0 },
  timer: 120,
  weather: { type: 'clear', icon: '☀️', description: 'Clear' },
  fanAttendance: { total: 0, dedicatedFans: { team1: 0, team2: 0 } },
  team1: {
    id: '',
    name: '',
    race: '',
    players: [],
    color: '#4a7c3f',
  },
  team2: {
    id: '',
    name: '',
    race: '',
    players: [],
    color: '#c4a35a',
  },
  team1Players: [],
  team2Players: [],
  field: {
    markers: [],
    ballPosition: { x: 8, y: 5 },
  },
  ballPosition: { x: 8, y: 5 },
  selectedPlayer: null,
  selectedTeam: 'team1',
  diceLog: [],
  chatMessages: [],
  isLive: false,
  lastUpdate: Date.now(),
});

// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------

type GameAction =
  | { type: 'SET_STATE'; payload: Partial<GameState> }
  | { type: 'SELECT_PLAYER'; payload: Player | null }
  | { type: 'SELECT_TEAM'; payload: 'team1' | 'team2' }
  | { type: 'SEND_ACTION'; payload: { action: BloodBowlAction; playerId: number } }
  | { type: 'ADD_DICE_LOG'; payload: DiceLogEntry }
  | { type: 'ADD_DICE_LOGS'; payload: DiceLogEntry[] }
  | { type: 'CLEAR_DICE_LOG' }
  | { type: 'ADD_CHAT_MESSAGE'; payload: ChatMessage }
  | { type: 'ADD_CHAT_MESSAGES'; payload: ChatMessage[] }
  | { type: 'CLEAR_CHAT' }
  | { type: 'UPDATE_TIMER'; payload: number }
  | { type: 'UPDATE_SCORE'; payload: { team1: number; team2: number } }
  | { type: 'UPDATE_TURN'; payload: number }
  | { type: 'UPDATE_FAN_ATTENDANCE'; payload: { total: number; dedicatedFans: { team1: number; team2: number } } }
  | { type: 'TOGGLE_REROLL'; payload: 'team1' | 'team2' }
  | { type: 'LOAD_STATE'; payload: GameState }
  | { type: 'SYNC_STATE'; payload: GameState };

// -----------------------------------------------------------------------------
// Reducer
// -----------------------------------------------------------------------------

function gameReducer(state: GameState, action: GameAction): GameState {
  switch (action.type) {
    case 'SET_STATE':
      return { ...state, ...action.payload, lastUpdate: Date.now() };
    case 'SELECT_PLAYER':
      return { ...state, selectedPlayer: action.payload, lastUpdate: Date.now() };
    case 'SELECT_TEAM':
      return { ...state, selectedTeam: action.payload, lastUpdate: Date.now() };
    case 'SEND_ACTION': {
      const player = [...state.team1Players, ...state.team2Players].find(p => p.id === action.payload.playerId);
      if (!player) return state;
      const newId = generateUniqueId();
      const now = Date.now();
      const newLogEntry: DiceLogEntry = {
        id: newId,
        type: 'action' as const,
        timestamp: now,
        text: `${player.name} uses ${action.payload.action.charAt(0).toUpperCase() + action.payload.action.slice(1)}`,
        turn: state.turn,
      };
      return {
        ...state,
        diceLog: [...state.diceLog, newLogEntry],
        lastUpdate: Date.now(),
      };
    }
    case 'ADD_DICE_LOG':
      return { ...state, diceLog: [...state.diceLog, action.payload], lastUpdate: Date.now() };
    case 'ADD_DICE_LOGS':
      return { ...state, diceLog: [...state.diceLog, ...action.payload], lastUpdate: Date.now() };
    case 'CLEAR_DICE_LOG':
      return { ...state, diceLog: [], lastUpdate: Date.now() };
    case 'ADD_CHAT_MESSAGE':
      return { ...state, chatMessages: [...state.chatMessages, action.payload], lastUpdate: Date.now() };
    case 'ADD_CHAT_MESSAGES':
      return { ...state, chatMessages: [...state.chatMessages, ...action.payload], lastUpdate: Date.now() };
    case 'CLEAR_CHAT':
      return { ...state, chatMessages: [], lastUpdate: Date.now() };
    case 'UPDATE_TIMER':
      return { ...state, timer: action.payload, lastUpdate: Date.now() };
    case 'UPDATE_SCORE':
      return { ...state, score: action.payload, lastUpdate: Date.now() };
    case 'UPDATE_TURN':
      return { ...state, turn: action.payload, lastUpdate: Date.now() };
    case 'UPDATE_FAN_ATTENDANCE':
      return { ...state, fanAttendance: action.payload, lastUpdate: Date.now() };
    case 'TOGGLE_REROLL':
      return {
        ...state,
        reRolls: {
          ...state.reRolls,
          [action.payload]: Math.max(0, state.reRolls[action.payload] - 1),
        },
        lastUpdate: Date.now(),
      };
    case 'LOAD_STATE':
      return { ...action.payload, lastUpdate: Date.now() };
    case 'SYNC_STATE':
      // Deep merge incoming partial state with current state.
      // CRITICAL: Do NOT use createInitialState() as it resets ALL fields to defaults,
      // which wipes out diceLog, chatMessages, etc. when receiving partial updates like {timer: 28}.
      // Instead, merge the incoming payload into the existing state, preserving fields not present.
      const merged = { ...state, ...action.payload };
      // Deep merge nested objects that should be preserved when the payload has partial data
      if (action.payload.score && typeof action.payload.score === 'object') {
        merged.score = { ...state.score, ...action.payload.score };
      }
      if (action.payload.fanAttendance && typeof action.payload.fanAttendance === 'object') {
        merged.fanAttendance = { ...state.fanAttendance, ...action.payload.fanAttendance };
      }
      if (action.payload.reRolls && typeof action.payload.reRolls === 'object') {
        merged.reRolls = { ...state.reRolls, ...action.payload.reRolls };
      }
      // Preserve UI-only state that the server does NOT send back:
      // - selectedPlayer: local selection made by the user, server has no knowledge of it
      //   (the GameModel.toGameState() always returns selectedPlayer:null, so we must preserve)
      //   CRITICAL: Must refresh the reference to match the current player from updated arrays,
      //   otherwise the PlayerInfo panel shows stale data (old position, hasBall, status, injuries)
      //   or appears to "disappear" when the player object no longer matches the current game state.
      if (state.selectedPlayer) {
        const allPlayers = [
          ...(action.payload.team1Players || []),
          ...(action.payload.team2Players || []),
        ];
        const selectedId = String(state.selectedPlayer.id);
        merged.selectedPlayer = allPlayers.find(
          p => String(p.id) === selectedId
        ) || state.selectedPlayer;
      } else {
        merged.selectedPlayer = null;
      }
      // - diceLog: local log of dice rolls and actions, accumulated client-side
      // - chatMessages: local chat history, accumulated client-side
      merged.diceLog = state.diceLog;
      merged.chatMessages = state.chatMessages;
      return { ...merged, lastUpdate: Date.now() };
    default:
      return state;
  }
}

// -----------------------------------------------------------------------------
// Context
// -----------------------------------------------------------------------------

interface GameContextType {
  gameState: GameState;
  // FUMBBL Service access
  fumbblService: FumbblService | null;
  isServiceConnected: boolean;
  // Authentication
  isAuthenticated: boolean;
  // Actions
  setState: (payload: Partial<GameState>) => void;
  setCredentials: (clientId: string, clientSecret: string) => Promise<void>;
  selectPlayer: (player: Player | null) => void;
  selectTeam: (team: 'team1' | 'team2') => void;
  sendAction: (action: BloodBowlAction, playerId: number) => void;
  addDiceLog: (entry: DiceLogEntry) => void;
  addDiceLogs: (entries: DiceLogEntry[]) => void;
  clearDiceLog: () => void;
  addChatMessage: (message: ChatMessage) => void;
  clearChat: () => void;
  updateTimer: (timer: number) => void;
  updateScore: (score: { team1: number; team2: number }) => void;
  updateTurn: (turn: number) => void;
  updateFanAttendance: (fa: { total: number; dedicatedFans: { team1: number; team2: number } }) => void;
  toggleReRoll: (team: 'team1' | 'team2') => void;
  loadState: (state: GameState) => void;
  // Service methods
  connectToGame: (gameId: number, teamId?: number, spectate?: boolean, fumbblUsername?: string) => Promise<void>;
  connectAsSpectator: (gameId: number, fumbblUsername?: string) => Promise<void>;
  disconnect: () => void;
  sendChatMessage: (message: string) => void;
  requestReroll: () => void;
  confirmDecision: (decision: boolean, param?: string) => void;
  declineDecision: (param?: string) => void;
}

const GameContext = createContext<GameContextType | undefined>(undefined);

// -----------------------------------------------------------------------------
// Provider
// -----------------------------------------------------------------------------

interface GameProviderProps {
  children: ReactNode;
  serviceConfig?: FumbblServiceConfig;
}

export function GameProvider({ children, serviceConfig }: GameProviderProps) {
  const [gameState, dispatch] = useReducer(gameReducer, null, createInitialState);
  const fumbblServiceRef = useRef<FumbblService | null>(null);
  const [isServiceConnected, setIsServiceConnected] = useState(false);
  const [isAuthenticated, setIsAuthenticated] = useState(false);

  // Initialize FUMBBL service
  useEffect(() => {
    fumbblServiceRef.current = new FumbblService(serviceConfig);

    // Set up callbacks
    const service = fumbblServiceRef.current;
    service.setCallbacks({
      onConnectionChange: (connected) => {
        setIsServiceConnected(connected);
      },
      onError: (error) => {
        console.error('[GameContext] Service error:', error);
      },
      onStateUpdate: (updatedState) => {
        console.log('[GameContext] onStateUpdate received:', {
          team1Name: updatedState.team1?.name,
          team2Name: updatedState.team2?.name,
          team1Players: updatedState.team1Players?.length,
          team2Players: updatedState.team2Players?.length,
          turn: updatedState.turn,
          phase: updatedState.phase,
          timer: updatedState.timer,
          score: updatedState.score,
        });
        // Sync the updated state to the reducer
        dispatch({ type: 'SYNC_STATE', payload: updatedState });
      },
      onModelChanges: (changes) => {
        console.log('[GameContext] Model changes received:', changes.length, 'changes');
        // Model changes are already applied to the GameModel inside the WebSocket
        // We sync the updated state from the GameModel
        const gameModel = fumbblServiceRef.current?.getGameModel();
        if (gameModel) {
          const updatedGameState = gameModel.toGameState() as GameState;
          console.log('[GameContext] Syncing state from model changes:', {
            team1Name: updatedGameState.team1?.name,
            team2Name: updatedGameState.team2?.name,
            team1Players: updatedGameState.team1Players?.length,
            team2Players: updatedGameState.team2Players?.length,
            turn: updatedGameState.turn,
            phase: updatedGameState.phase,
          });
          dispatch({ type: 'SYNC_STATE', payload: updatedGameState });
        }
      },
      onReports: (reports) => {
        console.log('[GameContext] Reports received:', reports.length, 'reports');
        // Parse FUMBBL server reports into DiceLogEntry objects
        const turn = gameState.turn;
        const diceEntries = parseReportsToDiceLogs(reports, turn);
        if (diceEntries.length > 0) {
          console.log('[GameContext] Adding', diceEntries.length, 'dice log entries from reports');
          dispatch({ type: 'ADD_DICE_LOGS', payload: diceEntries });
        }
      },
    });

    // Set up auth change callback
    service.setAuthCallback((isAuth) => {
      setIsAuthenticated(isAuth);
    });

    // Initialize the service (authenticate & connect if configured)
    fumbblServiceRef.current.initialize().catch(err => {
      console.error('[GameContext] Failed to initialize FUMBBL service:', err);
    });

    // Cleanup on unmount
    return () => {
      if (fumbblServiceRef.current) {
        fumbblServiceRef.current.disconnect();
      }
    };
  }, []); // Only run once on mount

  // Update auth status when service state changes
  useEffect(() => {
    if (fumbblServiceRef.current) {
      const state = fumbblServiceRef.current.getState();
      setIsAuthenticated(state.isAuthenticated);
    }
  }, [fumbblServiceRef.current?.getState()?.isAuthenticated]);

  // Timer countdown effect: ONLY run local countdown when NOT connected to WebSocket.
  // When connected, the server sends serverGameTime updates which handle the timer.
  // Running both causes the timer to jump back and forth (local countdown vs server updates).
  useEffect(() => {
    // Disable local countdown when WebSocket is connected - server handles timer
    if (isServiceConnected) return;
    if (!gameState.isLive || gameState.timer <= 0) return;
    const interval = setInterval(() => {
      dispatch({ type: 'UPDATE_TIMER', payload: gameState.timer - 1 });
    }, 1000);
    return () => clearInterval(interval);
  }, [gameState.isLive, gameState.timer, isServiceConnected]);

  // Memoized action creators
  const setState = useCallback((payload: Partial<GameState>) => dispatch({ type: 'SET_STATE', payload }), []);
  const selectPlayer = useCallback((player: Player | null) => dispatch({ type: 'SELECT_PLAYER', payload: player }), []);
  const selectTeam = useCallback((team: 'team1' | 'team2') => dispatch({ type: 'SELECT_TEAM', payload: team }), []);
  const sendAction = useCallback((action: BloodBowlAction, playerId: number) => dispatch({ type: 'SEND_ACTION', payload: { action, playerId } }), []);
  const addDiceLog = useCallback((entry: DiceLogEntry) => dispatch({ type: 'ADD_DICE_LOG', payload: entry }), []);
  const addDiceLogs = useCallback((entries: DiceLogEntry[]) => dispatch({ type: 'ADD_DICE_LOGS', payload: entries }), []);
  const clearDiceLog = useCallback(() => dispatch({ type: 'CLEAR_DICE_LOG' }), []);
  const addChatMessage = useCallback((message: ChatMessage) => dispatch({ type: 'ADD_CHAT_MESSAGE', payload: message }), []);
  const clearChat = useCallback(() => dispatch({ type: 'CLEAR_CHAT' }), []);
  const updateTimer = useCallback((timer: number) => dispatch({ type: 'UPDATE_TIMER', payload: timer }), []);
  const updateScore = useCallback((score: { team1: number; team2: number }) => dispatch({ type: 'UPDATE_SCORE', payload: score }), []);
  const updateTurn = useCallback((turn: number) => dispatch({ type: 'UPDATE_TURN', payload: turn }), []);
  const updateFanAttendance = useCallback((fa: { total: number; dedicatedFans: { team1: number; team2: number } }) => dispatch({ type: 'UPDATE_FAN_ATTENDANCE', payload: fa }), []);
  const toggleReRoll = useCallback((team: 'team1' | 'team2') => dispatch({ type: 'TOGGLE_REROLL', payload: team }), []);
  const loadState = useCallback((state: GameState) => dispatch({ type: 'LOAD_STATE', payload: state }), []);

  // FUMBBL service methods
  const connectToGame = useCallback(async (gameId: number, teamId?: number, spectate?: boolean, fumbblUsername?: string) => {
    if (fumbblServiceRef.current) {
      if (spectate) {
        await fumbblServiceRef.current.connectAsSpectator(gameId, fumbblUsername);
      } else {
        await fumbblServiceRef.current.connectToGame(gameId, teamId, fumbblUsername);
      }
    }
  }, []);

  const connectAsSpectator = useCallback(async (gameId: number, fumbblUsername?: string) => {
    if (fumbblServiceRef.current) {
      await fumbblServiceRef.current.connectAsSpectator(gameId, fumbblUsername);
    }
  }, []);

  const disconnect = useCallback(() => {
    if (fumbblServiceRef.current) {
      fumbblServiceRef.current.disconnect();
      setIsServiceConnected(false);
    }
  }, []);

  const sendChatMessage = useCallback((message: string) => {
    if (fumbblServiceRef.current) {
      fumbblServiceRef.current.sendChatMessage(message);
    }
  }, []);

  const requestReroll = useCallback(() => {
    if (fumbblServiceRef.current) {
      fumbblServiceRef.current.requestReroll();
    }
  }, []);

  const confirmDecision = useCallback((decision: boolean, param?: string) => {
    if (fumbblServiceRef.current) {
      fumbblServiceRef.current.confirmDecision(decision, param);
    }
  }, []);

  const declineDecision = useCallback((param?: string) => {
    if (fumbblServiceRef.current) {
      fumbblServiceRef.current.declineDecision(param);
    }
  }, []);

  // Set OAuth2 credentials and authenticate
  const setCredentials = useCallback(async (clientId: string, clientSecret: string) => {
    if (!fumbblServiceRef.current) {
      throw new Error('FumbblService not initialized');
    }
    try {
      await fumbblServiceRef.current.authenticateWithCredentials(clientId, clientSecret);
    } catch (error) {
      throw error;
    }
  }, []);

  const value: GameContextType = {
    gameState,
    fumbblService: fumbblServiceRef.current,
    isServiceConnected,
    isAuthenticated,
    setState,
    setCredentials,
    selectPlayer,
    selectTeam,
    sendAction,
    addDiceLog,
    addDiceLogs,
    clearDiceLog,
    addChatMessage,
    clearChat,
    updateTimer,
    updateScore,
    updateTurn,
    updateFanAttendance,
    toggleReRoll,
    loadState,
   connectToGame,
   connectAsSpectator,
   disconnect,
    sendChatMessage,
    requestReroll,
    confirmDecision,
    declineDecision,
  };

  return <GameContext.Provider value={value}>{children}</GameContext.Provider>;
}

// -----------------------------------------------------------------------------
// Hook
// -----------------------------------------------------------------------------

export function useGameState(): GameContextType {
  const context = useContext(GameContext);
  if (!context) {
    throw new Error('useGameState must be used within a GameProvider');
  }
  return context;
}