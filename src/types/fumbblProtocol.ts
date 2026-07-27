// =============================================================================
// FUMBBL WebSocket Protocol Types
// Reference: ffbclient protocol.d.ts, ffb CommandSocket.java, jervis-ffb
// =============================================================================

// -----------------------------------------------------------------------------
// WebSocket Message Identifiers (netCommandId)
// -----------------------------------------------------------------------------

/** All WebSocket commands share this base interface */
export interface FumbblCommand {
  netCommandId: string;
}

// -----------------------------------------------------------------------------
// Client Commands (Outgoing)
// -----------------------------------------------------------------------------

/** Client sends version request to server */
export interface ClientRequestVersion extends FumbblCommand {
  netCommandId: 'clientRequestVersion';
}

/** Client joins a game session */
export interface ClientJoin extends FumbblCommand {
  netCommandId: 'clientJoin';
  clientMode: 'player' | 'spectator' | 'replay';  // Join mode (matches ClientMode enum from official client)
  coach: string;          // Coach name/ID (String in Java server)
  password: string;       // Password (empty for most cases)
  gameId: number;         // Game ID
  gameName?: string;      // Optional game name
  teamId?: string;        // Team ID (String in Kotlin, nullable for spectator)
  teamName?: string;      // Optional team name
}

/** Client sends a chat message
 *  Official format uses "talk" field (not "message"):
 *  { netCommandId: 'clientTalk', talk: text }
 */
export interface ClientTalk extends FumbblCommand {
  netCommandId: 'clientTalk';
  talk: string;
}

/** Client closes the session */
export interface ClientCloseSession extends FumbblCommand {
  netCommandId: 'clientCloseSession';
}

/** Client requests a specific action on the field */
export interface ClientAction extends FumbblCommand {
  netCommandId: 'clientAction';
  action: string;         // Action type (e.g., 'block', 'dodge', 'pass')
  params: string[];       // Action parameters
}

/** Client confirms a decision (e.g., reroll, coin toss) */
export interface ClientConfirm extends FumbblCommand {
  netCommandId: 'clientConfirm';
  decision: boolean;
  param?: string;
}

/** Client declines a decision */
export interface ClientDecline extends FumbblCommand {
  netCommandId: 'clientDecline';
  param?: string;
}

/** Client requests reroll */
export interface ClientReroll extends FumbblCommand {
  netCommandId: 'clientReroll';
}

/** Client requests lineup */
export interface ClientLineup extends FumbblCommand {
  netCommandId: 'clientLineup';
}

/** Client sends a touchback confirmation */
export interface ClientTouchback extends FumbblCommand {
  netCommandId: 'clientTouchback';
  decision: boolean;
}

/** Client kicks off */
export interface ClientKickOff extends FumbblCommand {
  netCommandId: 'clientKickOff';
  team: number;           // Which team kicks off
}

/** Client confirms ready to receive game state (sent after serverJoin) */
export interface ClientReady extends FumbblCommand {
  netCommandId: 'clientReady';
}

/** Client sends a ping to keep the connection alive (matches ClientCommandPing from official ffbclient)
 *  The server responds with serverPong and updates the last ping time.
 *  CRITICAL: The official client sends pings every 2 seconds (client.ping.interval=2000 in client.ini).
 *  The server SessionTimeoutTask closes connections where lastPing + 10000ms < current time.
 *  With a 2-second ping interval, we have a 5x safety margin against the 10-second timeout.
 */
export interface ClientPing extends FumbblCommand {
  netCommandId: 'clientPing';
  timestamp: number;
}

/** Client selects player for action */
export interface ClientSelectPlayer extends FumbblCommand {
  netCommandId: 'clientSelectPlayer';
  playerId: number;
  action: string;
}

/** Client sends a general game event response */
export interface ClientGameEvent extends FumbblCommand {
  netCommandId: 'clientGameEvent';
  eventType: string;
  data?: Record<string, unknown>;
}

// -----------------------------------------------------------------------------
// Server Commands (Incoming)
// -----------------------------------------------------------------------------

/** Full game state update — the most important message type */
export interface GameStateUpdate extends FumbblCommand {
  netCommandId: 'gameStateUpdate';
  gameState: GameType;
}

/** List of model changes (incremental updates) */
export interface ModelChangeList extends FumbblCommand {
  netCommandId: 'modelChangeList';
  changes: ModelChangeType[];
}

/** List of game reports (blocks, injuries, etc.) */
export interface ReportList extends FumbblCommand {
  netCommandId: 'reportList';
  reports: ReportType[];
}

/** Animation data */
export interface AnimationCommand extends FumbblCommand {
  netCommandId: 'animation';
  animation: AnimationType;
}

/** Server chat message */
export interface ServerTalk extends FumbblCommand {
  netCommandId: 'serverTalk';
  message: string;
  player?: {
    id: number;
    name: string;
  };
}

/** Server plays a sound effect */
export interface ServerSound extends FumbblCommand {
  netCommandId: 'serverSound';
  soundFile: string;
}

/** Server confirms join was successful (ServerCommandJoin)
 *  Reference: ffb ServerCommandJoin.java, jervis-ffb ServerCommandJoin.kt
 */
export interface ServerJoin extends FumbblCommand {
  netCommandId: 'serverJoin';
  commandNr?: number;           // Command number
  coach: string;                // Coach name
  clientMode: 'player' | 'spectator' | 'replay';
  playerNames: string[];        // Names of players in the game
  spectators: string[];         // Names of spectators
  spectatorCount?: number;      // Count of spectators (legacy)
  replayName?: string;          // Replay name if in replay mode
}

/** Server version response */
export interface ServerVersion extends FumbblCommand {
  netCommandId: 'serverVersion';
  version: string;
  build?: string;
}

/** Server ping for keepalive */
export interface ServerPing extends FumbblCommand {
  netCommandId: 'serverPing';
  timestamp: number;
}

/** Server pong response */
export interface ServerPong extends FumbblCommand {
  netCommandId: 'serverPong';
  timestamp: number;
}

/** Server announces a new player joined */
export interface ServerPlayerJoined extends FumbblCommand {
  netCommandId: 'serverPlayerJoined';
  player: {
    id: number;
    name: string;
    teamId: number;
  };
}

/** Server announces a player left */
export interface ServerPlayerLeft extends FumbblCommand {
  netCommandId: 'serverPlayerLeft';
  playerId: number;
  reason?: string;
}

/** Server notifies about a decision needed */
export interface ServerDecision extends FumbblCommand {
  netCommandId: 'serverDecision';
  decisionType: string;
  description: string;
  options?: string[];
  timeout?: number;
}

/** Server notifies game start */
export interface ServerGameStart extends FumbblCommand {
  netCommandId: 'serverGameStart';
  gameId: number;
  team1: number;
  team2: number;
}

/** Server notifies game end */
export interface ServerGameEnd extends FumbblCommand {
  netCommandId: 'serverGameEnd';
  gameId: number;
  score: { team1: number; team2: number };
  winner?: number;
}

/** Server notifies match state changes */
export interface ServerMatchState extends FumbblCommand {
  netCommandId: 'serverMatchState';
  state: string;
  data?: Record<string, unknown>;
}

/** Server sends a coin toss request */
export interface ServerCoinToss extends FumbblCommand {
  netCommandId: 'serverCoinToss';
  playerId: number;
  winner: number;         // Coin flip winner team ID
}

/** Server sends a kickoff request */
export interface ServerKickOffRequest extends FumbblCommand {
  netCommandId: 'serverKickOffRequest';
  team: number;           // Which team should kick off
}

/** Server sends a kickoff return request */
export interface ServerKickOffReturn extends FumbblCommand {
  netCommandId: 'serverKickOffReturn';
  team: number;           // Which team receives kickoff
}

/** Server sends a turn start notification */
export interface ServerTurnStart extends FumbblCommand {
  netCommandId: 'serverTurnStart';
  turn: number;
  team: number;           // Team whose turn it is
}

/** Server sends a turn end notification */
export interface ServerTurnEnd extends FumbblCommand {
  netCommandId: 'serverTurnEnd';
  turn: number;
}

/** Server sends a halftime notification */
export interface ServerHalftime extends FumbblCommand {
  netCommandId: 'serverHalftime';
  score: { team1: number; team2: number };
}

/** Server sends a touchdown notification */
export interface ServerTouchdown extends FumbblCommand {
  netCommandId: 'serverTouchdown';
  team: number;
  scorer: number;         // Player ID
}

/** Server sends a reroll available notification */
export interface ServerRerollAvailable extends FumbblCommand {
  netCommandId: 'serverRerollAvailable';
  team: number;
  rerollsLeft: number;
}

/** Server sends a lineage update (position changes) */
export interface ServerLineage extends FumbblCommand {
  netCommandId: 'serverLineage';
  lineage: LineageType[];
}

/** Server sends a star player notification */
export interface ServerStarPlayer extends FumbblCommand {
  netCommandId: 'serverStarPlayer';
  playerId: number;
  teamId: number;
}

/** Server sends a blowhard notification */
export interface ServerBlowhard extends FumbblCommand {
  netCommandId: 'serverBlowhard';
  teamId: number;
}

/** Server sends a lineup request */
export interface ServerLineupRequest extends FumbblCommand {
  netCommandId: 'serverLineupRequest';
  teamId: number;
  required: number;
}

/** Server sends a ready notification */
export interface ServerReady extends FumbblCommand {
  netCommandId: 'serverReady';
  teamId: number;
  ready: boolean;
}

/** Server sends a ready up notification */
export interface ServerReadyUp extends FumbblCommand {
  netCommandId: 'serverReadyUp';
  teamId: number;
}

/** Server sends a schedule update */
export interface ServerSchedule extends FumbblCommand {
  netCommandId: 'serverSchedule';
  schedule: ScheduleEntryType[];
}

/** Server sends a slot update */
export interface ServerSlotUpdate extends FumbblCommand {
  netCommandId: 'serverSlotUpdate';
  slot: number;
  playerId?: number;
}

/** Server sends a team lineup */
export interface ServerTeamLineup extends FumbblCommand {
  netCommandId: 'serverTeamLineup';
  teamId: number;
  players: number[];      // Player IDs
}

/** Server sends a token update */
export interface ServerTokenUpdate extends FumbblCommand {
  netCommandId: 'serverTokenUpdate';
  token: string;
}

/** Server sends a weather update */
export interface ServerWeatherUpdate extends FumbblCommand {
  netCommandId: 'serverWeatherUpdate';
  weather: string;
}

/** Server sends a fan attendance update */
export interface ServerFanAttendanceUpdate extends FumbblCommand {
  netCommandId: 'serverFanAttendanceUpdate';
  fanAttendance: number;
  dedicatedFans: { team1: number; team2: number };
}

/** Server sends a re-roll count update */
export interface ServerRerollCountUpdate extends FumbblCommand {
  netCommandId: 'serverRerollCountUpdate';
  team1: number;
  team2: number;
}

/** Server sends a score update */
export interface ServerScoreUpdate extends FumbblCommand {
  netCommandId: 'serverScoreUpdate';
  score: { team1: number; team2: number };
}

/** Server sends a timer update */
export interface ServerTimerUpdate extends FumbblCommand {
  netCommandId: 'serverTimerUpdate';
  timer: number;
}

/** Server sends a turn count update */
export interface ServerTurnCountUpdate extends FumbblCommand {
  netCommandId: 'serverTurnCountUpdate';
  turn: number;
}

/** Server sends a phase update */
export interface ServerPhaseUpdate extends FumbblCommand {
  netCommandId: 'serverPhaseUpdate';
  phase: string;
}

/** Server sends a match info update */
export interface ServerMatchInfoUpdate extends FumbblCommand {
  netCommandId: 'serverMatchInfoUpdate';
  matchInfo: MatchInfoType;
}

/** Server sends a bye notification */
export interface ServerBye extends FumbblCommand {
  netCommandId: 'serverBye';
  teamId: number;
}

/** Server sends a bye accepted notification */
export interface ServerByeAccepted extends FumbblCommand {
  netCommandId: 'serverByeAccepted';
  teamId: number;
}

/** Server sends a bye declined notification */
export interface ServerByeDeclined extends FumbblCommand {
  netCommandId: 'serverByeDeclined';
  teamId: number;
}

/** Server sends a coin toss result */
export interface ServerCoinTossResult extends FumbblCommand {
  netCommandId: 'serverCoinTossResult';
  winner: number;
  choice: 'coinTossWinner' | 'coinTossLoser';
}

/** Server sends a coin toss choice request */
export interface ServerCoinTossChoice extends FumbblCommand {
  netCommandId: 'serverCoinTossChoice';
}

/** Server sends a coin toss choice result */
export interface ServerCoinTossChoiceResult extends FumbblCommand {
  netCommandId: 'serverCoinTossChoiceResult';
  choice: 'coinTossWinner' | 'coinTossLoser';
}

/** Server sends a coin toss choice declined */
export interface ServerCoinTossChoiceDeclined extends FumbblCommand {
  netCommandId: 'serverCoinTossChoiceDeclined';
}

/** Server sends a coin toss kick off team */
export interface ServerCoinTossKickOff extends FumbblCommand {
  netCommandId: 'serverCoinTossKickOff';
  team: number;
}

/** Server sends a coin toss receive */
export interface ServerCoinTossReceive extends FumbblCommand {
  netCommandId: 'serverCoinTossReceive';
  team: number;
}

/** Server sends a coin toss loser */
export interface ServerCoinTossLoser extends FumbblCommand {
  netCommandId: 'serverCoinTossLoser';
  team: number;
}

/** Server sends a coin toss kickoff response */
export interface ServerCoinTossKickOffResponse extends FumbblCommand {
  netCommandId: 'serverCoinTossKickOffResponse';
  team: number;
}

/** Server sends a coin toss receive response */
export interface ServerCoinTossReceiveResponse extends FumbblCommand {
  netCommandId: 'serverCoinTossReceiveResponse';
  team: number;
}

/** Server sends a coin toss loser response */
export interface ServerCoinTossLoserResponse extends FumbblCommand {
  netCommandId: 'serverCoinTossLoserResponse';
  team: number;
}

// Union type of all server commands
export type ServerCommandType =
  | ServerJoin
  | GameStateUpdate
  | ModelChangeList
  | ReportList
  | AnimationCommand
  | ServerTalk
  | ServerSound
  | ServerVersion
  | ServerPing
  | ServerPong
  | ServerPlayerJoined
  | ServerPlayerLeft
  | ServerDecision
  | ServerGameStart
  | ServerGameEnd
  | ServerMatchState
  | ServerCoinToss
  | ServerKickOffRequest
  | ServerKickOffReturn
  | ServerTurnStart
  | ServerTurnEnd
  | ServerHalftime
  | ServerTouchdown
  | ServerRerollAvailable
  | ServerLineage
  | ServerStarPlayer
  | ServerBlowhard
  | ServerLineupRequest
  | ServerReady
  | ServerReadyUp
  | ServerSchedule
  | ServerSlotUpdate
  | ServerTeamLineup
  | ServerTokenUpdate
  | ServerWeatherUpdate
  | ServerFanAttendanceUpdate
  | ServerRerollCountUpdate
  | ServerScoreUpdate
  | ServerTimerUpdate
  | ServerTurnCountUpdate
  | ServerPhaseUpdate
  | ServerMatchInfoUpdate
  | ServerBye
  | ServerByeAccepted
  | ServerByeDeclined
  | ServerCoinTossResult
  | ServerCoinTossChoice
  | ServerCoinTossChoiceResult
  | ServerCoinTossChoiceDeclined
  | ServerCoinTossKickOff
  | ServerCoinTossReceive
  | ServerCoinTossLoser
  | ServerCoinTossKickOffResponse
  | ServerCoinTossReceiveResponse
  | ServerCoinTossLoserResponse;

// Union type of all client commands
export type ClientCommandType =
  | ClientRequestVersion
  | ClientJoin
  | ClientReady
  | ClientPing
  | ClientTalk
  | ClientCloseSession
  | ClientAction
  | ClientConfirm
  | ClientDecline
  | ClientReroll
  | ClientLineup
  | ClientTouchback
  | ClientKickOff
  | ClientSelectPlayer
  | ClientGameEvent;

// -----------------------------------------------------------------------------
// Game Type (Core game state object)
// -----------------------------------------------------------------------------

export interface GameType {
  id: number;
  name: string;
  status: string;
  ruleset: string;
  tournament?: number;
  group?: number;
  round?: number;
  map?: number;
  mapName?: string;
  turn: number;
  turnTeam: number;       // Team ID whose turn it is
  phase: string;
  homeTeam: TeamType;
  awayTeam: TeamType;
  score: { team1: number; team2: number };
  turnScore: { team1: number; team2: number };
  sideChoice: number;     // 1 = home kicked off, 2 = away kicked off
  coinTossWinner: number;
  coinTossLoser: number;
  coinTossChoice: string;
  rerolls: { team1: number; team2: number };
  fanAttendance: number;
  dedicatedFans: { team1: number; team2: number };
  weather: string;
  field: FieldModelType;
  turnData: TurnDataType;
  reports: ReportType[];
  animations: AnimationType[];
  lineage: LineageType[];
  models: ModelType[];
  starPlayer?: { teamId: number; playerId: number };
  blowhard?: { teamId: number };
  ready: { team1: boolean; team2: boolean };
  schedule?: ScheduleEntryType[];
  kickoffTeam?: number;
  receiveTeam?: number;
  byeTeam?: number;
  touchback?: boolean;
  lls?: number;           // Last Lineage Sequence
  mls?: number;           // Last Model Sequence
  rls?: number;           // Last Report Lineage Sequence
  als?: number;           // Last Animation Lineage Sequence
}

// -----------------------------------------------------------------------------
// Team Type
// -----------------------------------------------------------------------------

export interface TeamType {
  id: number;
  name: string;
  shortName: string;
  race: string;
  logo?: string;
  color: string;
  secondaryColor?: string;
  coachId: number;
  coachName?: string;
  isHome: boolean;
  players: PlayerType[];
  lineup: number[];       // Player IDs in starting lineup
  rerolls: number;
  starPlayer?: number;    // Player ID
  blowhard?: number;      // Player ID
  skills?: SkillType[];
}

// -----------------------------------------------------------------------------
// Player Type (WebSocket protocol)
// -----------------------------------------------------------------------------

export interface PlayerType {
  id: number;
  number: number;
  name: string;
  race: string;
  positionId: number;
  positionName?: string;
  ma: number;
  st: number;
  ag: number;
  pa: number;
  av: number;
  skills: SkillType[];
  status: string;         // 'active', 'injured', 'dead', 'rotd', 'doubtful', 'missing'
  statusShort?: string;
  hasBall: boolean;
  fieldX?: number;
  fieldY?: number;
  isStar?: boolean;
  isCaptain?: boolean;
  isStarPlayer?: boolean;
  isBlowhard?: boolean;
  isRerollable?: boolean;
  touched?: boolean;
  blockedThisTurn?: boolean;
  doubledThisTurn?: boolean;
  stumblledThisTurn?: boolean;
  passedThisTurn?: boolean;
  tackledThisTurn?: boolean;
  madThisTurn?: boolean;
  cp?: number;            // Current position (legacy)
  lineup?: boolean;
  teamId: number;
}

// -----------------------------------------------------------------------------
// Skill Type
// -----------------------------------------------------------------------------

export interface SkillType {
  id: number;
  name: string;
  shortName?: string;
  cost?: number;
  type?: string;          // 'basic', 'advanced', 'racial', 'unique'
  teamSkill?: boolean;
  replaceable?: boolean;
}

// -----------------------------------------------------------------------------
// Field Model Type
// -----------------------------------------------------------------------------

export interface FieldModelType {
  id: number;
  type: string;
  endzoneLeft: number;    // 0 or 1 (which side is left endzone)
  endzoneRight: number;
  seedLeft: number[];     // Player IDs in left home endzone
  seedRight: number[];    // Player IDs in right away endzone
  homeEndzoneUp: boolean;
  awayEndzoneUp: boolean;
  windDirection: string;  // 'N', 'S', 'E', 'W', 'NE', 'NW', 'SE', 'SW'
  windStrength: number;   // 0-3
  poison: boolean;
  pw: boolean;            // Power of Ynn / White Hole
  pitch: string;          // 'normal', 'rough', 'waterlogged', 'cramped'
  boundaryLeft: string;   // 'wall', 'crowd', 'guardians', etc.
  boundaryRight: string;
  boundaryTop: string;
  boundaryBottom: string;
  homeGoalPost: string;   // 'left', 'right', 'center'
  awayGoalPost: string;
}

// -----------------------------------------------------------------------------
// Turn Data Type
// -----------------------------------------------------------------------------

export interface TurnDataType {
  teamId: number;
  turn: number;
  phase: string;          // 'setup', 'action', 'scramble', 'end'
  actions: ActionEntryType[];
  availableActions: string[];
  currentAction?: string;
  pendingAction?: ActionEntryType;
  rerollsAvailable: number;
  rerollsUsed: number;
  hasBall: number[];      // Player IDs with the ball
  ballPosition: { x: number; y: number };
  ballCarrier?: number;   // Player ID
  passTarget?: number;    // Player ID
  passOrigin?: number;    // Player ID
  blockAttacker?: number;
  blockDefender?: number;
  blitzPlayer?: number;
  turnScore: number;
  touchdownScore: boolean;
  extraTurn: boolean;
  cascade: boolean;
}

// -----------------------------------------------------------------------------
// Action Entry Type
// -----------------------------------------------------------------------------

export interface ActionEntryType {
  id: number;
  type: string;           // 'move', 'block', 'pass', 'catch', 'dodge', 'tackle', etc.
  player: number;         // Player ID
  target?: number;        // Target player ID
  origin?: { x: number; y: number };
  targetPos?: { x: number; y: number };
  result?: string;
  resultDetail?: string;
  dice?: number[];
  success?: boolean;
  timestamp?: number;
}

// -----------------------------------------------------------------------------
// Report Type (Block reports, injury reports, etc.)
// -----------------------------------------------------------------------------

export interface ReportType {
  id: number;
  type: string;           // 'block', 'injury', 'casualty', 'fumble', 'tackle', 'pass', 'catch', 'knockDown', 'standUp', etc.
  team?: number;
  attacker?: number;      // Player ID
  defender?: number;      // Player ID
  attackerResult?: string;
  defenderResult?: string;
  dice?: number[];
  diceResult?: number[];
  skill?: string;
  description: string;
  turn?: number;
  actionId?: number;
  // Block report specific
  blockAttackerRolls?: number[];
  blockDefenderRolls?: number[];
  blockAttackerResult?: 'success' | 'failure';
  blockDefenderResult?: 'success' | 'failure';
  armorRoll?: number[];
  armorResult?: string;
  injuryRoll?: number[];
  injuryResult?: string;
  injuryName?: string;
  // Casualty specific
  casualty?: boolean;
  unconscious?: boolean;
  // Fumble specific
  fumbler?: number;
  fumblePlayers?: number[];
  // Pass specific
  passType?: string;      // 'long', 'short', 'lateral', 'deflect'
  passOrigin?: number;
  passTarget?: number;
  passResult?: string;
  // Tackle specific
  tackler?: number;
  tumbrler?: number;
  // General
  message?: string;
  color?: string;
}

// -----------------------------------------------------------------------------
// Animation Type
// -----------------------------------------------------------------------------

export interface AnimationType {
  id: number;
  name: string;           // 'move', 'block', 'pass', 'catch', 'dodge', 'tackle', 'fumble', 'injury', etc.
  duration: number;       // milliseconds
  data?: Record<string, unknown>;
  player?: number;        // Player ID
  target?: number;        // Target player ID
  origin?: { x: number; y: number };
  targetPos?: { x: number; y: number };
  path?: { x: number; y: number }[];
}

// -----------------------------------------------------------------------------
// Lineage Type (tracking sequence numbers for incremental updates)
// -----------------------------------------------------------------------------

export interface LineageType {
  id: number;
  type: string;           // 'model', 'report', 'animation', 'player', etc.
  sequence: number;
  data?: Record<string, unknown>;
}

// -----------------------------------------------------------------------------
// Model Type (incremental model changes)
// -----------------------------------------------------------------------------

export interface ModelType {
  id: number;
  type: string;           // 'player', 'team', 'field', 'game', etc.
  action: string;         // 'add', 'update', 'remove'
  data: Record<string, unknown>;
}

// -----------------------------------------------------------------------------
// Model Change Type (matching official ffbclient protocol)
// Reference: ffbclient types/protocol.d.ts and ffb Java server
// -----------------------------------------------------------------------------

export interface ModelChangeType {
  id: number;
  /** The type/identifier of the model change (e.g., 'fieldModelSetPlayerCoordinate') */
  modelChangeId: string;
  /** The key for the change (e.g., player ID for coordinate changes, 'home'/'away' for score) */
  modelChangeKey: string;
  /** The value of the change (e.g., coordinate array, state string, number) */
  modelChangeValue: string | number | boolean | { x: number; y: number } | number[];
  /** Legacy fields for compatibility */
  type?: string;
  action?: string;
  data?: Record<string, unknown>;
  lineage?: number;
}

// -----------------------------------------------------------------------------
// Schedule Entry Type
// -----------------------------------------------------------------------------

export interface ScheduleEntryType {
  id: number;
  type: string;
  time: number;
  data?: Record<string, unknown>;
}

// -----------------------------------------------------------------------------
// Match Info Type
// -----------------------------------------------------------------------------

export interface MatchInfoType {
  id: number;
  name: string;
  status: string;
  ruleset: string;
  tournament?: number;
  group?: number;
  round?: number;
  map?: number;
  mapName?: string;
  homeTeam: {
    id: number;
    name: string;
    shortName: string;
    score: number;
  };
  awayTeam: {
    id: number;
    name: string;
    shortName: string;
    score: number;
  };
  turn: number;
  turnTeam: number;
  phase: string;
  startTime?: number;
  timeRemaining?: number;
}

// -----------------------------------------------------------------------------
// Protocol Message Wrapper (for serialization)
// -----------------------------------------------------------------------------

/** All protocol messages are plain JSON objects with netCommandId */
export type ProtocolMessage = ServerCommandType | ClientCommandType;

/** Type guard to check if a message is a server command */
export function isServerCommand(msg: ProtocolMessage): msg is ServerCommandType {
  const serverCommands: string[] = [
    'serverJoin',
    'gameStateUpdate', 'modelChangeList', 'reportList', 'animation',
    'serverTalk', 'serverSound', 'serverVersion', 'serverPing', 'serverPong',
    'serverPlayerJoined', 'serverPlayerLeft', 'serverDecision', 'serverGameStart',
    'serverGameEnd', 'serverMatchState', 'serverCoinToss', 'serverKickOffRequest',
    'serverKickOffReturn', 'serverTurnStart', 'serverTurnEnd', 'serverHalftime',
    'serverTouchdown', 'serverRerollAvailable', 'serverLineage', 'serverStarPlayer',
    'serverBlowhard', 'serverLineupRequest', 'serverReady', 'serverReadyUp',
    'serverSchedule', 'serverSlotUpdate', 'serverTeamLineup', 'serverTokenUpdate',
    'serverWeatherUpdate', 'serverFanAttendanceUpdate', 'serverRerollCountUpdate',
    'serverScoreUpdate', 'serverTimerUpdate', 'serverTurnCountUpdate',
    'serverPhaseUpdate', 'serverMatchInfoUpdate', 'serverBye', 'serverByeAccepted',
    'serverByeDeclined', 'serverCoinTossResult', 'serverCoinTossChoice',
    'serverCoinTossChoiceResult', 'serverCoinTossChoiceDeclined',
    'serverCoinTossKickOff', 'serverCoinTossReceive', 'serverCoinTossLoser',
    'serverCoinTossKickOffResponse', 'serverCoinTossReceiveResponse',
    'serverCoinTossLoserResponse',
  ];
  return serverCommands.includes(msg.netCommandId);
}

/** Type guard to check if a message is a client command */
export function isClientCommand(msg: ProtocolMessage): msg is ClientCommandType {
  return !isServerCommand(msg);
}

/** Type guard to check if a message is a game state update */
export function isGameStateUpdate(msg: ProtocolMessage): msg is GameStateUpdate {
  return msg.netCommandId === 'gameStateUpdate';
}

/** Type guard to check if a message is a model change list */
export function isModelChangeList(msg: ProtocolMessage): msg is ModelChangeList {
  return msg.netCommandId === 'modelChangeList';
}

/** Type guard to check if a message is a report list */
export function isReportList(msg: ProtocolMessage): msg is ReportList {
  return msg.netCommandId === 'reportList';
}

/** Type guard to check if a message is an animation */
export function isAnimation(msg: ProtocolMessage): msg is AnimationCommand {
  return msg.netCommandId === 'animation';
}

/** Type guard to check if a message is a chat message */
export function isServerTalk(msg: ProtocolMessage): msg is ServerTalk {
  return msg.netCommandId === 'serverTalk';
}

/** Type guard to check if a message is a sound effect */
export function isServerSound(msg: ProtocolMessage): msg is ServerSound {
  return msg.netCommandId === 'serverSound';
}

// -----------------------------------------------------------------------------
// WebSocket Connection States
// -----------------------------------------------------------------------------

export enum WebSocketState {
  CONNECTING = 0,
  OPEN = 1,
  CLOSING = 2,
  CLOSED = 3,
}

// -----------------------------------------------------------------------------
// WebSocket Service Configuration
// -----------------------------------------------------------------------------

export interface FumbblWebSocketConfig {
  /** WebSocket server URL */
  url: string;
  /** Reconnection delay in milliseconds */
  reconnectDelay: number;
  /** Maximum reconnection attempts (-1 for infinite) */
  maxReconnectAttempts: number;
  /** Ping interval in milliseconds (0 to disable) */
  pingInterval: number;
  /** Message size limit in bytes */
  maxMessageSize: number;
}

export const DEFAULT_WEBSOCKET_CONFIG: FumbblWebSocketConfig = {
  url: 'ws://fumbbl.com:22223/command',
  reconnectDelay: 3000,
  maxReconnectAttempts: -1, // Infinite
  pingInterval: 5000,
  maxMessageSize: 64 * 1024, // 64KB
};

// -----------------------------------------------------------------------------
// WebSocket Event Callbacks
// -----------------------------------------------------------------------------

export interface FumbblWebSocketCallbacks {
  /** Called when connection is established */
  onOpen?: () => void;
  /** Called when connection is closed */
  onClose?: (code: number, reason: string) => void;
  /** Called when an error occurs */
  onError?: (error: Error) => void;
  /** Called when join is confirmed by the server */
  onJoin?: (coach: string, clientMode: 'player' | 'spectator' | 'replay', playerNames: string[], spectatorNames: string[]) => void;
  /** Called when a full game state update is received */
  onGameStateUpdate?: (gameState: GameType) => void;
  /** Called when model changes are received */
  onModelChanges?: (changes: ModelChangeType[]) => void;
  /** Called when reports are received */
  onReports?: (reports: ReportType[]) => void;
  /** Called when animations are received */
  onAnimations?: (animations: AnimationType[]) => void;
  /** Called when a chat message is received */
  onChatMessage?: (message: string, player?: { id: number; name: string }) => void;
  /** Called when a sound effect is received */
  onSound?: (soundFile: string) => void;
  /** Called when a raw message is received (for custom handling) */
  onMessage?: (msg: ProtocolMessage) => void;
  /** Called when reconnection is attempted */
  onReconnect?: (attempt: number) => void;
  /** Called when reconnection succeeds */
  onReconnectSuccess?: () => void;
}