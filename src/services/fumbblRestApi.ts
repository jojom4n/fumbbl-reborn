// =============================================================================
// FUMBBL REST API Service
// Handles REST API calls with rate limiting (5 req/s)
// Reference: FUMBBLUI SerializedApiRunner pattern
// =============================================================================

import {
  FumbblApiConfig,
  DEFAULT_API_CONFIG,
  ApiResponse,
  Team,
  TeamSummary,
  Player,
  Coach,
  CoachTeams,
  RunningGame,
  Match,
  RecentMatch,
  Tournament,
  BoxTrophyResponse,
  GraphData,
  Roster,
  Position,
  PlayerStatistics,
  UpcomingTournaments,
  OAuthIdentityResponse,
} from '../types/fumbblApiTypes';

// -----------------------------------------------------------------------------
// Serialized Request Runner (Rate Limiting)
// -----------------------------------------------------------------------------

/**
 * SerializedApiRunner ensures we don't exceed the rate limit
 * Pattern from FUMBBLUI: 5 requests per 1000ms
 */
class SerializedApiRunner {
  private queue: Array<() => void> = [];
  private processing = false;
  private timestamps: number[] = [];
  private readonly maxRequests: number;
  private readonly windowMs: number;

  constructor(maxRequests: number, windowMs: number) {
    this.maxRequests = maxRequests;
    this.windowMs = windowMs;
  }

  /**
   * Execute a request, respecting the rate limit
   */
  async execute<T>(requestFn: () => Promise<T>): Promise<T> {
    return new Promise<T>((resolve, reject) => {
      this.queue.push(() => {
        requestFn()
          .then(resolve)
          .catch(reject);
      });
      this.process();
    });
  }

  private process(): void {
    if (this.processing || this.queue.length === 0) {
      if (this.queue.length === 0) this.processing = false;
      return;
    }

    this.processing = true;
    const now = Date.now();

    // Remove timestamps outside the current window
    this.timestamps = this.timestamps.filter(ts => now - ts < this.windowMs);

    // Check if we've hit the rate limit
    if (this.timestamps.length >= this.maxRequests) {
      // Wait until the oldest timestamp exits the window
      const waitTime = this.windowMs - (now - this.timestamps[0]) + 10;
      setTimeout(() => {
        this.process();
      }, waitTime);
      return;
    }

    // Record this request
    this.timestamps.push(now);
    const fn = this.queue.shift();
    this.processing = false;

    if (fn) {
      fn();
    }

    // Process next in queue
    if (this.queue.length > 0) {
      // Small delay between requests to spread them out
      setTimeout(() => this.process(), 50);
    }
  }
}

// -----------------------------------------------------------------------------
// FUMBBL REST API Service
// -----------------------------------------------------------------------------

export class FumbblRestApi {
  private config: FumbblApiConfig;
  private runner: SerializedApiRunner;

  constructor(config?: Partial<FumbblApiConfig>) {
    this.config = { ...DEFAULT_API_CONFIG, ...config };
    this.runner = new SerializedApiRunner(
      this.config.rateLimit,
      this.config.rateLimitWindow
    );
  }

  // ---------------------------------------------------------------------------
  // HTTP Helper Methods
  // ---------------------------------------------------------------------------

  private async request<T>(path: string, options: RequestInit = {}): Promise<T> {
    const url = `${this.config.baseUrl}${path}`;
    const headers: Record<string, string> = {
      'Accept': 'application/json',
      ...this.config.accessToken ? { 'Authorization': `Bearer ${this.config.accessToken}` } : {},
      ...options.headers as Record<string, string>,
    };

    const requestInit: RequestInit = {
      ...options,
      headers,
      signal: AbortSignal.timeout(this.config.timeout),
    };

    return this.runner.execute<T>(async () => {
      const response = await fetch(url, requestInit);

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`API request failed: ${response.status} - ${errorText} (${url})`);
      }

      return response.json();
    });
  }

  private get<T>(path: string): Promise<T> {
    return this.request<T>(path, { method: 'GET' });
  }

  private post<T>(path: string, body?: unknown): Promise<T> {
    return this.request<T>(path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: body ? JSON.stringify(body) : undefined,
    });
  }

  // DELETE HTTP method placeholder - not currently used in any endpoints

  // ---------------------------------------------------------------------------
  // Match Endpoints
  // ---------------------------------------------------------------------------

  /** Get current live matches */
  getMatches(): Promise<ApiResponse<RunningGame[]>> {
    return this.get('/api/match/current');
  }

  /** Get matches since lastId */
  getMatchList(lastId: number): Promise<ApiResponse<RecentMatch[]>> {
    return this.get(`/api/match/list/${lastId}`);
  }

  /** Get match details by ID */
  getMatch(matchId: number): Promise<ApiResponse<Match>> {
    return this.get(`/api/match/get/${matchId}`);
  }

  // ---------------------------------------------------------------------------
  // Gamestate Endpoints
  // ---------------------------------------------------------------------------

  /** Check if a game exists between two teams */
  checkGamestate(team1: number, team2: number): Promise<ApiResponse<unknown>> {
    return this.get(`/api/gamestate/check/${team1}/${team2}`);
  }

  /** Get gamestate options for two teams */
  getGamestateOptions(team1: number, team2: number): Promise<ApiResponse<unknown>> {
    return this.get(`/api/gamestate/options/${team1}/${team2}`);
  }

  /** Get scheduled games between two teams */
  getGamestateSchedule(team1: number, team2: number): Promise<ApiResponse<unknown>> {
    return this.get(`/api/gamestate/schedule/${team1}/${team2}`);
  }

  // ---------------------------------------------------------------------------
  // Team Endpoints
  // ---------------------------------------------------------------------------

  /** Get team by ID */
  getTeam(id: number): Promise<ApiResponse<Team>> {
    return this.get(`/api/team/${id}`);
  }

  /** Get team logo URL */
  getTeamLogoUrl(id: number): string {
    return `${this.config.baseUrl.replace('/api', '')}/api/team/${id}/logo`;
  }

  /** Search teams by term */
  searchTeams(term: string): Promise<ApiResponse<TeamSummary[]>> {
    return this.get(`/api/team/search/${encodeURIComponent(term)}`);
  }

  // ---------------------------------------------------------------------------
  // Coach Endpoints
  // ---------------------------------------------------------------------------

  /** Search coaches by term */
  searchCoaches(term: string): Promise<ApiResponse<Coach[]>> {
    return this.get(`/api/coach/search/${encodeURIComponent(term)}`);
  }

  /** Get coach by ID */
  getCoach(id: number): Promise<ApiResponse<Coach>> {
    return this.get(`/api/coach/get/${id}`);
  }

  /** Get coach and their teams */
  getCoachTeams(coachId: number): Promise<ApiResponse<CoachTeams>> {
    return this.get(`/api/coach/teams/${coachId}`);
  }

  // ---------------------------------------------------------------------------
  // Player Endpoints
  // ---------------------------------------------------------------------------

  /** Get player by ID */
  getPlayer(id: number): Promise<ApiResponse<Player>> {
    return this.get(`/api/player/${id}`);
  }

  /** Get player statistics */
  getPlayerStats(id: number): Promise<ApiResponse<PlayerStatistics>> {
    return this.get(`/api/player/${id}/stats`);
  }

  // ---------------------------------------------------------------------------
  // Position Endpoints
  // ---------------------------------------------------------------------------

  /** Get position by ID */
  getPosition(id: number): Promise<ApiResponse<Position>> {
    return this.get(`/api/position/get/${id}`);
  }

  // ---------------------------------------------------------------------------
  // Roster Endpoints
  // ---------------------------------------------------------------------------

  /** List rosters for a ruleset */
  listRosters(ruleset: string): Promise<ApiResponse<Roster[]>> {
    return this.get(`/api/roster/list/${encodeURIComponent(ruleset)}`);
  }

  /** Get roster by ID */
  getRoster(id: number): Promise<ApiResponse<Roster>> {
    return this.get(`/api/roster/${id}`);
  }

  // ---------------------------------------------------------------------------
  // Skill Endpoints
  // ---------------------------------------------------------------------------

  /** Get skill by ID */
  getSkill(id: number): Promise<ApiResponse<unknown>> {
    return this.get(`/api/skill/get/${id}`);
  }

  // ---------------------------------------------------------------------------
  // Tournament Endpoints
  // ---------------------------------------------------------------------------

  /** Get group tournaments by ID */
  getGroupTournaments(id: number): Promise<ApiResponse<Tournament>> {
    return this.get(`/api/group/tournaments/${id}`);
  }

  /** Get upcoming tournaments by ID */
  getUpcomingTournaments(id: number): Promise<ApiResponse<UpcomingTournaments>> {
    return this.get(`/api/group/upcoming/${id}`);
  }

  /** Get tournament by ID */
  getTournament(id: number): Promise<ApiResponse<Tournament>> {
    return this.get(`/api/tournament/${id}`);
  }

  // ---------------------------------------------------------------------------
  // Stats Endpoints
  // ---------------------------------------------------------------------------

  /** Get graph data */
  getGraphData(id: number): Promise<ApiResponse<GraphData>> {
    return this.get(`/api/stats/graph/${id}`);
  }

  /** Get Box Trophy standings */
  getBoxTrophyStandings(): Promise<ApiResponse<BoxTrophyResponse>> {
    return this.get('/api/boxtrophy/standings');
  }

  /** Get recent Box Trophy matches */
  getBoxTrophyRecent(): Promise<ApiResponse<BoxTrophyResponse>> {
    return this.get('/api/boxtrophy/recent');
  }

  // ---------------------------------------------------------------------------
  // Other Endpoints
  // ---------------------------------------------------------------------------

  /** Parse BBCode */
  parseBBCode(text: string): Promise<ApiResponse<string>> {
    return this.post('/api/bbcode/parse', { text });
  }

  /** Get OAuth identity */
  getIdentity(): Promise<ApiResponse<OAuthIdentityResponse>> {
    return this.get('/api/oauth/identity');
  }

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------

  /** Update the access token */
  setAccessToken(token: string): void {
    this.config.accessToken = token;
  }

  /** Update the session token */
  setSessionToken(token: string): void {
    this.config.sessionToken = token;
  }

  /** Get the current access token */
  getAccessToken(): string | undefined {
    return this.config.accessToken;
  }

  /** Get the current session token */
  getSessionToken(): string | undefined {
    return this.config.sessionToken;
  }
}

// -----------------------------------------------------------------------------
// Singleton instance for use across the app
// -----------------------------------------------------------------------------

export const fumbblApi = new FumbblRestApi();

// -----------------------------------------------------------------------------
// Factory Function
// -----------------------------------------------------------------------------

/**
 * Create a new FUMBBL REST API instance with custom configuration
 */
export function createFumbblApi(config?: Partial<FumbblApiConfig>): FumbblRestApi {
  return new FumbblRestApi(config);
}