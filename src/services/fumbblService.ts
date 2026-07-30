// =============================================================================
// FUMBBL Service — Main Integration Service
// Integrates Auth, WebSocket, REST API, and Game State Mapper
// =============================================================================

import {
  GameState,
  ChatMessage,
} from '../types/bloodbowl';

import {
  FumbblAuthService,
  FumbblWebSocket,
  FumbblRestApi,
  FumbblApiConfig,
  FumbblWebSocketConfig,
  FumbblWebSocketCallbacks,
} from '.';

import { FumbblGameModel } from './fumbblGameModel';

// -----------------------------------------------------------------------------
// FUMBBL Service Configuration
// -----------------------------------------------------------------------------

export interface FumbblServiceConfig {
  /** REST API configuration */
  apiConfig?: Partial<FumbblApiConfig>;
  /** WebSocket configuration */
  websocketConfig?: Partial<FumbblWebSocketConfig>;
  /** OAuth2 credentials */
  clientId?: string;
  clientSecret?: string;
  /** Username provided at login */
  username?: string;
  /** Game ID to connect to */
  gameId?: number;
  /** Team ID to join as */
  teamId?: number;
  /** Spectate mode (no teamId needed) */
  spectate?: boolean;
}

// -----------------------------------------------------------------------------
// FUMBBL Service State
// -----------------------------------------------------------------------------

export interface FumbblServiceState {
  /** Current game state mapped from FUMBBL */
  gameState: GameState;
  /** Connection status */
  isConnected: boolean;
  /** Authentication status */
  isAuthenticated: boolean;
  /** Authentication error message */
  error: string | null;
  /** Loading state */
  isLoading: boolean;
}

// -----------------------------------------------------------------------------
// FUMBBL Service
// =============================================================================

export class FumbblService {
  private auth: FumbblAuthService;
  private websocket: FumbblWebSocket | null = null;
  private api: FumbblRestApi;
  private config: FumbblServiceConfig;
  private state: FumbblServiceState;
  private callbacks: {
    onStateUpdate?: (state: GameState) => void;
    onModelChanges?: (changes: any[]) => void;
    onReports?: (reports: any[]) => void;
    onConnectionChange?: (isConnected: boolean) => void;
    onAuthChange?: (isAuthenticated: boolean) => void;
    onError?: (error: string) => void;
  } = {};

  /**
   * Get the underlying game model (read-only access)
   */
  getGameModel(): FumbblGameModel | null {
    return this.websocket?.getGameModel() ?? null;
  }

  /**
   * Get the authenticated username
   */
  getAuthUsername(): string {
    return this.auth.getUsername();
  }

  /**
   * Set the username manually (e.g., from user input at login)
   */
  setUsername(username: string): void {
    this.auth.setUsername(username);
  }

  constructor(config?: FumbblServiceConfig) {
    this.config = config || {};
    this.auth = new FumbblAuthService();
    this.api = new FumbblRestApi();

    this.state = {
      gameState: this.createInitialState(),
      isConnected: false,
      isAuthenticated: false,
      error: null,
      isLoading: false,
    };

    // Sync API config
    if (this.config.apiConfig) {
      this.api = new FumbblRestApi(this.config.apiConfig);
    }
  }

  // ---------------------------------------------------------------------------
  // Initialization
  // ---------------------------------------------------------------------------

  /**
   * Initialize the service with configuration
   */
  async initialize(): Promise<void> {
    this.setState({ isLoading: true });

    try {
      // Set username if provided
      if (this.config.username) {
        this.auth.setUsername(this.config.username);
      }
      // Try to authenticate if credentials provided
      if (this.config.clientId && this.config.clientSecret) {
        await this.authenticate();
      }

      // Try to connect WebSocket if gameId provided
      if (this.config.gameId) {
        await this.connectToGame(this.config.gameId, this.config.teamId);
      }

      this.setState({ isLoading: false });
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Failed to initialize';
      this.setState({ isLoading: false, error: errorMsg });
      this.callbacks.onError?.(errorMsg);
    }
  }

  // ---------------------------------------------------------------------------
  // Authentication
  // ---------------------------------------------------------------------------

  /**
   * Authenticate with FUMBBL server using OAuth2
   */
  async authenticate(): Promise<void> {
    if (!this.config.clientId || !this.config.clientSecret) {
      throw new Error('OAuth2 credentials not provided');
    }

    this.setState({ isLoading: true, error: null });

    try {
      const result = await this.auth.fullAuthFlow(this.config.clientId, this.config.clientSecret);

      // Set tokens in services
      this.auth.setAccessToken(result.accessToken);
      this.auth.setSessionToken(result.sessionToken);
      this.api.setAccessToken(result.accessToken);
      this.api.setSessionToken(result.sessionToken);

      this.setState({
        isAuthenticated: true,
        isLoading: false,
      });
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Authentication failed';
      this.setState({ isLoading: false, error: errorMsg });
      this.callbacks.onError?.(errorMsg);
      throw error;
    }
  }

  /**
   * Connect to a game as spectator (no teamId needed)
   * IMPORTANT: Fetches a fresh session token for each connection (tokens are one-time use)
   * @param gameId - The game ID to connect to
   * @param fumbblUsername - The FUMBBL username to use as "coach" in clientJoin
   */
  async connectAsSpectator(gameId: number, fumbblUsername?: string): Promise<void> {
    console.log('[FumbblService] connectAsSpectator called, gameId:', gameId, 'fumbblUsername:', fumbblUsername);

    // IMPORTANT: Always fetch a fresh session token for each connection
    // Session tokens are one-time use and get invalidated after first use
    let accessToken = this.auth.getAccessToken();
    if (!accessToken) {
      // If no access token, we need to re-authenticate
      if (!this.config.clientId || !this.config.clientSecret) {
        throw new Error('No OAuth2 credentials available');
      }
      console.log('[FumbblService] No access token, re-authenticating...');
      await this.authenticate();
      accessToken = this.auth.getAccessToken();
      if (!accessToken) {
        throw new Error('Failed to obtain access token');
      }
    }

    // Fetch a fresh session token
    console.log('[FumbblService] Fetching fresh session token for connection...');
    let sessionToken: string;
    try {
      const tokenResponse = await fetch('https://fumbbl.com/api/auth/getToken', {
        method: 'POST',
        headers: {
          'Accept': 'application/json',
          'Authorization': `Bearer ${accessToken}`,
          'Content-Type': 'application/json',
        },
      });

      if (!tokenResponse.ok) {
        throw new Error(`Failed to get session token: ${tokenResponse.status}`);
      }

      const tokenText = await tokenResponse.text();
      console.log('[FumbblService] Fresh session token response (raw):', tokenText);
      console.log('[FumbblService] Fresh session token response length:', tokenText.length);

      // CRITICAL: The API returns a JSON-encoded string: "ddec93be..."
      // We MUST parse it with JSON.parse to remove the JSON quotes!
      // If we just trim(), we get "\"ddec93be...\"" with quotes still included
      try {
        const parsed = JSON.parse(tokenText);
        if (typeof parsed === 'string') {
          sessionToken = parsed; // JSON.parse removes the quotes automatically
          console.log('[FumbblService] Fresh session token (parsed):', sessionToken);
        } else {
          throw new Error(`Unexpected token format: ${typeof parsed}`);
        }
      } catch (parseError) {
        console.warn('[FumbblService] JSON.parse failed, trying trim:', parseError);
        sessionToken = tokenText.trim();
      }

      // Validate token doesn't contain JSON quotes
      if (sessionToken.startsWith('"') || sessionToken.startsWith('\\')) {
        console.error('[FumbblService] WARNING: Token still contains quotes! Raw was:', tokenText);
        sessionToken = sessionToken.replace(/^["\\]+|["\\]+$/g, '');
        console.log('[FumbblService] Stripped quotes from token:', sessionToken);
      }

      // Update stored session token
      this.auth.setSessionToken(sessionToken);
      console.log('[FumbblService] Fresh session token obtained:', sessionToken.substring(0, 20) + '...');
    } catch (error) {
      console.error('[FumbblService] Failed to fetch fresh session token:', error);
      throw error;
    }

    // Use provided username or try to get from OAuth2 identity
    let username = fumbblUsername || this.auth.getUsername();
    if (!username) {
      try {
        const identity = await this.auth.getIdentity();
        username = identity.user_name || identity.client_id || '';
        console.log('[FumbblService] OAuth2 identity — user_name:', username);
      } catch (error) {
        console.warn('[FumbblService] Failed to get OAuth2 identity, using Spectator:', error);
        username = 'Spectator';
      }
    }

    this.setState({ isLoading: true, error: null });

    // Create WebSocket connection with callbacks that integrate GameModel → GameContext
    this.websocket = new FumbblWebSocket(
      this.config.websocketConfig,
      this.createWebSocketCallbacks()
    );

    // Configure WebSocket for spectator mode with username
    this.websocket.setAuthToken(sessionToken);
    this.websocket.setUsername(username);
    this.websocket.setGameId(gameId);
    // No teamId for spectator mode

    // Enable spectator mode and connect
    this.websocket.enableSpectatorMode();
    this.websocket.connect();

    // Wait for connection
    await new Promise((resolve) => setTimeout(resolve, 1000));

    this.setState({
      isConnected: this.websocket?.isReady() || false,
      isLoading: false,
    });

    this.callbacks.onConnectionChange?.(this.websocket?.isReady() || false);
  }

  /**
   * Authenticate with new credentials (called from debug panel)
   * Gets session token from /api/auth/getToken endpoint
   */
  async authenticateWithCredentials(clientId: string, clientSecret: string): Promise<void> {
    this.setState({ isLoading: true, error: null });

    // Update config
    this.config.clientId = clientId;
    this.config.clientSecret = clientSecret;

    try {
      console.log('[FumbblService] Starting authentication with credentials...');

      // Step 1: Get access token via OAuth2
      await this.auth.authenticate(clientId, clientSecret);
      console.log('[FumbblService] OAuth2 auth completed, accessToken:', this.auth.getAccessToken()?.substring(0, 20) + '...');

      // Step 2: Get username from OAuth2 identity
      let username = '';
      try {
        const identity = await this.auth.getIdentity();
        username = identity.user_name || identity.client_id || '';
        console.log('[FumbblService] OAuth2 identity — user_name:', username, 'client_id:', identity.client_id);
      } catch (error) {
        console.warn('[FumbblService] Failed to get OAuth2 identity:', error);
      }

      // Step 3: Get session token from /api/auth/getToken endpoint
      // Direct fetch to see the raw response format
      const authTokenUrl = 'https://fumbbl.com/api/auth/getToken';
      console.log('[FumbblService] Fetching session token from:', authTokenUrl);

      const tokenResponse = await fetch(authTokenUrl, {
        method: 'POST',
        headers: {
          'Accept': 'application/json',
          'Authorization': `Bearer ${this.auth.getAccessToken()}`,
          'Content-Type': 'application/json',
        },
      });

      console.log('[FumbblService] Token response status:', tokenResponse.status);
      console.log('[FumbblService] Token response headers:', Object.fromEntries(tokenResponse.headers.entries()));

      const tokenText = await tokenResponse.text();
      console.log('[FumbblService] Token response raw text:', tokenText);

      let sessionToken: string | null = null;

      if (tokenResponse.ok && tokenText) {
        console.log('[FumbblService] Token response text:', tokenText);

        try {
          const tokenData = JSON.parse(tokenText);
          console.log('[FumbblService] Token response parsed:', tokenData);
          console.log('[FumbblService] Token response type:', typeof tokenData);

          // The API returns a plain string, not an object!
          // Response format: "b691fa57fdd43a5b0a1dfc62e50e9c81"
          if (typeof tokenData === 'string') {
            sessionToken = tokenData;
            console.log('[FumbblService] Session token extracted as plain string:', sessionToken);
          } else if (typeof tokenData === 'object' && tokenData !== null) {
            // Try various possible field names for the session token
            sessionToken =
              (tokenData as any).session_token ||
              (tokenData as any).sessionToken ||
              (tokenData as any).token ||
              (tokenData as any).session ||
              null;

            // If response has a data wrapper, check inside
            if (!sessionToken && (tokenData as any).data) {
              console.log('[FumbblService] Checking data wrapper:', JSON.stringify((tokenData as any).data));
              sessionToken =
                ((tokenData as any).data as any).session_token ||
                ((tokenData as any).data as any).sessionToken ||
                ((tokenData as any).data as any).token ||
                null;
            }
          } else {
            console.warn('[FumbblService] Unexpected tokenData type:', typeof tokenData);
          }

          console.log('[FumbblService] Final extracted session token:', sessionToken);
        } catch (parseError) {
          console.warn('[FumbblService] Failed to parse token response as JSON:', tokenText);
          // If JSON.parse fails, try using the raw text as the token
          const trimmed = tokenText.trim();
          if (trimmed.length > 10 && !trimmed.startsWith('{') && !trimmed.startsWith('[')) {
            sessionToken = trimmed;
            console.log('[FumbblService] Used raw text as session token:', sessionToken);
          }
        }
      }

      if (!sessionToken) {
        console.error('[FumbblService] Could not extract session token from response');
      }

      // Set tokens in services
      this.auth.setAccessToken(this.auth.getAccessToken()!);
      if (sessionToken) {
        this.auth.setSessionToken(sessionToken);
        this.api.setSessionToken(sessionToken);
        console.log('[FumbblService] Session token set successfully');
      }

      // Store username for later use in WebSocket join
      this.auth._username = username;

      this.setState({
        isAuthenticated: true,
        isLoading: false,
      });

      this.callbacks.onAuthChange?.(true);
      this.callbacks.onConnectionChange?.(this.websocket?.isReady() || false);
      console.log('[FumbblService] Authentication complete, isAuthenticated:', true, 'username:', username);
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Authentication failed';
      console.error('[FumbblService] Authentication error:', errorMsg);
      this.setState({ isLoading: false, error: errorMsg, isAuthenticated: false });
      this.callbacks.onAuthChange?.(false);
      this.callbacks.onError?.(errorMsg);
      throw error;
    }
  }

  // ---------------------------------------------------------------------------
  // WebSocket Connection
  // ---------------------------------------------------------------------------

  /**
   * Connect to a game via WebSocket
   * IMPORTANT: Fetches a fresh session token for each connection (tokens are one-time use)
   * @param gameId - The game ID to connect to
   * @param teamId - Optional team ID to join as a player
   * @param fumbblUsername - The FUMBBL username to use as "coach" in clientJoin
   */
  async connectToGame(gameId: number, teamId?: number, fumbblUsername?: string): Promise<void> {
    console.log('[FumbblService] connectToGame called, gameId:', gameId, 'teamId:', teamId, 'fumbblUsername:', fumbblUsername);

    // IMPORTANT: Always fetch a fresh session token for each connection
    // Session tokens are one-time use and get invalidated after first use
    let accessToken = this.auth.getAccessToken();
    if (!accessToken) {
      if (!this.config.clientId || !this.config.clientSecret) {
        throw new Error('No OAuth2 credentials available');
      }
      console.log('[FumbblService] No access token, re-authenticating...');
      await this.authenticate();
      accessToken = this.auth.getAccessToken();
      if (!accessToken) {
        throw new Error('Failed to obtain access token');
      }
    }

    // Fetch a fresh session token
    console.log('[FumbblService] Fetching fresh session token for connection...');
    let sessionToken: string;
    try {
      const tokenResponse = await fetch('https://fumbbl.com/api/auth/getToken', {
        method: 'POST',
        headers: {
          'Accept': 'application/json',
          'Authorization': `Bearer ${accessToken}`,
          'Content-Type': 'application/json',
        },
      });

      if (!tokenResponse.ok) {
        throw new Error(`Failed to get session token: ${tokenResponse.status}`);
      }

      const tokenText = await tokenResponse.text();
      console.log('[FumbblService] Fresh session token response (raw):', tokenText);
      console.log('[FumbblService] Fresh session token response length:', tokenText.length);

      // CRITICAL: The API returns a JSON-encoded string: "ddec93be..."
      // We MUST parse it with JSON.parse to remove the JSON quotes!
      try {
        const parsed = JSON.parse(tokenText);
        if (typeof parsed === 'string') {
          sessionToken = parsed; // JSON.parse removes the quotes automatically
          console.log('[FumbblService] Fresh session token (parsed):', sessionToken);
        } else {
          throw new Error(`Unexpected token format: ${typeof parsed}`);
        }
      } catch (parseError) {
        console.warn('[FumbblService] JSON.parse failed, trying trim:', parseError);
        sessionToken = tokenText.trim();
      }

      // Validate token doesn't contain JSON quotes
      if (sessionToken.startsWith('"') || sessionToken.startsWith('\\')) {
        console.error('[FumbblService] WARNING: Token still contains quotes! Raw was:', tokenText);
        sessionToken = sessionToken.replace(/^["\\]+|["\\]+$/g, '');
        console.log('[FumbblService] Stripped quotes from token:', sessionToken);
      }

      this.auth.setSessionToken(sessionToken);
      console.log('[FumbblService] Fresh session token obtained:', sessionToken.substring(0, 20) + '...');
    } catch (error) {
      console.error('[FumbblService] Failed to fetch fresh session token:', error);
      throw error;
    }

    // Use provided username or try to get from OAuth2 identity
    let username = fumbblUsername || this.auth.getUsername();
    if (!username) {
      try {
        const identity = await this.auth.getIdentity();
        username = identity.user_name || identity.client_id || '';
        console.log('[FumbblService] OAuth2 identity — user_name:', username);
      } catch (error) {
        console.warn('[FumbblService] Failed to get OAuth2 identity, using Coach:', error);
        username = 'Coach';
      }
    }

    this.setState({ isLoading: true, error: null });

    // Create WebSocket connection with callbacks that integrate GameModel → GameContext
    this.websocket = new FumbblWebSocket(
      this.config.websocketConfig,
      this.createWebSocketCallbacks()
    );

    // Configure WebSocket with username
    this.websocket.setAuthToken(sessionToken);
    this.websocket.setUsername(username);
    this.websocket.setGameId(gameId);
    if (teamId) {
      this.websocket.setTeamId(teamId);
    }

    // Connect
    this.websocket.connect();

    // Wait for connection
    await new Promise((resolve) => setTimeout(resolve, 1000));

    this.setState({
      isConnected: this.websocket?.isReady() || false,
      isLoading: false,
    });

    this.callbacks.onConnectionChange?.(this.websocket?.isReady() || false);
  }

  /**
   * Disconnect from the current game
   */
  disconnect(): void {
    if (this.websocket) {
      this.websocket.disconnect();
      this.websocket = null;
    }
    this.setState({ isConnected: false });
    this.callbacks.onConnectionChange?.(false);
  }

  /**
   * Get WebSocket connection state
   */
  isWebSocketConnected(): boolean {
    return this.websocket?.isReady() || false;
  }

  // ---------------------------------------------------------------------------
  // REST API Operations
  // ---------------------------------------------------------------------------

  /**
   * Fetch a team by ID
   */
  async fetchTeam(teamId: number) {
    this.setState({ isLoading: true, error: null });
    try {
      const response = await this.api.getTeam(teamId);
      this.setState({ isLoading: false });
      return response;
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Failed to fetch team';
      this.setState({ isLoading: false, error: errorMsg });
      this.callbacks.onError?.(errorMsg);
      throw error;
    }
  }

  /**
   * Fetch current live matches
   */
  async fetchLiveMatches() {
    this.setState({ isLoading: true, error: null });
    try {
      const response = await this.api.getMatches();
      this.setState({ isLoading: false });
      return response;
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Failed to fetch matches';
      this.setState({ isLoading: false, error: errorMsg });
      this.callbacks.onError?.(errorMsg);
      throw error;
    }
  }

  /**
   * Fetch match details
   */
  async fetchMatchDetails(matchId: number) {
    this.setState({ isLoading: true, error: null });
    try {
      const response = await this.api.getMatch(matchId);
      this.setState({ isLoading: false });
      return response;
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Failed to fetch match details';
      this.setState({ isLoading: false, error: errorMsg });
      this.callbacks.onError?.(errorMsg);
      throw error;
    }
  }

  // ---------------------------------------------------------------------------
  // Game Actions (via WebSocket)
  // ---------------------------------------------------------------------------

  /**
   * Send a chat message
   */
  sendChatMessage(message: string): void {
    if (this.websocket?.isReady()) {
      this.websocket.sendChatMessage(message);
    }
  }

  /**
   * Send an action (block, dodge, pass, etc.)
   */
  sendAction(action: string, params: string[]): void {
    if (this.websocket?.isReady()) {
      this.websocket.sendAction(action, params);
    }
  }

  /**
   * Request a reroll (via clientAction)
   */
  requestReroll(): void {
    if (this.websocket?.isReady()) {
      this.websocket.sendAction('reroll', []);
    }
  }

  /**
   * Confirm a decision (via clientAction)
   */
  confirmDecision(decision: boolean, param?: string): void {
    if (this.websocket?.isReady()) {
      this.websocket.sendAction('confirm', [decision ? 'true' : 'false', param || '']);
    }
  }

  /**
   * Decline a decision (via clientAction)
   */
  declineDecision(param?: string): void {
    if (this.websocket?.isReady()) {
      this.websocket.sendAction('decline', [param || '']);
    }
  }

  // ---------------------------------------------------------------------------
  // Callbacks
  // ---------------------------------------------------------------------------

  /**
   * Set service callbacks
   */
  setCallbacks(callbacks: typeof this.callbacks): void {
    this.callbacks = { ...this.callbacks, ...callbacks };
  }

  /**
   * Set auth change callback
   */
  setAuthCallback(callback: (isAuthenticated: boolean) => void): void {
    this.callbacks.onAuthChange = callback;
  }

  /**
   * Get current state
   */
  getState(): FumbblServiceState {
    return this.state;
  }

  /**
   * Get current game state
   */
  getGameState(): GameState {
    return this.state.gameState;
  }

  /**
   * Set game state updater (called when game state changes from WebSocket)
   */
  setGameStateUpdater(updater: (state: GameState) => void): void {
    this.callbacks.onStateUpdate = updater;
  }

  // ---------------------------------------------------------------------------
  // Private Methods
  // ---------------------------------------------------------------------------

  private createWebSocketCallbacks(): FumbblWebSocketCallbacks {
    return {
      onOpen: () => {
        this.setState({ isConnected: true, error: null });
        this.callbacks.onConnectionChange?.(true);
      },
      onClose: (_code, _reason) => {
        this.setState({ isConnected: false });
        this.callbacks.onConnectionChange?.(false);
      },
      onError: (error) => {
        const errorMsg = error.message;
        this.setState({ error: errorMsg });
        this.callbacks.onError?.(errorMsg);
      },
      onGameStateUpdate: (gameState: any) => {
        // The new architecture uses FumbblGameModel.toGameState() which returns a full GameState
        // We merge it with the current state and notify the GameContext
        const newGameState = { ...this.state.gameState, ...gameState } as GameState;
        this.setState({
          gameState: newGameState,
        });
        this.callbacks.onStateUpdate?.(newGameState);
      },
      onReports: (reports: any[]) => {
        // Forward reports from WebSocket to GameContext for dice log parsing
        console.log('[FumbblService] Reports received, forwarding to callbacks:', reports.length);
        this.callbacks.onReports?.(reports);
      },
      onChatMessage: (message, player) => {
        const chatMessage: ChatMessage = {
          id: Date.now(),
          sender: player?.name || 'Server',
          senderColor: 'text-gray-400',
          text: message,
          timestamp: Date.now(),
          type: 'general',
        };
        // This would be handled by the GameContext reducer
        console.log('Chat message received:', chatMessage);
      },
      onMessage: () => {
        // Generic message handler - no action needed
      },
    };
  }

  private setState(partial: Partial<FumbblServiceState>): void {
    this.state = { ...this.state, ...partial };
  }

  private createInitialState(): GameState {
    return {
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
    };
  }
}

// -----------------------------------------------------------------------------
// Factory Function
// -----------------------------------------------------------------------------

/**
 * Create a new FUMBBL service instance
 */
export function createFumbblService(config?: FumbblServiceConfig): FumbblService {
  return new FumbblService(config);
}