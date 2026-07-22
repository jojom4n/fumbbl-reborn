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

// -----------------------------------------------------------------------------
// Unique ID generator (avoids Date.now() duplicates)
// Uses timestamp + counter, returns number for DiceLogEntry/ChatMessage compatibility
// -----------------------------------------------------------------------------
let _idCounter = 0;
const generateUniqueId = (): number => {
  _idCounter += 1;
  // Use timestamp with counter offset to avoid duplicates within same millisecond
  return Number(`${Date.now()}${String(_idCounter).padStart(6, '0')}`);
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
      // Merge incoming state with defaults to ensure all fields exist
      return { ...createInitialState(), ...action.payload, lastUpdate: Date.now() };
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

  // Timer countdown effect (when game is live)
  useEffect(() => {
    if (!gameState.isLive || gameState.timer <= 0) return;
    const interval = setInterval(() => {
      dispatch({ type: 'UPDATE_TIMER', payload: gameState.timer - 1 });
    }, 1000);
    return () => clearInterval(interval);
  }, [gameState.isLive, gameState.timer]);

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