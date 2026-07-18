// =============================================================================
// GameContext — Global Game State Provider
// =============================================================================

import { createContext, useContext, useReducer, useCallback, useEffect, ReactNode } from 'react';
import {
  GameState,
  Player,
  BloodBowlAction,
  DiceLogEntry,
  ChatMessage,
  Weather,
  FanAttendance,
  ReRolls,
} from '../types/bloodbowl';

// -----------------------------------------------------------------------------
// Mock Data
// -----------------------------------------------------------------------------

const createMockTeam1Players = (): Player[] => [
  { id: 1, name: 'Grom Flayer', number: 1, race: 'Orc', position: 'ap', status: 'active', skills: ['AG', 'D'], ma: 6, st: 4, ag: 3, pa: 3, av: 10 },
  { id: 2, name: 'Karg Blitzer', number: 2, race: 'Orc', position: 'bl', status: 'active', skills: ['ST', 'T'], ma: 6, st: 5, ag: 2, pa: 2, av: 10 },
  { id: 3, name: 'Snikchy Blitzer', number: 3, race: 'Orc', position: 'bl', status: 'active', skills: ['AG'], ma: 6, st: 4, ag: 3, pa: 3, av: 10 },
  { id: 4, name: 'Throt Block', number: 4, race: 'Orc', position: 'bl', status: 'active', skills: ['B', 'ST'], ma: 5, st: 5, ag: 2, pa: 2, av: 12 },
  { id: 5, name: 'Mog Halfback', number: 5, race: 'Orc', position: 'rb', status: 'injured', skills: ['AG', 'C'], ma: 7, st: 3, ag: 3, pa: 4, av: 10 },
  { id: 6, name: 'Grimskull Runaround', number: 6, race: 'Orc', position: 'wr', status: 'active', skills: ['AG', 'D', 'C'], ma: 7, st: 3, ag: 4, pa: 2, av: 10 },
  { id: 7, name: 'Fingle Running Thug', number: 7, race: 'Orc', position: 'wr', status: 'active', skills: ['AG'], ma: 7, st: 3, ag: 3, pa: 2, av: 10 },
  { id: 8, name: 'Nasty Chikey', number: 8, race: 'Orc', position: 'c', status: 'active', skills: ['ST'], ma: 5, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 9, name: 'Urk Stone-Eyed', number: 9, race: 'Orc', position: 'lh', status: 'active', skills: [], ma: 5, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 10, name: 'Mash Bouncer', number: 10, race: 'Orc', position: 'rh', status: 'active', skills: ['ST'], ma: 5, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 11, name: 'Ghazgull Goalie', number: 11, race: 'Orc', position: 'g', status: 'active', skills: ['AN'], ma: 5, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 12, name: 'Shunk Large', number: 12, race: 'Orc', position: 'sub', status: 'active', skills: ['ST'], ma: 5, st: 5, ag: 2, pa: 2, av: 12 },
  { id: 13, name: 'Dak Backstabber', number: 13, race: 'Orc', position: 'sub', status: 'active', skills: ['AG', 'NK'], ma: 6, st: 4, ag: 3, pa: 3, av: 10 },
  { id: 14, name: 'Pogo Block', number: 14, race: 'Orc', position: 'sub', status: 'active', skills: ['B'], ma: 5, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 15, name: 'Ripted Block', number: 15, race: 'Orc', position: 'sub', status: 'active', skills: [], ma: 5, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 16, name: 'Zug Anvil', number: 16, race: 'Orc', position: 'sub', status: 'active', skills: ['ST'], ma: 5, st: 5, ag: 2, pa: 2, av: 12 },
];

const createMockTeam2Players = (): Player[] => [
  { id: 101, name: 'Sylvan Glader', number: 1, race: 'Elf', position: 'ap', status: 'active', skills: ['AG', 'D', 'C', 'SV'], ma: 8, st: 2, ag: 4, pa: 4, av: 10 },
  { id: 102, name: 'Aenil Runaround', number: 2, race: 'Elf', position: 'wr', status: 'active', skills: ['AG', 'D', 'C'], ma: 8, st: 2, ag: 4, pa: 2, av: 10 },
  { id: 103, name: 'Evershader', number: 3, race: 'Elf', position: 'wr', status: 'active', skills: ['AG', 'D'], ma: 8, st: 2, ag: 3, pa: 2, av: 10 },
  { id: 104, name: 'Lightstep', number: 4, race: 'Elf', position: 'rb', status: 'active', skills: ['AG', 'C'], ma: 7, st: 3, ag: 3, pa: 3, av: 10 },
  { id: 105, name: 'Nimblefellow', number: 5, race: 'Elf', position: 'c', status: 'active', skills: ['ST'], ma: 6, st: 3, ag: 3, pa: 2, av: 10 },
  { id: 106, name: 'Truefly', number: 6, race: 'Elf', position: 'lh', status: 'active', skills: [], ma: 6, st: 3, ag: 3, pa: 2, av: 10 },
  { id: 107, name: 'Clearwater', number: 7, race: 'Elf', position: 'rh', status: 'active', skills: ['ST'], ma: 6, st: 3, ag: 3, pa: 2, av: 10 },
  { id: 108, name: 'Valourheart', number: 8, race: 'Elf', position: 'bl', status: 'active', skills: ['B', 'ST'], ma: 6, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 109, name: 'Fairhelms', number: 9, race: 'Elf', position: 'bl', status: 'active', skills: ['B', 'AN'], ma: 6, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 110, name: 'Waverider', number: 10, race: 'Elf', position: 'g', status: 'active', skills: ['K', 'AN'], ma: 6, st: 3, ag: 3, pa: 2, av: 10 },
  { id: 111, name: 'Shadefell', number: 11, race: 'Elf', position: 'sub', status: 'active', skills: ['AG', 'D'], ma: 8, st: 2, ag: 3, pa: 2, av: 10 },
  { id: 112, name: 'Glenlough', number: 12, race: 'Elf', position: 'sub', status: 'active', skills: ['ST'], ma: 6, st: 3, ag: 3, pa: 2, av: 10 },
  { id: 113, name: 'Dawnstrider', number: 13, race: 'Elf', position: 'sub', status: 'active', skills: ['AG'], ma: 7, st: 3, ag: 3, pa: 3, av: 10 },
  { id: 114, name: 'Brightblade', number: 14, race: 'Elf', position: 'sub', status: 'active', skills: ['B'], ma: 6, st: 4, ag: 2, pa: 2, av: 12 },
  { id: 115, name: 'Goldleaf', number: 15, race: 'Elf', position: 'sub', status: 'active', skills: [], ma: 6, st: 3, ag: 3, pa: 2, av: 10 },
  { id: 116, name: 'Silvertongue', number: 16, race: 'Elf', position: 'sub', status: 'active', skills: ['ST'], ma: 6, st: 3, ag: 3, pa: 2, av: 10 },
];

const createMockWeather = (): Weather => ({
  type: 'raining',
  icon: '🌧️',
  description: 'Raining',
});

const createMockFanAttendance = (): FanAttendance => ({
  total: 7,
  dedicatedFans: { team1: 4, team2: 3 },
});

const createMockReRolls = (): ReRolls => ({ team1: 2, team2: 1 });

const createInitialState = (): GameState => {
  const team1Players = createMockTeam1Players();
  const team2Players = createMockTeam2Players();

  // Assign field positions for display
  team1Players.forEach((p, i) => {
    if (i < 11) {
      p.fieldX = Math.floor(Math.random() * 4) + 1; // Columns 1-4
      p.fieldY = i;
    }
  });
  team2Players.forEach((p, i) => {
    if (i < 11) {
      p.fieldX = Math.floor(Math.random() * 4) + 11; // Columns 11-14
      p.fieldY = i;
    }
  });

  return {
    score: { team1: 2, team2: 1 },
    turn: 6,
    phase: 'regular',
    reRolls: createMockReRolls(),
    timer: 81, // 01:21
    weather: createMockWeather(),
    fanAttendance: createMockFanAttendance(),
    team1: {
      id: 'orc',
      name: 'Orcs',
      race: 'Orc',
      logoUrl: 'https://fumbbl.com/api/team/orc/logo',
      players: team1Players,
      color: '#4a7c3f',
      secondaryColor: '#2d5a1e',
    },
    team2: {
      id: 'elf',
      name: 'Elves',
      race: 'Elf',
      logoUrl: 'https://fumbbl.com/api/team/elf/logo',
      players: team2Players,
      color: '#c4a35a',
      secondaryColor: '#8b7332',
    },
    team1Players,
    team2Players,
    field: {
      markers: [],
      ballPosition: { x: 8, y: 5 },
    },
    ballPosition: { x: 8, y: 5 },
    selectedPlayer: null,
    selectedTeam: 'team1',
    diceLog: [
      { id: 1, type: 'action', timestamp: Date.now() - 60000, text: 'Orc Blitzer #5 uses Blitz', color: 'text-gray-300', turn: 6 },
      { id: 2, type: 'block_roll', timestamp: Date.now() - 55000, text: '[Block Roll] -> Selected', color: 'text-yellow-400', dice: [6, 4], target: 3, result: 'success', turn: 6 },
      { id: 3, type: 'armor', timestamp: Date.now() - 50000, text: 'Elf #8 Armor Roll: 9 -> Armor Broken!', color: 'text-red-400', dice: [9], target: 4, result: 'failure', turn: 6 },
      { id: 4, type: 'injury', timestamp: Date.now() - 45000, text: 'Elf #8 Injury Roll: 10 -> CASUALTY (Smashed Hip) - Unconscious', color: 'text-red-500 font-bold', dice: [10], target: 6, result: 'failure', turn: 6 },
    ],
    chatMessages: [
      { id: 1, sender: 'FUMBBL_Player', senderColor: 'text-green-400', text: 'Bel colpo!', timestamp: Date.now() - 50000, type: 'general' },
      { id: 2, sender: 'Orc_Admin', senderColor: 'text-red-400', text: 'RIP Sylvan Glader', timestamp: Date.now() - 40000, type: 'general' },
    ],
    isLive: true,
    lastUpdate: Date.now(),
  };
};

// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------

type GameAction =
  | { type: 'SET_STATE'; payload: Partial<GameState> }
  | { type: 'SELECT_PLAYER'; payload: Player | null }
  | { type: 'SELECT_TEAM'; payload: 'team1' | 'team2' }
  | { type: 'SEND_ACTION'; payload: { action: BloodBowlAction; playerId: number } }
  | { type: 'ADD_DICE_LOG'; payload: DiceLogEntry }
  | { type: 'CLEAR_DICE_LOG' }
  | { type: 'ADD_CHAT_MESSAGE'; payload: ChatMessage }
  | { type: 'CLEAR_CHAT' }
  | { type: 'UPDATE_TIMER'; payload: number }
  | { type: 'UPDATE_SCORE'; payload: { team1: number; team2: number } }
  | { type: 'UPDATE_TURN'; payload: number }
  | { type: 'UPDATE_FAN_ATTENDANCE'; payload: FanAttendance }
  | { type: 'TOGGLE_REROLL'; payload: 'team1' | 'team2' }
  | { type: 'LOAD_STATE'; payload: GameState };

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
      const newLogEntry: DiceLogEntry = {
        id: Date.now(),
        type: 'action',
        timestamp: Date.now(),
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
    case 'CLEAR_DICE_LOG':
      return { ...state, diceLog: [], lastUpdate: Date.now() };
    case 'ADD_CHAT_MESSAGE':
      return { ...state, chatMessages: [...state.chatMessages, action.payload], lastUpdate: Date.now() };
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
      return action.payload;
    default:
      return state;
  }
}

// -----------------------------------------------------------------------------
// Context
// -----------------------------------------------------------------------------

interface GameContextType {
  gameState: GameState;
  // Actions
  setState: (payload: Partial<GameState>) => void;
  selectPlayer: (player: Player | null) => void;
  selectTeam: (team: 'team1' | 'team2') => void;
  sendAction: (action: BloodBowlAction, playerId: number) => void;
  addDiceLog: (entry: DiceLogEntry) => void;
  clearDiceLog: () => void;
  addChatMessage: (message: ChatMessage) => void;
  clearChat: () => void;
  updateTimer: (timer: number) => void;
  updateScore: (score: { team1: number; team2: number }) => void;
  updateTurn: (turn: number) => void;
  updateFanAttendance: (fa: FanAttendance) => void;
  toggleReRoll: (team: 'team1' | 'team2') => void;
  loadState: (state: GameState) => void;
}

const GameContext = createContext<GameContextType | undefined>(undefined);

// -----------------------------------------------------------------------------
// Provider
// -----------------------------------------------------------------------------

export function GameProvider({ children }: { children: ReactNode }) {
  const [gameState, dispatch] = useReducer(gameReducer, null, createInitialState);

  // Memoized action creators
  const setState = useCallback((payload: Partial<GameState>) => dispatch({ type: 'SET_STATE', payload }), []);
  const selectPlayer = useCallback((player: Player | null) => dispatch({ type: 'SELECT_PLAYER', payload: player }), []);
  const selectTeam = useCallback((team: 'team1' | 'team2') => dispatch({ type: 'SELECT_TEAM', payload: team }), []);
  const sendAction = useCallback((action: BloodBowlAction, playerId: number) => dispatch({ type: 'SEND_ACTION', payload: { action, playerId } }), []);
  const addDiceLog = useCallback((entry: DiceLogEntry) => dispatch({ type: 'ADD_DICE_LOG', payload: entry }), []);
  const clearDiceLog = useCallback(() => dispatch({ type: 'CLEAR_DICE_LOG' }), []);
  const addChatMessage = useCallback((message: ChatMessage) => dispatch({ type: 'ADD_CHAT_MESSAGE', payload: message }), []);
  const clearChat = useCallback(() => dispatch({ type: 'CLEAR_CHAT' }), []);
  const updateTimer = useCallback((timer: number) => dispatch({ type: 'UPDATE_TIMER', payload: timer }), []);
  const updateScore = useCallback((score: { team1: number; team2: number }) => dispatch({ type: 'UPDATE_SCORE', payload: score }), []);
  const updateTurn = useCallback((turn: number) => dispatch({ type: 'UPDATE_TURN', payload: turn }), []);
  const updateFanAttendance = useCallback((fa: FanAttendance) => dispatch({ type: 'UPDATE_FAN_ATTENDANCE', payload: fa }), []);
  const toggleReRoll = useCallback((team: 'team1' | 'team2') => dispatch({ type: 'TOGGLE_REROLL', payload: team }), []);
  const loadState = useCallback((state: GameState) => dispatch({ type: 'LOAD_STATE', payload: state }), []);

  // Timer countdown effect (when game is live)
  useEffect(() => {
    if (!gameState.isLive || gameState.timer <= 0) return;
    const interval = setInterval(() => {
      dispatch({ type: 'UPDATE_TIMER', payload: gameState.timer - 1 });
    }, 1000);
    return () => clearInterval(interval);
  }, [gameState.isLive, gameState.timer]);

  const value: GameContextType = {
    gameState,
    setState,
    selectPlayer,
    selectTeam,
    sendAction,
    addDiceLog,
    clearDiceLog,
    addChatMessage,
    clearChat,
    updateTimer,
    updateScore,
    updateTurn,
    updateFanAttendance,
    toggleReRoll,
    loadState,
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