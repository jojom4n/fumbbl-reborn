// =============================================================================
// FUMBBL Services Index — Barrel Export
// =============================================================================

// --- Protocol Types ---
export type {
  FumbblCommand,
  ClientRequestVersion,
  ClientJoin,
  ClientTalk,
  ClientCloseSession,
  ClientAction,
  ClientConfirm,
  ClientDecline,
  ClientReroll,
  ClientLineup,
  ClientTouchback,
  ClientKickOff,
  ClientSelectPlayer,
  ClientGameEvent,
  ClientCommandType,
  ServerCommandType,
  ProtocolMessage,
  GameStateUpdate,
  ModelChangeList,
  ReportList,
  AnimationCommand,
  ServerTalk,
  ServerSound,
  ServerVersion,
  ServerPing,
  ServerPong,
  ServerPlayerJoined,
  ServerPlayerLeft,
  ServerDecision,
  ServerGameStart,
  ServerGameEnd,
  ServerMatchState,
  ServerCoinToss,
  ServerKickOffRequest,
  ServerKickOffReturn,
  ServerTurnStart,
  ServerTurnEnd,
  ServerHalftime,
  ServerTouchdown,
  ServerRerollAvailable,
  ServerLineage,
  ServerStarPlayer,
  ServerBlowhard,
  ServerLineupRequest,
  ServerReady,
  ServerReadyUp,
  ServerSchedule,
  ServerSlotUpdate,
  ServerTeamLineup,
  ServerTokenUpdate,
  ServerWeatherUpdate,
  ServerFanAttendanceUpdate,
  ServerRerollCountUpdate,
  ServerScoreUpdate,
  ServerTimerUpdate,
  ServerTurnCountUpdate,
  ServerPhaseUpdate,
  ServerMatchInfoUpdate,
  ServerBye,
  ServerByeAccepted,
  ServerByeDeclined,
  ServerCoinTossResult,
  ServerCoinTossChoice,
  ServerCoinTossChoiceResult,
  ServerCoinTossChoiceDeclined,
  ServerCoinTossKickOff,
  ServerCoinTossReceive,
  ServerCoinTossLoser,
  ServerCoinTossKickOffResponse,
  ServerCoinTossReceiveResponse,
  ServerCoinTossLoserResponse,
  GameType,
  TeamType,
  PlayerType,
  SkillType,
  FieldModelType,
  TurnDataType,
  ActionEntryType,
  ReportType,
  AnimationType,
  LineageType,
  ModelType,
  ModelChangeType,
  ScheduleEntryType,
  MatchInfoType,
  WebSocketState,
  FumbblWebSocketConfig,
  FumbblWebSocketCallbacks,
  isServerCommand,
  isClientCommand,
  isGameStateUpdate,
  isModelChangeList,
  isReportList,
  isAnimation,
  isServerTalk,
  isServerSound,
} from '../types/fumbblProtocol';
export type { DEFAULT_WEBSOCKET_CONFIG } from '../types/fumbblProtocol';

// --- API Types ---
export type {
  FumbblApiConfig,
  ApiResponse,
  IdNamePair,
  Coach,
  CoachTeams,
  Team,
  Player as PlayerApi,
  Position,
  Roster,
  RunningGame,
  Match,
  RecentMatch,
  ScheduledMatch,
  UpcomingMatch,
  Tournament,
  BoxTrophyStanding,
  BoxTrophyMatch,
  BoxTrophyResponse,
  GraphData,
  PlayerStatistics,
  UpcomingTournaments,
  OAuthIdentityResponse,
  OAuthTokenResponse,
  SessionTokenResponse,
} from '../types/fumbblApiTypes';
export type { DEFAULT_API_CONFIG } from '../types/fumbblApiTypes';

// --- Services ---
export { FumbblAuthService, fumbblAuth } from './fumbblAuth';
export { FumbblWebSocket, createFumbblWebSocket } from './fumbblWebSocket';
export { FumbblRestApi, fumbblApi, createFumbblApi } from './fumbblRestApi';
export { FumbblService, createFumbblService } from './fumbblService';
export type { FumbblServiceConfig, FumbblServiceState } from './fumbblService';

// --- Game Model (ffbclient architecture) ---
export { FumbblGameModel } from './fumbblGameModel';
export type { FumbblCommandHandlerCallbacks } from './fumbblCommandHandler';
export { FumbblCommandHandler } from './fumbblCommandHandler';

// --- Utilities ---
export { LZString, compressToUTF16, decompressFromUTF16, compressToBase64, decompressFromBase64, isCompressed, smartDecompress } from '../utils/lzString';

// --- Game State Mapper (legacy - replaced by FumbblGameModel) ---
export {
  mapFumbblGameState,
  mapModelChanges,
  mapReportsToDiceLog,
} from './gameStateMapper';
