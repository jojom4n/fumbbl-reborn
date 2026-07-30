// =============================================================================
// FUMBBL Command Handler
// Routes server messages to appropriate handlers
// Based on ffbclient core/commandhandler.ts architecture (OFFICIAL CLIENT)
// =============================================================================

import { FumbblGameModel } from './fumbblGameModel';

// Callback types for handling different server commands
// Based on ffbclient ClientCommandHandler architecture
export interface FumbblCommandHandlerCallbacks {
  onGameState: (gameState: any) => void;
  onModelSync: (changes: any[]) => void;
  onReports: (reports: any[]) => void;
  onGameTime: (gameTime: number, turnTime: number) => void;
  onTalk: (message: any) => void;
  onSound: (sound: any) => void;
  onJoin: (joinData: any) => void;
  onLeave: (leaveData: any) => void;
  onVersion: (versionData: any) => void;
  onUserSettings: (settings: any) => void;
  onAdminMessage: (messages: string[]) => void;
  onAddPlayer: (data: any) => void;
  onRemovePlayer: (data: any) => void;
  onZapPlayer: (data: any) => void;
  onUnzapPlayer: (data: any) => void;
  onUpdateLocalPlayerMarkers: (markers: any[]) => void;
  onAddSketches: (data: any) => void;
  onRemoveSketches: (data: any) => void;
  onClearSketches: (data: any) => void;
  onSketchAddCoordinate: (data: any) => void;
  onSketchSetColor: (data: any) => void;
  onSketchSetLabel: (data: any) => void;
  onSetPreventSketching: (data: any) => void;
  onSocketClosed: (data: any) => void;
  onUnknownCommand: (commandId: string, data: any) => void;
}

export class FumbblCommandHandler {
  private gameModel: FumbblGameModel;
  private callbacks: FumbblCommandHandlerCallbacks;

  private commandHandlers: { [id: string]: (data: any) => void };

  public constructor(gameModel: FumbblGameModel, callbacks: FumbblCommandHandlerCallbacks) {
    this.gameModel = gameModel;
    this.callbacks = callbacks;

    // Register ALL command handlers (matching ffbclient ClientCommandHandlerFactory)
    // These match exactly the handlers registered in ClientCommandHandlerFactory.java
    this.commandHandlers = {
      // Core handlers (already implemented)
      'serverGameState': this.handleGameState.bind(this),
      'serverModelSync': this.handleModelSync.bind(this),
      'serverGameTime': this.handleGameTime.bind(this),
      'serverTalk': this.handleTalk.bind(this),
      'serverSound': this.handleSound.bind(this),
      'serverJoin': this.handleJoin.bind(this),
      'serverVersion': this.handleVersion.bind(this),
      'serverUserSettings': this.handleUserSettings.bind(this),
      
      // NEW handlers from official client (ClientCommandHandlerFactory.java)
      'serverLeave': this.handleLeave.bind(this),
      'serverAdminMessage': this.handleAdminMessage.bind(this),
      'serverAddPlayer': this.handleAddPlayer.bind(this),
      'serverRemovePlayer': this.handleRemovePlayer.bind(this),
      'serverZapPlayer': this.handleZapPlayer.bind(this),
      'serverUnzapPlayer': this.handleUnzapPlayer.bind(this),
      'serverUpdateLocalPlayerMarkers': this.handleUpdateLocalPlayerMarkers.bind(this),
      'serverAddSketches': this.handleAddSketches.bind(this),
      'serverRemoveSketches': this.handleRemoveSketches.bind(this),
      'serverClearSketches': this.handleClearSketches.bind(this),
      'serverSketchAddCoordinate': this.handleSketchAddCoordinate.bind(this),
      'serverSketchSetColor': this.handleSketchSetColor.bind(this),
      'serverSketchSetLabel': this.handleSketchSetLabel.bind(this),
      'serverSetPreventSketching': this.handleSetPreventSketching.bind(this),
      'serverSocketClosed': this.handleSocketClosed.bind(this),

      // Ping/Pong handlers (keepalive)
      'serverPong': this.handlePong.bind(this),
      'serverPing': this.handleServerPing.bind(this),
    };
  }

  /**
   * Handle an incoming server command
   * This is called by fumbblWebSocket.ts when a message is received
   */
  public handleCommand(data: any): void {
    const commandId = data.netCommandId;

    if (!commandId) {
      console.warn('[FumbblCommandHandler] Message received without netCommandId:', data);
      return;
    }

    const handler = this.commandHandlers[commandId];
    if (handler) {
      try {
        handler(data);
      } catch (error) {
        console.error(`[FumbblCommandHandler] Error handling command ${commandId}:`, error);
      }
    } else {
      console.log('[FumbblCommandHandler] Unknown command:', commandId, data);
      this.callbacks.onUnknownCommand(commandId, data);
    }
  }

  // ---------------------------------------------------------------------------
  // Command Handlers (matching ffbclient ClientCommandHandler classes)
  // ---------------------------------------------------------------------------

  /**
   * Handles serverGameState - Full game state update
   * Based on ffbclient ClientCommandHandlerGameState
   */
  private handleGameState(data: any): void {
    console.log('[FumbblCommandHandler] Processing game state command');

    // The serverGameState message has a "game" property (matching ffbclient CommandGameState)
    // Structure: { netCommandId: "serverGameState", game: { teamHome: {...}, teamAway: {...}, fieldModel: {...}, ... } }
    const gameData = data.game || data;

    console.log('[FumbblCommandHandler] serverGameState data:', {
      hasGame: !!data.game,
      hasTeamHome: !!gameData.teamHome,
      hasTeamAway: !!gameData.teamAway,
      hasFieldModel: !!gameData.fieldModel,
      hasActingPlayer: !!gameData.actingPlayer,
      teamHomeId: gameData.teamHome?.teamId,
      teamHomeName: gameData.teamHome?.teamName,
      teamHomeRace: gameData.teamHome?.race,
      teamHomeRosterLength: gameData.teamHome?.roster?.positionArray?.length,
      teamHomePlayerArrayLength: gameData.teamHome?.playerArray?.length,
      teamAwayId: gameData.teamAway?.teamId,
      teamAwayName: gameData.teamAway?.teamName,
      teamAwayRace: gameData.teamAway?.race,
      teamAwayRosterLength: gameData.teamAway?.roster?.positionArray?.length,
      teamAwayPlayerArrayLength: gameData.teamAway?.playerArray?.length,
      half: gameData.half,
      turnMode: gameData.turnMode?.name,
      homePlaying: gameData.homePlaying,
    });

    // Initialize or re-initialize the game model with the game object
    // Use isUpdate=false for full state refresh (serverGameState always sends complete state)
    this.gameModel.initialize(gameData);

    // Convert to UI format and notify
    const gameState = this.gameModel.toGameState();

    console.log('[FumbblCommandHandler] Mapped gameState to UI:', {
      hasTeam1: !!gameState.team1,
      hasTeam2: !!gameState.team2,
      team1Name: gameState.team1?.name,
      team1Race: gameState.team1?.race,
      team1PlayersCount: gameState.team1Players?.length,
      team2Name: gameState.team2?.name,
      team2Race: gameState.team2?.race,
      team2PlayersCount: gameState.team2Players?.length,
      turn: gameState.turn,
      phase: gameState.phase,
      score: gameState.score,
      reRolls: gameState.reRolls,
      timer: gameState.timer,
      isLive: gameState.isLive,
    });

    // CRITICAL: Always call onGameState to ensure the UI receives the update
    // This matches ffbclient: after serverGameState, the UI must be refreshed
    this.callbacks.onGameState(gameState);
  }

  /**
   * Handles serverModelSync - Incremental model changes
   * Based on ffbclient ClientCommandHandlerModelSync
   */
  private handleModelSync(data: any): void {
    console.log('[FumbblCommandHandler] Processing model sync command');

    // Extract model change array and reports (matching ffbclient CommandModelSync)
    // Structure: { netCommandId: "serverModelSync", modelChangeList: { modelChangeArray: [...] }, reportList: { reports: [...] }, sound: "..." }
    const modelChangeList = data.modelChangeList || {};
    const changes = modelChangeList.modelChangeArray || [];
    const reportList = data.reportList || {};
    const reports = reportList.reports || [];
    const sound = data.sound || '';
    const gameTime = data.gameTime;
    const turnTime = data.turnTime;

    console.log('[FumbblCommandHandler] Model sync:', {
      changesCount: changes.length,
      reportsCount: reports.length,
      sound: sound,
      gameTime,
      turnTime,
    });

    // Log the first few model changes for debugging
    if (changes.length > 0 && changes.length <= 10) {
      console.log('[FumbblCommandHandler] Model changes:', changes.map((c: any) => ({
        modelChangeId: c.modelChangeId || c.type,
        modelChangeKey: c.modelChangeKey,
      })));
    } else if (changes.length > 10) {
      console.log('[FumbblCommandHandler] First 10 of', changes.length, 'changes:',
        changes.slice(0, 10).map((c: any) => c.modelChangeId || c.type)
      );
    }

    // Apply changes to the game model
    this.gameModel.applyModelChanges(changes);

    // Convert to UI format and notify
    const gameState = this.gameModel.toGameState();

    console.log('[FumbblCommandHandler] gameState after model sync:', {
      team1Name: gameState.team1?.name,
      team2Name: gameState.team2?.name,
      team1PlayersCount: gameState.team1Players?.length,
      team2PlayersCount: gameState.team2Players?.length,
      turn: gameState.turn,
      phase: gameState.phase,
      score: gameState.score,
      timer: gameState.timer,
    });

    // CRITICAL: Always call all callbacks to ensure complete UI update
    // This matches ffbclient: after serverModelSync, update UI with all changes
    this.callbacks.onModelSync(changes);
    // Send reports to the callback for dice log parsing
    if (reports.length > 0) {
      console.log('[FumbblCommandHandler] Reports received:', reports.length, 'reports');
      this.callbacks.onReports(reports);
    }
    this.callbacks.onSound(sound);
    this.callbacks.onGameState(gameState);
  }

  /**
   * Handles serverGameTime - Game time update
   * Based on ffbclient ClientCommandHandlerGameTime
   */
  private handleGameTime(data: any): void {
    console.log('[FumbblCommandHandler] Processing game time command:', {
      gameTime: data.gameTime,
      turnTime: data.turnTime,
      turnTimeSeconds: data.turnTime ? Math.floor(data.turnTime / 1000) : 0,
    });
    this.callbacks.onGameTime(data.gameTime, data.turnTime);
  }

  /**
   * Handles serverTalk - Chat message
   * Based on ffbclient ClientCommandHandlerTalk
   */
  private handleTalk(data: any): void {
    console.log('[FumbblCommandHandler] Processing talk command');
    this.callbacks.onTalk(data);
  }

  /**
   * Handles serverSound - Sound effect
   * Based on ffbclient ClientCommandHandlerSound
   */
  private handleSound(data: any): void {
    console.log('[FumbblCommandHandler] Processing sound command');
    this.callbacks.onSound(data);
  }

  /**
   * Handles serverJoin - Join confirmation
   * Based on ffbclient ClientCommandHandlerJoin
   * CRITICAL: Does NOT send clientReady (that command does not exist in official protocol)
   */
  private handleJoin(data: any): void {
    console.log('[FumbblCommandHandler] Processing join command');
    console.log('[FumbblCommandHandler] serverJoin received - coach:', data.coach, 'mode:', data.clientMode, 'players:', data.playerNames, 'spectators:', data.spectators);
    
    // Update game model with join data (spectators, players)
    if (data.spectators) {
      this.gameModel.setSpectatorCount(data.spectatorCount || data.spectators.length);
      this.gameModel.setSpectators(data.spectators);
    }

    // Notify about join
    this.callbacks.onJoin(data);

    // CRITICAL FIX: Do NOT send clientReady!
    // The official ffbclient does NOT send clientReady after serverJoin.
    // This was causing the server to close the connection.
    // Reference: ClientCommandHandlerJoin.java does not send any response.
    console.log('[FumbblCommandHandler] Join processed. NOT sending clientReady (not part of official protocol)');
  }

  /**
   * Handles serverVersion - Version negotiation
   * Based on ffbclient ClientCommandHandlerVersion
   */
  private handleVersion(data: any): void {
    console.log('[FumbblCommandHandler] Processing version command');
    console.log('[FumbblCommandHandler] Server version:', data.serverVersion, 'Client version:', data.clientVersion);
    this.callbacks.onVersion(data);
  }

  /**
   * Handles serverUserSettings - User settings
   * Based on ffbclient ClientCommandHandlerUserSettings
   */
  private handleUserSettings(data: any): void {
    console.log('[FumbblCommandHandler] Processing user settings command');
    this.callbacks.onUserSettings(data);
  }

  /**
   * Handles serverLeave - Player left
   * Based on ffbclient ClientCommandHandlerLeave
   */
  private handleLeave(data: any): void {
    console.log('[FumbblCommandHandler] Processing leave command');
    console.log('[FumbblCommandHandler] serverLeave received - coach:', data.coach, 'mode:', data.clientMode, 'spectators:', data.spectators);
    this.callbacks.onLeave(data);
  }

  /**
   * Handles serverAdminMessage - Admin message
   * Based on ffbclient ClientCommandHandlerAdminMessage
   */
  private handleAdminMessage(data: any): void {
    console.log('[FumbblCommandHandler] Processing admin message command');
    const messages = data.messages || (typeof data.message === 'string' ? [data.message] : []);
    console.log('[FumbblCommandHandler] Admin messages:', messages);
    this.callbacks.onAdminMessage(messages);
  }

  /**
   * Handles serverAddPlayer - Player added
   * Based on ffbclient ClientCommandHandlerAddPlayer
   */
  private handleAddPlayer(data: any): void {
    console.log('[FumbblCommandHandler] Processing add player command');
    console.log('[FumbblCommandHandler] Player added:', data.teamId, data.player, data.playerState);
    this.callbacks.onAddPlayer(data);
  }

  /**
   * Handles serverRemovePlayer - Player removed
   * Based on ffbclient ClientCommandHandlerRemovePlayer
   */
  private handleRemovePlayer(data: any): void {
    console.log('[FumbblCommandHandler] Processing remove player command');
    console.log('[FumbblCommandHandler] Player removed:', data.playerId);
    this.callbacks.onRemovePlayer(data);
  }

  /**
   * Handles serverZapPlayer - Player zapped
   * Based on ffbclient ClientCommandHandlerZapPlayer
   */
  private handleZapPlayer(data: any): void {
    console.log('[FumbblCommandHandler] Processing zap player command');
    console.log('[FumbblCommandHandler] Zap player:', data.playerId, data.teamId);
    this.callbacks.onZapPlayer(data);
  }

  /**
   * Handles serverUnzapPlayer - Player unzapped
   * Based on ffbclient ClientCommandHandlerUnzapPlayer
   */
  private handleUnzapPlayer(data: any): void {
    console.log('[FumbblCommandHandler] Processing unzap player command');
    console.log('[FumbblCommandHandler] Unzap player:', data.playerId, data.teamId);
    this.callbacks.onUnzapPlayer(data);
  }

  /**
   * Handles serverUpdateLocalPlayerMarkers - Player markers update
   * Based on ffbclient ClientCommandHandlerUpdateLocalPlayerMarkers
   */
  private handleUpdateLocalPlayerMarkers(data: any): void {
    console.log('[FumbblCommandHandler] Processing update local player markers command');
    // playerMarkerArray contains UI-only markers for the local player
    // Safe to ignore for spectator mode, but notify callback
    console.log('[FumbblCommandHandler] Local player markers:', data.playerMarkerArray?.length || 0);
    this.callbacks.onUpdateLocalPlayerMarkers(data.playerMarkerArray || []);
  }

  /**
   * Handles serverAddSketches - Sketches added to field
   * Based on ffbclient ClientCommandHandlerAddSketches
   */
  private handleAddSketches(data: any): void {
    console.log('[FumbblCommandHandler] Processing add sketches command');
    console.log('[FumbblCommandHandler] Sketches added:', data.sketches?.length || 0);
    this.callbacks.onAddSketches(data);
  }

  /**
   * Handles serverRemoveSketches - Sketches removed from field
   * Based on ffbclient ClientCommandHandlerRemoveSketches
   */
  private handleRemoveSketches(data: any): void {
    console.log('[FumbblCommandHandler] Processing remove sketches command');
    console.log('[FumbblCommandHandler] Sketches removed:', data.sketchIds?.length || 0);
    this.callbacks.onRemoveSketches(data);
  }

  /**
   * Handles serverClearSketches - All sketches cleared
   * Based on ffbclient ClientCommandHandlerClearSketches
   */
  private handleClearSketches(data: any): void {
    console.log('[FumbblCommandHandler] Processing clear sketches command');
    this.callbacks.onClearSketches(data);
  }

  /**
   * Handles serverSketchAddCoordinate - Coordinate added to sketch
   * Based on ffbclient ClientCommandHandlerSketchAddCoordinate
   */
  private handleSketchAddCoordinate(data: any): void {
    console.log('[FumbblCommandHandler] Processing sketch add coordinate command');
    console.log('[FumbblCommandHandler] Sketch coordinate:', data.sketchId, data.coordinate);
    this.callbacks.onSketchAddCoordinate(data);
  }

  /**
   * Handles serverSketchSetColor - Sketch color changed
   * Based on ffbclient ClientCommandHandlerSketchSetColor
   */
  private handleSketchSetColor(data: any): void {
    console.log('[FumbblCommandHandler] Processing sketch set color command');
    console.log('[FumbblCommandHandler] Sketch color:', data.sketchId, data.color);
    this.callbacks.onSketchSetColor(data);
  }

  /**
   * Handles serverSketchSetLabel - Sketch label changed
   * Based on ffbclient ClientCommandHandlerSketchSetLabel
   */
  private handleSketchSetLabel(data: any): void {
    console.log('[FumbblCommandHandler] Processing sketch set label command');
    console.log('[FumbblCommandHandler] Sketch label:', data.sketchId, data.label);
    this.callbacks.onSketchSetLabel(data);
  }

  /**
   * Handles serverSetPreventSketching - Prevent sketching toggled
   * Based on ffbclient ClientCommandHandlerSetPreventSketching
   */
  private handleSetPreventSketching(data: any): void {
    console.log('[FumbblCommandHandler] Processing set prevent sketching command');
    console.log('[FumbblCommandHandler] Prevent sketching:', data.preventSketching);
    this.callbacks.onSetPreventSketching(data);
  }

  /**
   * Handles serverSocketClosed - Socket closed notification
   * Based on ffbclient ClientCommandHandlerSocketClosed
   */
  private handleSocketClosed(data: any): void {
    console.log('[FumbblCommandHandler] Processing socket closed command');
    this.callbacks.onSocketClosed(data);
  }

  /**
   * Handles serverPong - Response to our clientPing
   * Based on ffbclient ClientCommandHandlerPong
   * The server echoes back the timestamp we sent in clientPing.
   */
  private handlePong(data: any): void {
    console.log('[FumbblCommandHandler] serverPong received, timestamp:', data.timestamp);
  }

  /**
   * Handles serverPing - Server initiated ping (rare, but possible)
   * We respond by doing nothing since the server primarily uses pong responses.
   */
  private handleServerPing(data: any): void {
    console.log('[FumbblCommandHandler] serverPing received, timestamp:', data.timestamp);
  }
}
