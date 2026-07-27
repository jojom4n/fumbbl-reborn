// =============================================================================
// FUMBBL WebSocket Service
// Handles WebSocket connection to the FUMBBL server for real-time game updates
// Based on ffbclient core/network.ts (OFFICIAL CLIENT - uses LZString compression)
// =============================================================================

import { decompressFromUTF16, compressToUTF16 } from 'lz-string';

import {
  FumbblWebSocketConfig,
  DEFAULT_WEBSOCKET_CONFIG,
  FumbblWebSocketCallbacks,
  ProtocolMessage,
  WebSocketState,
} from '../types/fumbblProtocol';

import { FumbblGameModel } from './fumbblGameModel';
import { FumbblCommandHandler, FumbblCommandHandlerCallbacks } from './fumbblCommandHandler';

// -----------------------------------------------------------------------------
// FUMBBL WebSocket Service
// -----------------------------------------------------------------------------

export class FumbblWebSocket {
  private config: FumbblWebSocketConfig;
  private callbacks: FumbblWebSocketCallbacks;
  private ws: WebSocket | null = null;
  private reconnectAttempts = 0;
  private reconnectTimer: number | null = null;
  private pingTimer: number | null = null;
  private isConnected = false;
  private isConnecting = false;
  private auth_token: string | null = null;
  private username: string | null = null;
  private game_id: number | null = null;
  private team_id: number | null = null;
  private spectate_mode = false;
  private version_sent = false;  // Track if we've already sent clientRequestVersion

  // NEW: Game model and command handler (ffbclient architecture)
  private gameModel: FumbblGameModel;
  private commandHandler: FumbblCommandHandler;

  constructor(config?: Partial<FumbblWebSocketConfig>, callbacks?: FumbblWebSocketCallbacks) {
    this.config = { ...DEFAULT_WEBSOCKET_CONFIG, ...config };
    this.callbacks = callbacks || {};

    // Initialize game model
    this.gameModel = new FumbblGameModel();

    // Initialize command handler with callbacks
    this.commandHandler = new FumbblCommandHandler(this.gameModel, this.createCommandHandlerCallbacks());
  }

  /**
   * Create the command handler callbacks that bridge WebSocket ↔ CommandHandler
   */
  private createCommandHandlerCallbacks(): FumbblCommandHandlerCallbacks {
    return {
      onGameState: (gameState) => {
        this.callbacks.onGameStateUpdate?.(gameState);
      },
      onModelSync: (changes) => {
        console.log('[FumbblWebSocket] onModelSync: propagating game state after', changes.length, 'changes');
        // ALWAYS propagate the game state after model sync - no conditional
        // This ensures the Dashboard UI receives updates even if score/team data hasn't changed
        const gameState = this.gameModel.toGameState();
        console.log('[FumbblWebSocket] onModelSync: gameState to UI:', {
          team1Name: gameState.team1?.name,
          team2Name: gameState.team2?.name,
          team1Players: gameState.team1Players?.length,
          team2Players: gameState.team2Players?.length,
          turn: gameState.turn,
          phase: gameState.phase,
          score: gameState.score,
        });
        this.callbacks.onGameStateUpdate?.(gameState as any);
      },
      onGameTime: (gameTime, turnTime) => {
        console.log('[FumbblWebSocket] Game time updated:', { gameTime, turnTime });
        // Propagate timer to GameContext - convert milliseconds to seconds for display
        const turnSeconds = Math.floor(turnTime / 1000);
        console.log('[FumbblWebSocket] Propagating timer update:', turnSeconds, 'seconds (from', turnTime, 'ms)');
        this.callbacks.onGameStateUpdate?.({ timer: turnSeconds } as any);
      },
      onTalk: (message) => {
        if (message.message) {
          this.callbacks.onChatMessage?.(message.message, message.player);
        }
      },
      onSound: (sound) => {
        this.callbacks.onSound?.(sound);
      },
      onJoin: (joinData) => {
        this.callbacks.onJoin?.(
          joinData.coach,
          joinData.clientMode,
          joinData.playerNames || [],
          joinData.spectators || 0
        );
      },
      onVersion: (versionData) => {
        console.log('[FumbblWebSocket] Server version:', versionData.serverVersion);
      },
      onUserSettings: (_settings) => {
        console.log('[FumbblWebSocket] User settings loaded');
      },
      onUnknownCommand: (commandId, _data) => {
        console.log('[FumbblWebSocket] Unknown command handled:', commandId);
      },
      // NEW callbacks from updated command handler
      onLeave: (_data) => {
        console.log('[FumbblWebSocket] Player left');
      },
      onAdminMessage: (messages) => {
        console.log('[FumbblWebSocket] Admin message:', messages);
      },
      onAddPlayer: (data) => {
        console.log('[FumbblWebSocket] Player added:', data);
      },
      onRemovePlayer: (data) => {
        console.log('[FumbblWebSocket] Player removed:', data);
      },
      onZapPlayer: (data) => {
        console.log('[FumbblWebSocket] Player zapped:', data);
      },
      onUnzapPlayer: (data) => {
        console.log('[FumbblWebSocket] Player unzapped:', data);
      },
      onUpdateLocalPlayerMarkers: (markers) => {
        console.log('[FumbblWebSocket] Local player markers updated:', markers.length);
      },
      onAddSketches: (data) => {
        console.log('[FumbblWebSocket] Sketches added:', data);
      },
      onRemoveSketches: (data) => {
        console.log('[FumbblWebSocket] Sketches removed:', data);
      },
      onClearSketches: (_data) => {
        console.log('[FumbblWebSocket] Sketches cleared');
      },
      onSketchAddCoordinate: (data) => {
        console.log('[FumbblWebSocket] Sketch coordinate added:', data);
      },
      onSketchSetColor: (data) => {
        console.log('[FumbblWebSocket] Sketch color changed:', data);
      },
      onSketchSetLabel: (data) => {
        console.log('[FumbblWebSocket] Sketch label changed:', data);
      },
      onSetPreventSketching: (data) => {
        console.log('[FumbblWebSocket] Prevent sketching toggled:', data);
      },
      onSocketClosed: (data) => {
        console.log('[FumbblWebSocket] Socket closed:', data);
      },
    };
  }

  // ---------------------------------------------------------------------------
  // Connection Management
  // ---------------------------------------------------------------------------

  /**
   * Connect to the FUMBBL WebSocket server
   * URL format: ws://fumbbl.com:22223/command
   */
  connect(): void {
    if (this.isConnected || this.isConnecting) {
      console.warn('FUMBBL WebSocket: Already connected or connecting');
      return;
    }

    this.isConnecting = true;
    const url = this.config.url;

    try {
      this.ws = new WebSocket(url);
    } catch (error) {
      this.isConnecting = false;
      const err = error instanceof Error ? error : new Error(String(error));
      this.callbacks.onError?.(err);
      this.scheduleReconnect();
      return;
    }

    this.ws.onopen = this.handleOpen.bind(this);
    this.ws.onmessage = this.handleMessage.bind(this);
    this.ws.onerror = this.handleError.bind(this);
    this.ws.onclose = this.handleClose.bind(this);
  }

  /**
   * Disconnect from the WebSocket server
   */
  disconnect(): void {
    this.stopPingTimer();

    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }

    this.isConnected = false;
    this.isConnecting = false;
    this.reconnectAttempts = 0;
  }

  /**
   * Get the current connection state
   */
  getState(): WebSocketState {
    if (!this.ws) {
      return WebSocketState.CLOSED;
    }
    return this.ws.readyState;
  }

  /**
   * Check if the connection is open and ready
   */
  isReady(): boolean {
    return this.isConnected && this.ws?.readyState === WebSocketState.OPEN;
  }

  // ---------------------------------------------------------------------------
  // Event Handlers
  // ---------------------------------------------------------------------------

  private handleOpen(): void {
    this.isConnecting = false;
    this.isConnected = true;
    this.reconnectAttempts = 0;

    console.log('FUMBBL WebSocket: Connected');
    console.log('[FumbblWebSocket] Connection state:', {
      auth_token: this.auth_token ? 'present' : 'null',
      game_id: this.game_id,
      team_id: this.team_id,
      spectate_mode: this.spectate_mode,
    });

    // CRITICAL: Start the ping timer to keep the connection alive.
    // The FFB server closes the connection if no pings are received.
    // Based on official ffbclient: ClientPingTask is scheduled immediately after connection.
    this.startPingTimer();

    this.callbacks.onOpen?.();

    // Auto-join if we have credentials
    if (this.auth_token && this.game_id !== null) {
      if (this.spectate_mode || this.team_id !== null) {
        console.log('[FumbblWebSocket] Sending join command, mode:', this.spectate_mode ? 'spectate' : 'game');
        this.sendJoinInternal();
      } else {
        console.warn('[FumbblWebSocket] Cannot auto-join: missing team_id and not in spectator mode');
      }
    } else {
      console.warn('[FumbblWebSocket] Cannot auto-join: missing auth_token or game_id');
    }
  }

  private async handleMessage(event: MessageEvent): Promise<void> {
    const rawData = event.data;
    const maxSize = this.config.maxMessageSize;

    // Check message size
    if (typeof rawData === 'string' && rawData.length > maxSize) {
      const err = new Error(`Message too large: ${rawData.length} > ${maxSize}`);
      this.callbacks.onError?.(err);
      return;
    }

    let decompressed: string;
    try {
      if (rawData instanceof Blob) {
        const text = await rawData.text();
        decompressed = decompressFromUTF16(text);
        if (!decompressed || decompressed.length === 0) {
          decompressed = text;
        }
      } else if (typeof rawData === 'string') {
        decompressed = decompressFromUTF16(rawData);
        if (!decompressed || decompressed.length === 0) {
          decompressed = rawData;
        }
      } else if (rawData instanceof ArrayBuffer) {
        const uint8Array = new Uint8Array(rawData);
        const decoder = new TextDecoder('utf-8');
        const text = decoder.decode(uint8Array);
        decompressed = decompressFromUTF16(text);
        if (!decompressed || decompressed.length === 0) {
          decompressed = text;
        }
      } else {
        console.error('[FumbblWebSocket] Unsupported message type:', typeof rawData);
        return;
      }
    } catch (error) {
      console.error('[FumbblWebSocket] Failed to decompress WebSocket message:', error, 'raw:', rawData);
      const err = new Error(`Failed to decompress WebSocket message: ${error}`);
      this.callbacks.onError?.(err);
      return;
    }

    // Parse the decompressed JSON
    let msg: ProtocolMessage;
    try {
      const parsed = JSON.parse(decompressed);
      msg = parsed as ProtocolMessage;
    } catch (error) {
      console.error('[FumbblWebSocket] Failed to parse WebSocket message:', error, 'raw:', decompressed);
      const err = new Error(`Failed to parse WebSocket message: ${error}`);
      this.callbacks.onError?.(err);
      return;
    }

    // Log message for debugging
    const commandId = (msg as any).netCommandId;
    console.log('[FumbblWebSocket] Received message:', commandId);

    // Route through CommandHandler (ffbclient architecture)
    this.commandHandler.handleCommand(msg);

    // Also call the generic message handler for backward compatibility
    this.callbacks.onMessage?.(msg);
  }

  private handleError(event: Event): void {
    const err = new Error('WebSocket error occurred');
    this.callbacks.onError?.(err);
    console.error('FUMBBL WebSocket error:', event);
  }

  private handleClose(event: CloseEvent): void {
    this.isConnected = false;
    this.isConnecting = false;
    this.stopPingTimer();

    console.log(`FUMBBL WebSocket: Closed (code: ${event.code}, reason: ${event.reason})`);
    this.callbacks.onClose?.(event.code, event.reason || 'Connection closed');

    // Attempt to reconnect if not intentionally closed
    if (event.code !== 1000) {
      this.scheduleReconnect();
    }
  }

  // ---------------------------------------------------------------------------
  // Commands (Outgoing)
  // ---------------------------------------------------------------------------

  /**
   * Send a version request to the server
   */
  sendVersionRequest(): void {
    this.sendRaw({ netCommandId: 'clientRequestVersion' });
  }

  /**
   * Join a game session
   */
  sendJoin(clientMode: 'player' | 'spectator' | 'replay' = 'player', coachName?: string): void {
    if (!this.auth_token || this.game_id === null) {
      console.warn('FUMBBL WebSocket: Cannot join - missing auth_token or game_id');
      return;
    }

    if (!this.spectate_mode && this.team_id === null) {
      console.warn('FUMBBL WebSocket: Cannot join - missing team_id (not in spectator mode)');
      return;
    }

    const command: any = {
      netCommandId: 'clientJoin',
      clientMode,
      coach: coachName || this.username || '',
      password: this.auth_token || '',
      gameId: this.game_id,
      gameName: '',
      teamId: this.spectate_mode ? '' : (this.team_id ? String(this.team_id) : ''),
      teamName: '',
    };

    console.log('[FumbblWebSocket] Sending clientJoin command:', JSON.stringify(command, null, 2));
    this.sendRaw(command);
  }

  /**
   * Internal join method called after connection is established
   */
  private sendJoinInternal(): void {
    // CRITICAL: Send clientRequestVersion BEFORE clientJoin (matches official ffbclient pattern)
    if (!this.version_sent) {
      this.version_sent = true;
      console.log('[FumbblWebSocket] Sending clientRequestVersion before clientJoin');
      this.sendVersionRequest();
    }
    this.sendJoin(this.spectate_mode ? 'spectator' : 'player');
  }

  /**
   * Send a chat message
   */
  sendChatMessage(message: string): void {
    this.sendRaw({ netCommandId: 'clientTalk', talk: message });
  }

  /**
   * Send an action (block, dodge, pass, etc.) to the server.
   * Maps to clientAction command in the FFB protocol.
   * @param action The action type (e.g., 'block', 'dodge', 'pass', 'reroll', 'confirm', 'decline')
   * @param params Additional parameters for the action
   */
  sendAction(action: string, params: string[]): void {
    const command: any = {
      netCommandId: 'clientAction',
      action,
    };
    if (params && params.length > 0) {
      command.params = params;
    }
    console.log('[FumbblWebSocket] Sending clientAction:', action, params);
    this.sendRaw(command);
  }

  /**
   * Send a ping to the server with current timestamp.
   * Matches ClientCommandPing(timestamp) from the official ffbclient.
   * The server responds with serverPong and updates the last ping time.
   */
  private sendPing(): void {
    if (!this.isReady()) {
      // Don't send ping if not connected
      return;
    }
    const timestamp = Date.now();
    this.sendRaw({ netCommandId: 'clientPing', timestamp });
    console.log('[FumbblWebSocket] Sent clientPing:', timestamp);
  }

  /**
   * Send a raw JSON message (LZString compressed with UTF16 encoding).
   * The fumbbl.com production server uses compression.
   * Evidence: Original log shows server responds ONLY when compressed messages are sent.
   */
  sendRaw(message: ProtocolMessage): void {
    if (!this.ws || this.ws.readyState !== WebSocketState.OPEN) {
      console.warn('FUMBBL WebSocket: Cannot send - connection not open');
      return;
    }

    try {
      const json = JSON.stringify(message);
      const compressed = compressToUTF16(json);
      console.log('[FumbblWebSocket] Sending compressed message (raw:', json.length, 'compressed:', compressed.length, '):', json.substring(0, 200));
      this.ws.send(compressed);
    } catch (error) {
      const err = error instanceof Error ? error : new Error(String(error));
      this.callbacks.onError?.(err);
    }
  }

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------

  /**
   * Set the session token (used as password in clientJoin)
   */
  setAuthToken(token: string): void {
    this.auth_token = token;
  }

  /**
   * Set the username (used as coach in clientJoin)
   */
  setUsername(username: string): void {
    this.username = username;
  }

  /**
   * Set the game ID to join
   */
  setGameId(gameId: number): void {
    this.game_id = gameId;
  }

  /**
   * Set the team ID to join
   */
  setTeamId(teamId: number): void {
    this.team_id = teamId;
  }

  /**
   * Enable spectator mode (no teamId needed)
   */
  enableSpectatorMode(): void {
    this.spectate_mode = true;
  }

  /**
   * Update the callbacks (merges with existing callbacks)
   */
  setCallbacks(callbacks: FumbblWebSocketCallbacks): void {
    this.callbacks = { ...this.callbacks, ...callbacks };
  }

  /**
   * Get the game model (for accessing state)
   */
  getGameModel(): FumbblGameModel {
    return this.gameModel;
  }

  // ---------------------------------------------------------------------------
  // Reconnection
  // ---------------------------------------------------------------------------

  private scheduleReconnect(): void {
    if (this.config.maxReconnectAttempts !== -1 &&
      this.reconnectAttempts >= this.config.maxReconnectAttempts) {
      console.warn('FUMBBL WebSocket: Max reconnection attempts reached');
      return;
    }

    this.reconnectAttempts++;
    const delay = this.config.reconnectDelay * Math.pow(2, Math.min(this.reconnectAttempts - 1, 5));

    console.log(`FUMBBL WebSocket: Scheduling reconnect (attempt ${this.reconnectAttempts}) in ${delay}ms`);
    this.callbacks.onReconnect?.(this.reconnectAttempts);

    this.reconnectTimer = window.setTimeout(() => {
      this.connect();
    }, delay);
  }

  // ---------------------------------------------------------------------------
  // Ping/Keepalive
  // ---------------------------------------------------------------------------

  private stopPingTimer(): void {
    if (this.pingTimer) {
      clearInterval(this.pingTimer);
      this.pingTimer = null;
    }
  }

  /**
   * Start the ping timer to keep the connection alive.
   * The FFB server requires periodic clientPing messages.
   *
   * CRITICAL: The official ffbclient uses client.ping.interval=2000 (2 seconds) from client.ini!
   * The server SessionTimeoutTask closes connections where lastPing + 10000ms < now.
   * With a 2-second ping interval, we're well within the 10-second timeout.
   *
   * The official Java client uses Timer.schedule(task, 0, interval) which sends
   * the FIRST ping IMMEDIATELY (delay=0), then every 2 seconds. JavaScript setInterval
   * does NOT fire immediately, so we must call sendPing() once after setting up the interval.
   */
  private startPingTimer(): void {
    this.stopPingTimer();
    // OFFICIAL CLIENT VALUE: client.ping.interval=2000 from client.ini
    // Server timeout is 10000ms, so 2000ms ping interval gives us 5x safety margin
    const pingInterval = 2000; // 2 seconds - EXACT match with official ffbclient
    this.pingTimer = window.setInterval(() => {
      this.sendPing();
    }, pingInterval);
    // IMMEDIATE FIRST PING - matches official client Timer.schedule(task, 0, interval) behavior
    // The first ping must be sent immediately after connection to prevent server timeout
    this.sendPing();
    console.log(`[FumbblWebSocket] Ping timer started (interval: ${pingInterval}ms, matches official client.ini)`);
  }
}

// -----------------------------------------------------------------------------
// Factory Function
// -----------------------------------------------------------------------------

/**
 * Create a new FUMBBL WebSocket instance with default configuration
 */
export function createFumbblWebSocket(config?: Partial<FumbblWebSocketConfig>, callbacks?: FumbblWebSocketCallbacks): FumbblWebSocket {
  return new FumbblWebSocket(config, callbacks);
}