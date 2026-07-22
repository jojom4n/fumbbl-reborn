// =============================================================================
// FUMBBL REST API Types
// Reference: fumbbl.com/apidoc.json (OpenAPI 3.0), Fumbbl-Api C# DTOs
// =============================================================================

// -----------------------------------------------------------------------------
// Common Types
// -----------------------------------------------------------------------------

/** Simple ID/name pair used throughout the API */
export interface IdNamePair {
  id: number;
  name: string;
}

/** Pagination parameters */
export interface PaginationParams {
  limit?: number;
  offset?: number;
}

/** Paginated response wrapper */
export interface PaginatedResponse<T> {
  data: T[];
  total: number;
  limit: number;
  offset: number;
}

// -----------------------------------------------------------------------------
// Coach Types
// -----------------------------------------------------------------------------

/** Coach information from the API */
export interface Coach {
  id: number;
  name: string;
  rating: number;         // Coach rating
  wins: number;
  losses: number;
  draws: number;
  touchDowns: number;
  fans: number;           // Dedicated fans
  xp: number;             // Experience points
  skill: number;          // Coach skill level
  avatar?: string;        // Avatar URL
  country?: string;       // Country code
  teamId?: number;        // Current team ID
}

/** Coach data returned from /api/coach/teams/{coach} */
export interface CoachTeams {
  coach: Coach;
  teams: TeamSummary[];
}

// -----------------------------------------------------------------------------
// Team Types
// -----------------------------------------------------------------------------

/** Team summary (minimal info) */
export interface TeamSummary {
  id: number;
  name: string;
  shortName: string;
  race: string;
  logo?: string;
  color?: string;
  rating?: number;
}

/** Full team information from the API */
export interface Team {
  id: number;
  name: string;
  shortName: string;
  race: string;
  logo?: string;
  color: string;
  secondaryColor?: string;
  coachId: number;
  coachName?: string;
  rating: number;
  wins: number;
  losses: number;
  draws: number;
  touchDowns: number;
  fans: number;
  dedicatedFans: number;
  skills: TeamSkill[];
  players: Player[];
  roster: Roster;
  createdAt?: string;
  updatedAt?: string;
}

/** Team skill */
export interface TeamSkill {
  id: number;
  name: string;
  shortName: string;
  cost: number;
}

// -----------------------------------------------------------------------------
// Player Types
// -----------------------------------------------------------------------------

/** Player information from the API */
export interface Player {
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
  skills: PlayerSkill[];
  status: string;         // 'active', 'injured', 'dead', 'rotd', 'doubtful', 'missing'
  isStar: boolean;
  isCaptain: boolean;
  xp: number;             // Experience points
  mv: number;             // Man Value
  teamId?: number;
  avatar?: string;        // Avatar URL
}

/** Player skill */
export interface PlayerSkill {
  id: number;
  name: string;
  shortName: string;
  cost: number;
  type: string;           // 'basic', 'advanced', 'racial', 'unique'
  teamSkill: boolean;
  replaceable: boolean;
}

/** Player statistics */
export interface PlayerStatistics {
  playerId: number;
  playerName: string;
  teamId: number;
  teamName: string;
  gamesPlayed: number;
  touchDowns: number;
  assists: number;
  catches: number;
  blocksWon: number;
  blocksLost: number;
  mudballs: number;
  MVP: number;
  MvM: number;            // Man vs Man
  fanFactor: number;
  starGame: boolean;
}

// -----------------------------------------------------------------------------
// Position Types
// -----------------------------------------------------------------------------

/** Position information from the API */
export interface Position {
  id: number;
  name: string;
  shortName: string;
  mv: number;             // Man Value
  skills: PositionSkill[];
  maxStats: {
    ma: number;
    st: number;
    ag: number;
    pa: number;
  };
}

/** Position skill */
export interface PositionSkill {
  id: number;
  name: string;
  shortName: string;
  cost: number;
}

// -----------------------------------------------------------------------------
// Roster Types
// -----------------------------------------------------------------------------

/** Roster from the API */
export interface Roster {
  id: number;
  name: string;
  ruleset: string;
  race: string;
  skills: RosterSkill[];
  players: RosterPlayer[];
  coachingStaff: CoachingStaffMember[];
  totalCost: number;
}

/** Roster player */
export interface RosterPlayer {
  id: number;
  number: number;
  name: string;
  positionId: number;
  positionName: string;
  ma: number;
  st: number;
  ag: number;
  pa: number;
  av: number;
  skills: RosterPlayerSkill[];
  mv: number;
  xp: number;
}

/** Roster player skill */
export interface RosterPlayerSkill {
  id: number;
  name: string;
  shortName: string;
  cost: number;
}

/** Roster skill */
export interface RosterSkill {
  id: number;
  name: string;
  shortName: string;
  cost: number;
}

/** Coaching staff member */
export interface CoachingStaffMember {
  id: number;
  name: string;
  role: string;
  cost: number;
}

// -----------------------------------------------------------------------------
// Running Game Types
// -----------------------------------------------------------------------------

/** Running game (current live game) */
export interface RunningGame {
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
  turnTeam: number;
  phase: string;
  homeTeam: TeamSummary;
  awayTeam: TeamSummary;
  score: { team1: number; team2: number };
  startTime: string;
  timeRemaining?: number;
  fanAttendance?: number;
  weather?: string;
  isLive: boolean;
}

// -----------------------------------------------------------------------------
// Match Types
// -----------------------------------------------------------------------------

/** Recent match summary */
export interface RecentMatch {
  id: number;
  homeTeam: TeamSummary;
  awayTeam: TeamSummary;
  score: { team1: number; team2: number };
  date: string;
  tournament?: string;
  ruleset: string;
}

/** Scheduled match */
export interface ScheduledMatch {
  id: number;
  homeTeam: TeamSummary;
  awayTeam: TeamSummary;
  scheduledAt: string;
  tournament?: string;
  group?: string;
  round?: string;
}

/** Upcoming match */
export interface UpcomingMatch {
  id: number;
  homeTeam: TeamSummary;
  awayTeam: TeamSummary;
  scheduledAt: string;
  tournament?: string;
}

/** Full match details */
export interface Match {
  id: number;
  name: string;
  status: string;
  ruleset: string;
  tournament?: number;
  group?: number;
  round?: number;
  map?: number;
  mapName?: string;
  homeTeam: Team;
  awayTeam: Team;
  score: { team1: number; team2: number };
  turn: number;
  phase: string;
  startTime: string;
  endTime?: string;
  recentMatches: RecentMatch[];
  scheduledMatches: ScheduledMatch[];
}

// -----------------------------------------------------------------------------
// Tournament Types
// -----------------------------------------------------------------------------

/** Tournament information */
export interface Tournament {
  id: number;
  name: string;
  ruleset: string;
  status: string;         // 'upcoming', 'active', 'completed'
  start_date: string;
  end_date?: string;
  maxTeams: number;
  currentTeams: number;
  format: string;         // 'single_elimination', 'double_elimination', 'round_robin'
  map?: number;
  organizer?: string;
  groups?: TournamentGroup[];
  brackets?: TournamentBracket[];
}

/** Tournament group */
export interface TournamentGroup {
  id: number;
  name: string;
  teams: TournamentTeamStanding[];
  matches: RecentMatch[];
}

/** Tournament team standing */
export interface TournamentTeamStanding {
  team: TeamSummary;
  played: number;
  won: number;
  lost: number;
  drawn: number;
  touchDowns: number;
  touchDownsAgainst: number;
  points: number;
}

/** Tournament bracket entry */
export interface TournamentBracket {
  round: number;
  matches: TournamentMatch[];
}

/** Tournament match */
export interface TournamentMatch {
  id: number;
  homeTeam: TeamSummary;
  awayTeam: TeamSummary;
  score?: { team1: number; team2: number };
  winner?: TeamSummary;
  scheduledAt?: string;
  completedAt?: string;
}

/** Group tournament info */
export interface GroupTournament {
  id: number;
  name: string;
  ruleset: string;
  status: string;
  groups: TournamentGroup[];
}

/** Upcoming tournaments */
export interface UpcomingTournaments {
  tournaments: Tournament[];
}

// -----------------------------------------------------------------------------
// Box Trophy Types
// -----------------------------------------------------------------------------

/** Box Trophy standing */
export interface BoxTrophyStanding {
  rank: number;
  coach: Coach;
  rating: number;
  wins: number;
  losses: number;
  draws: number;
  touchDowns: number;
  fanFactor: number;
  gamesPlayed: number;
}

/** Box Trophy match */
export interface BoxTrophyMatch {
  id: number;
  homeTeam: TeamSummary;
  awayTeam: TeamSummary;
  score: { team1: number; team2: number };
  date: string;
}

/** Box Trophy standings response */
export interface BoxTrophyResponse {
  standings: BoxTrophyStanding[];
  recent: BoxTrophyMatch[];
}

// -----------------------------------------------------------------------------
// Graph Data Types
// -----------------------------------------------------------------------------

/** Graph data point */
export interface GraphDataPoint {
  label: string;
  value: number;
}

/** Graph data */
export interface GraphData {
  title: string;
  description: string;
  points: GraphDataPoint[];
  type: string;           // 'line', 'bar', 'pie', etc.
}

// -----------------------------------------------------------------------------
// API Response Types
// -----------------------------------------------------------------------------

/** Generic API response wrapper */
export interface ApiResponse<T = unknown> {
  success: boolean;
  data?: T;
  error?: string;
  message?: string;
}

/** API error response */
export interface ApiErrorResponse {
  success: false;
  error: string;
  code?: number;
  details?: Record<string, unknown>;
}

// -----------------------------------------------------------------------------
// OAuth Types
// -----------------------------------------------------------------------------

/** OAuth2 token request */
export interface OAuthTokenRequest {
  grant_type: 'client_credentials';
  client_id: string;
  client_secret: string;
}

/** OAuth2 token response */
export interface OAuthTokenResponse {
  access_token: string;
  token_type: 'Bearer';
  expires_in: number;     // Seconds until expiration
}

/** Session token request */
export interface SessionTokenRequest {
  // No body required - uses Authorization header
}

/** Session token response */
export interface SessionTokenResponse {
  session_token: string;
  match_id?: string;
  team_id?: number;
}

/** OAuth identity response */
export interface OAuthIdentityResponse {
  client_id: string;
  scope: string;
  user_id?: number;
  user_name?: string;
}

// -----------------------------------------------------------------------------
// API Endpoint Paths
// -----------------------------------------------------------------------------

/** All available REST API endpoints */
export const API_ENDPOINTS = {
  // Authentication
  oauthToken: '/api/oauth/token' as const,
  authGetToken: '/api/auth/getToken' as const,
  authVerify: '/api/auth/verify' as const,
  oauthIdentity: '/api/oauth/identity' as const,

  // Match
  matchCurrent: '/api/match/current' as const,
  matchList: '/api/match/list/{lastId}' as const,
  matchGet: '/api/match/get/{matchId}' as const,

  // Gamestate
  gamestateCheck: '/api/gamestate/check/{t1}/{t2}' as const,
  gamestateOptions: '/api/gamestate/options/{t1}/{t2}' as const,
  gamestateSchedule: '/api/gamestate/schedule/{t1}/{t2}' as const,

  // Team
  teamGet: '/api/team/{id}' as const,
  teamLogo: '/api/team/{id}/logo' as const,
  teamSearch: '/api/team/search/{term}' as const,

  // Coach
  coachSearch: '/api/coach/search/{term}' as const,
  coachGet: '/api/coach/get/{id}' as const,
  coachTeams: '/api/coach/teams/{coach}' as const,

  // Player
  playerGet: '/api/player/{id}' as const,
  positionGet: '/api/position/get/{id}' as const,
  rosterList: '/api/roster/list/{ruleset}' as const,
  rosterGet: '/api/roster/{id}' as const,
  skillGet: '/api/skill/get/{id}' as const,

  // Tournament
  groupTournaments: '/api/group/tournaments/{id}' as const,
  groupUpcoming: '/api/group/upcoming/{id}' as const,
  tournamentGet: '/api/tournament/{id}' as const,

  // Stats
  statsGraph: '/api/stats/graph/{id}' as const,
  boxtrophyStandings: '/api/boxtrophy/standings' as const,
  boxtrophyRecent: '/api/boxtrophy/recent' as const,

  // Other
  bbcodeParse: '/api/bbcode/parse' as const,
} as const;

// -----------------------------------------------------------------------------
// API Client Configuration
// -----------------------------------------------------------------------------

export interface FumbblApiConfig {
  /** Base URL for the FUMBBL REST API */
  baseUrl: string;
  /** OAuth2 access token */
  accessToken?: string;
  /** Session token (for WebSocket join) */
  sessionToken?: string;
  /** Request timeout in milliseconds */
  timeout: number;
  /** Rate limit: max requests per window */
  rateLimit: number;
  /** Rate limit window in milliseconds */
  rateLimitWindow: number;
}

export const DEFAULT_API_CONFIG: FumbblApiConfig = {
  baseUrl: 'https://fumbbl.com/api',
  timeout: 10000, // 10 seconds
  rateLimit: 5,
  rateLimitWindow: 1000, // 5 requests per 1000ms
};