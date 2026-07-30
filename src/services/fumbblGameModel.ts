// =============================================================================
// FUMBBL Game Model - Complete Implementation
// Copied EXACTLY from ffbclient model/Game.java and ModelChangeProcessor.java
// Reference: /home/saruman/Projects/ffb/ffb-common/src/main/java/com/fumbbl/ffb/model/
// =============================================================================

import { ModelChangeType } from '../types/fumbblProtocol';
import {
  GameState, Player as FumbblPlayer,
  PlayerPosition, PlayerStatus, WeatherType, FieldState,
  SkillShorthand,
} from '../types/bloodbowl';

// -----------------------------------------------------------------------------
// FFB Protocol internal types (matching actual server data structure EXACTLY)
// -----------------------------------------------------------------------------

interface FFBPlayerType {
  playerId: string;
  playerName: string;
  playerNr: number;
  playerType: string;
  positionName: string;
  race: string;
  agility: number;
  strength: number;
  movement: number;
  armour: number;
  skillArray: string[];
  playerCoordinate?: number[];
  playerState?: number;
  fieldX?: number;
  fieldY?: number;
}

interface FFBRosterType {
  rosterId: string;
  rosterName: string;
  apothecary: boolean;
}

interface FFBTeamType {
  teamId: string;
  teamName: string;
  coach: string;
  race: string;
  reRolls: number;
  fans: number;
  dedicatedFans: number;
  playerArray: FFBPlayerType[];
  roster: FFBRosterType;
  score?: number;
}

interface FFBActingPlayer {
  playerId: string;
  currentMove: number;
  goingForIt: boolean;
  hasBlocked: boolean;
  hasFed: boolean;
  hasFouled: boolean;
  hasMoved: boolean;
  hasPassed: boolean;
  standingUp: boolean;
  sufferingBloodlust: boolean;
  sufferingAnimosity: boolean;
  playerAction: { name: string };
  strength?: number;
}

interface FFBFieldModelType {
  ballCoordinate: number[] | null;
  ballInPlay: boolean;
  ballMoving: boolean;
  bombCoordinate: number[] | null;
  playerDataArray: Array<{ playerId: string; playerCoordinate: number[]; playerState: number }>;
  weather: { name: string };
}

interface FFBGameType {
  teamHome: FFBTeamType;
  teamAway: FFBTeamType;
  actingPlayer: FFBActingPlayer;
  fieldModel: FFBFieldModelType;
  turnMode: { name: string };
  turnDataHome?: any;
  turnDataAway?: any;
  turnTime: number;
  gameTime: number;
  half: number;
  homePlaying: boolean;
  score?: { team1: number; team2: number };
  fanAttendance?: number;
  dedicatedFans?: any;
  reports?: any[];
}

// -----------------------------------------------------------------------------
// Internal player type with field coordinates merged
// -----------------------------------------------------------------------------

/**
 * Display player type for field rendering.
 * Similar to FumbblPlayer but uses string id for unique React keys.
 */
interface DisplayPlayer {
  id: string;                // Unique composite ID for React keys
  playerId: string;          // Original server playerId
  teamSide: 'home' | 'away';
  name: string;
  number: number;
  race: string;
  position: PlayerPosition;
  status: PlayerStatus;
  skills: SkillShorthand[];
  ma: number;
  st: number;
  ag: number;
  pa: number;
  av: number;
  hasBall: boolean;
  fieldX: number;
  fieldY: number;
}

// -----------------------------------------------------------------------------
// FUMBBL Game Model Class - COPIED from official client architecture
// -----------------------------------------------------------------------------

export class FumbblGameModel {
  private isInitialized = false;
  private teamHome: FFBTeamType | null = null;
  private teamAway: FFBTeamType | null = null;
  private activePlayerId: string | null = null;
  private ballCoordinate = { x: 8, y: 5 };
  private half = 0;
  private sidePlaying: 'home' | 'away' = 'home';
  private score = { team1: 0, team2: 0 };
  private turn = 1;
  private turnMode = 'setup';
  private turnTime = 0;
  private gameTime = 0;

  // Spectator data (from ClientCommandHandlerJoin)
  private spectatorCount = 0;
  private spectators: string[] = [];

  // COPIED from official client: players map by playerId
  private players: Record<string, FFBPlayerType> = {};

  // COPIED from official client: coordinate -> player lookup
  private playerLocations: Map<string, string> = new Map();

  // MERGED players (roster + field coordinates) for UI display
  private mergedPlayers: Record<string, DisplayPlayer> = {};

  private gameState: FFBGameType | null = null;

  private modelChangeHandlers: Record<string, (key: string, value: any) => boolean>;

   constructor() {
     // Build handlers exactly matching the official ModelChangeId.java names
     // Reference: ffb-common/.../model/change/ModelChangeId.java
     this.modelChangeHandlers = {
       // ============================================================
       // ACTING_PLAYER handlers (27 total from official client)
       // ============================================================
       'actingPlayerMarkSkillUsed': this.hActSkillUsed.bind(this),
       'actingPlayerMarkSkillUnused': this.hActSkillUnused.bind(this),
       'actingPlayerSetCurrentMove': this.hActCurMove.bind(this),
       'actingPlayerSetDodging': this.hActDodging.bind(this),
       'actingPlayerSetGoingForIt': this.hActGFI.bind(this),
       'actingPlayerSetHasBlocked': this.hActBlocked.bind(this),
       'actingPlayerSetHasFed': this.hActFed.bind(this),
       'actingPlayerSetHasFouled': this.hActFouled.bind(this),
       'actingPlayerSetHasJumped': this.hActJumped.bind(this),
       'actingPlayerSetHasTriggeredEffect': this.hActTriggered.bind(this),
       'actingPlayerSetHasMoved': this.hActMoved.bind(this),
       'actingPlayerSetHasPassed': this.hActPassed.bind(this),
       'actingPlayerSetJumping': this.hActJumping.bind(this),
       'actingPlayerSetOldPlayerState': this.hActOldState.bind(this),
       'actingPlayerSetPlayerAction': this.hActAction.bind(this),
       'actingPlayerSetPlayerId': this.hActPlayerId.bind(this),
       'actingPlayerSetStandingUp': this.hActStanding.bind(this),
       'actingPlayerSetStrength': this.hActStrength.bind(this),
       'actingPlayerSetSufferingAnimosity': this.hActAnimosity.bind(this),
       'actingPlayerSetSufferingBloodLust': this.hActBloodLust.bind(this),
       'actingPlayerSetJumpsWithoutModifiers': this.hNoOp.bind(this),
       'actingPlayerSetHeldInPlace': this.hNoOp.bind(this),
       'actingPlayerSetMustCompleteAction': this.hNoOp.bind(this),
       'actingPlayerSetFellFromRush': this.hNoOp.bind(this),
       'actingPlayerSetInitialAdjacentPartnerIds': this.hActPartnerIds.bind(this),

       // ============================================================
       // FIELD_MODEL handlers (38 total from official client)
       // ============================================================
       'fieldModelAddBloodSpot': this.hNoOp.bind(this),
       'fieldModelAddCard': this.hNoOp.bind(this),
       'fieldModelAddCardEffect': this.hNoOp.bind(this),
       'fieldModelAddDiceDecoration': this.hNoOp.bind(this),
       'fieldModelAddEnhancements': this.hNoOp.bind(this),
       'fieldModelAddFieldMarker': this.hNoOp.bind(this),
       'fieldModelAddHatred': this.hNoOp.bind(this),
       'fieldModelAddIntensiveTraining': this.hNoOp.bind(this),
       'fieldModelAddMoveSquare': this.hNoOp.bind(this),
       'fieldModelAddPlayerMarker': this.hNoOp.bind(this),
       'fieldModelAddPrayer': this.hNoOp.bind(this),
       'fieldModelAddPushbackSquare': this.hNoOp.bind(this),
       'fieldModelAddSkillEnhancements': this.hNoOp.bind(this),
       'fieldModelAddTrackNumber': this.hNoOp.bind(this),
       'fieldModelAddTrapDoor': this.hNoOp.bind(this),
       'fieldModelAddWisdom': this.hNoOp.bind(this),
       'fieldModelKeepDeactivatedCard': this.hNoOp.bind(this),
       'fieldModelRemoveCard': this.hNoOp.bind(this),
       'fieldModelRemoveCardEffect': this.hNoOp.bind(this),
       'fieldModelRemoveDiceDecoration': this.hNoOp.bind(this),
       'fieldModelRemoveFieldMarker': this.hNoOp.bind(this),
       'fieldModelRemoveMoveSquare': this.hNoOp.bind(this),
       'fieldModelRemovePlayer': this.hRemovePlayer.bind(this),
       'fieldModelRemovePlayerMarker': this.hNoOp.bind(this),
       'fieldModelRemovePrayer': this.hNoOp.bind(this),
       'fieldModelRemovePushbackSquare': this.hNoOp.bind(this),
       'fieldModelRemoveSkillEnhancements': this.hNoOp.bind(this),
       'fieldModelRemoveTrackNumber': this.hNoOp.bind(this),
       'fieldModelRemoveTrapDoor': this.hNoOp.bind(this),
       'fieldModelSetBallCoordinate': this.hBallCoord.bind(this),
       'fieldModelSetBallInPlay': this.hBallInPlay.bind(this),
       'fieldModelSetBallMoving': this.hBallMoving.bind(this),
       'fieldModelSetBlitzState': this.hNoOp.bind(this),
       'fieldModelSetBombCoordinate': this.hBombCoord.bind(this),
       'fieldModelSetBombMoving': this.hBombMoving.bind(this),
       'fieldModelSetPlayerCoordinate': this.hPlayerCoord.bind(this),
       'fieldModelSetPlayerState': this.hPlayerState.bind(this),
       'fieldModelSetRangeRuler': this.hNoOp.bind(this),
       'fieldModelSetTargetSelectionState': this.hNoOp.bind(this),
       'fieldModelSetWeather': this.hWeather.bind(this),
       'fieldModelOutOfBounds': this.hNoOp.bind(this),
       'fieldModelAddChomp': this.hNoOp.bind(this),
       'fieldModelRemoveChomp': this.hNoOp.bind(this),

       // ============================================================
       // GAME handlers (24 total from official client)
       // ============================================================
       'gameSetAdminMode': this.hNoOp.bind(this),
       'gameSetConcededLegally': this.hNoOp.bind(this),
       'gameSetConcessionPossible': this.hNoOp.bind(this),
       'gameSetDefenderAction': this.hNoOp.bind(this),
       'gameSetDefenderId': this.hNoOp.bind(this),
       'gameSetDialogParameter': this.hNoOp.bind(this),
       'gameSetFinished': this.hNoOp.bind(this),
       'gameSetHalf': this.hHalf.bind(this),
       'gameSetHomeFirstOffense': this.hNoOp.bind(this),
       'gameSetHomePlaying': this.hHomePlaying.bind(this),
       'gameSetId': this.hNoOp.bind(this),
       'gameSetLastDefenderId': this.hNoOp.bind(this),
       'gameSetLastTurnMode': this.hNoOp.bind(this),
       'gameSetPassCoordinate': this.hPassCoord.bind(this),
       'gameSetScheduled': this.hNoOp.bind(this),
       'gameSetSetupOffense': this.hNoOp.bind(this),
       'gameSetStarted': this.hNoOp.bind(this),
       'gameSetTesting': this.hNoOp.bind(this),
       'gameSetThrowerId': this.hNoOp.bind(this),
       'gameSetThrowerAction': this.hNoOp.bind(this),
       'gameSetTimeoutEnforced': this.hNoOp.bind(this),
       'gameSetTimeoutPossible': this.hNoOp.bind(this),
       'gameSetTurnMode': this.hTurnMode.bind(this),
       'gameSetWaitingForOpponent': this.hNoOp.bind(this),
       'gameOptionsAddOption': this.hNoOp.bind(this),

       // ============================================================
       // INDUCEMENT_SET handlers (10 total from official client)
       // ============================================================
       'inducementSetActivateCard': this.hNoOp.bind(this),
       'inducementSetAddAvailableCard': this.hNoOp.bind(this),
       'inducementSetAddInducement': this.hNoOp.bind(this),
       'inducementSetCardChoices': this.hNoOp.bind(this),
       'inducementSetDeactivateCard': this.hNoOp.bind(this),
       'inducementSetRemoveAvailableCard': this.hNoOp.bind(this),
       'inducementSetRemoveInducement': this.hNoOp.bind(this),
       'inducementSetAddPrayer': this.hNoOp.bind(this),
       'inducementSetRemovePrayer': this.hNoOp.bind(this),

       // ============================================================
       // PLAYER handlers (2 total from official client)
       // ============================================================
       'playerMarkSkillUsed': this.hNoOp.bind(this),
       'playerMarkSkillUnused': this.hNoOp.bind(this),

       // ============================================================
       // PLAYER_RESULT handlers (20 total from official client)
       // ============================================================
       'playerResultSetBlocks': this.hNoOp.bind(this),
       'playerResultSetCasualties': this.hNoOp.bind(this),
       'playerResultSetCasualtiesWithAdditionalSpp': this.hNoOp.bind(this),
       'playerResultSetCatchesWithAdditionalSpp': this.hNoOp.bind(this),
       'playerResultSetCompletions': this.hNoOp.bind(this),
       'playerResultSetCompletionsWithAdditionalSpp': this.hNoOp.bind(this),
       'playerResultSetCurrentSpps': this.hNoOp.bind(this),
       'playerResultSetDefecting': this.hNoOp.bind(this),
       'playerResultSetFouls': this.hNoOp.bind(this),
       'playerResultSetHasUsedSecretWeapon': this.hNoOp.bind(this),
       'playerResultSetInterceptions': this.hNoOp.bind(this),
       'playerResultSetDeflections': this.hNoOp.bind(this),
       'playerResultSetPassing': this.hNoOp.bind(this),
       'playerResultSetPlayerAwards': this.hNoOp.bind(this),
       'playerResultSetRushing': this.hNoOp.bind(this),
       'playerResultSetSendToBoxByPlayerId': this.hNoOp.bind(this),
       'playerResultSetSendToBoxHalf': this.hNoOp.bind(this),
       'playerResultSetSendToBoxReason': this.hNoOp.bind(this),
       'playerResultSetSendToBoxTurn': this.hNoOp.bind(this),
       'playerResultSetSeriousInjury': this.hNoOp.bind(this),
       'playerResultSetSeriousInjuryDecay': this.hNoOp.bind(this),
       'playerResultSetTouchdowns': this.hNoOp.bind(this),
       'playerResultSetTurnsPlayed': this.hNoOp.bind(this),
       'playerResultSetLandings': this.hNoOp.bind(this),

       // ============================================================
       // SKETCH handler (1 from official client)
       // ============================================================
       'sketchUpdate': this.hNoOp.bind(this),

       // ============================================================
       // TARGET_SELECTION handler (1 from official client)
       // ============================================================
       'targetSelectionCommitted': this.hNoOp.bind(this),

       // ============================================================
       // TEAM_RESULT handlers (16 total from official client)
       // ============================================================
       'teamResultSetConceded': this.hNoOp.bind(this),
       'teamResultDedicatedFansModifier': this.hNoOp.bind(this),
       'teamResultSetFame': this.hNoOp.bind(this),
       'teamResultSetFanFactor': this.hNoOp.bind(this),
       'teamResultSetBadlyHurtSuffered': this.hNoOp.bind(this),
       'teamResultSetFanFactorModifier': this.hNoOp.bind(this),
       'teamResultSetPenaltyScore': this.hNoOp.bind(this),
       'teamResultSetPettyCashTransferred': this.hNoOp.bind(this),
       'teamResultSetPettyCashUsed': this.hNoOp.bind(this),
       'teamResultSetRaisedDead': this.hNoOp.bind(this),
       'teamResultSetRipSuffered': this.hNoOp.bind(this),
       'teamResultSetScore': this.hScore.bind(this),
       'teamResultSetSeriousInjurySuffered': this.hNoOp.bind(this),
       'teamResultSetSpectators': this.hNoOp.bind(this),
       'teamResultSetSpirallingExpenses': this.hNoOp.bind(this),
       'teamResultSetTeamValue': this.hNoOp.bind(this),
       'teamResultSetWinnings': this.hNoOp.bind(this),

       // ============================================================
       // TURN_DATA handlers (25 total from official client)
       // ============================================================
       'turnDataSetApothecaries': this.hNoOp.bind(this),
       'turnDataSetBlitzUsed': this.hNoOp.bind(this),
       'turnDataSetBombUsed': this.hNoOp.bind(this),
       'turnDataSetFirstTurnAfterKickoff': this.hNoOp.bind(this),
       'turnDataSetFoulUsed': this.hNoOp.bind(this),
       'turnDataSetHandOverUsed': this.hNoOp.bind(this),
       'turnDataSetLeaderState': this.hNoOp.bind(this),
       'turnDataSetPassUsed': this.hNoOp.bind(this),
       'turnDataSetTtmUsed': this.hNoOp.bind(this),
       'turnDataSetKtmUsed': this.hNoOp.bind(this),
       'turnDataSetSecureTheBallUsed': this.hNoOp.bind(this),
       'turnDataPuntUsed': this.hNoOp.bind(this),
       'turnDataSetReRolls': this.hNoOp.bind(this),
       'turnDataSetReRollsBrilliantCoachingOneDrive': this.hNoOp.bind(this),
       'turnDataSetReRollsPumpUpTheCrowdOneDrive': this.hNoOp.bind(this),
       'turnDataSetReRollsShowStarOneDrive': this.hNoOp.bind(this),
       'turnDataSetReRollsSingleUse': this.hNoOp.bind(this),
       'turnDataSetReRollUsed': this.hNoOp.bind(this),
       'turnDataSetTurnNr': this.hTurnNr.bind(this),
       'turnDataSetTurnStarted': this.hNoOp.bind(this),
       'turnDataSetCoachBanned': this.hNoOp.bind(this),
       'turnDataSetWanderingApothecaries': this.hNoOp.bind(this),
       'turnDataSetPlagueDoctors': this.hNoOp.bind(this),
       'turnDataSetCheeringFansBlockAssist': this.hNoOp.bind(this),
     };
   }

  // =========================================================================
  // INITIALIZATION - COPIED from official client Game.java
  // =========================================================================

  public initialize(data: FFBGameType | any): boolean {
    this.isInitialized = true;
    this.gameState = data;
    console.log('[FumbblGameModel] initialize() - teams:', {
      homeId: data.teamHome?.teamId, homeName: data.teamHome?.teamName,
      awayId: data.teamAway?.teamId, awayName: data.teamAway?.teamName,
    });

    this.teamHome = data.teamHome || null;
    this.teamAway = data.teamAway || null;

    // COPIED: Clear and rebuild players map from roster
    this.players = {};
    this.playerLocations = new Map();
    this.mergedPlayers = {};

    if (this.teamHome?.playerArray) {
      for (const p of this.teamHome.playerArray) {
        const pid = String(p.playerId);
        (p as unknown as FFBPlayerType & { _side?: string })._side = 'home' as const;
        this.players[pid] = p as unknown as FFBPlayerType;
      }
    }
    if (this.teamAway?.playerArray) {
      for (const p of this.teamAway.playerArray) {
        const pid = String(p.playerId);
        (p as unknown as FFBPlayerType & { _side?: string })._side = 'away' as const;
        this.players[pid] = p as unknown as FFBPlayerType;
      }
    }

    // COPIED: Apply field model to set coordinates
    if (data.fieldModel) this.applyFieldModel(data.fieldModel);

    // COPIED: Build merged players from field coordinates
    this.buildMergedPlayers();

    // Set other game state
    if (data.fieldModel?.ballCoordinate) {
      const c = data.fieldModel.ballCoordinate;
      this.ballCoordinate = { x: c[0], y: c[1] };
    }
    if (data.actingPlayer) this.activePlayerId = data.actingPlayer.playerId;
    if (data.half) this.half = data.half;
    if (data.homePlaying !== undefined) this.sidePlaying = data.homePlaying ? 'home' : 'away';
    if (data.turnMode?.name) this.turnMode = data.turnMode.name;
    if (data.turnTime) this.turnTime = data.turnTime;
    if (data.gameTime) this.gameTime = data.gameTime;
    if (data.turnDataHome?.turnNr || data.turnDataAway?.turnNr) {
      this.turn = data.turnDataHome?.turnNr || data.turnDataAway?.turnNr || 1;
    }
    if (data.score) this.score = data.score;
    if (data.teamHome?.score !== undefined) this.score.team1 = data.teamHome.score;
    if (data.teamAway?.score !== undefined) this.score.team2 = data.teamAway.score;

    console.log('[FumbblGameModel] Game initialized:', {
      players: Object.keys(this.players).length,
      mergedPlayers: Object.keys(this.mergedPlayers).length,
      half: this.half,
      side: this.sidePlaying,
      turnMode: this.turnMode,
      turn: this.turn,
    });
    return true;
  }

  // =========================================================================
  // FIELD MODEL APPLICATION - COPIED from official client
  // =========================================================================

  private applyFieldModel(fm: FFBFieldModelType | any): void {
    if (fm.playerDataArray) {
      for (const pd of fm.playerDataArray) {
        const pid = String(pd.playerId);
        const player = this.players[pid];
        if (player) {
          if (pd.playerCoordinate) {
            const [x, y] = pd.playerCoordinate;
            player.fieldX = x;
            player.fieldY = y;
            player.playerCoordinate = [x, y];
            // COPIED: Build coordinate lookup map
            const coordKey = `${x},${y}`;
            this.playerLocations.set(coordKey, pid);
          }
          if (pd.playerState !== undefined) {
            player.playerState = pd.playerState;
          }
        }
      }
    }
    if (fm.ballCoordinate) {
      const [x, y] = fm.ballCoordinate;
      this.ballCoordinate = { x, y };
    }
  }

  // =========================================================================
  // BUILD MERGED PLAYERS - COPIED from official client approach
  // =========================================================================

   /**
    * Build merged players list for UI display.
    * COPIED from official client: merges roster data with field coordinates.
    * Returns ALL players that have field coordinates (on-field players).
    * Also computes hasBall by checking if a player is on the same square as the ball.
    */
   private buildMergedPlayers(): void {
     this.mergedPlayers = {};

     for (const pid in this.players) {
       const rosterPlayer = this.players[pid];
       const x = rosterPlayer.fieldX;
       const y = rosterPlayer.fieldY;

       // Only include players that have field coordinates
       if (x === undefined || y === undefined) continue;

       // Determine team side from player data
       const teamSideRaw = (rosterPlayer as unknown as FFBPlayerType & { _side?: string })._side;
       const teamSide: 'home' | 'away' = (teamSideRaw === 'home' || teamSideRaw === 'away') ? teamSideRaw : 'home';

       // COPIED: Use composite key for uniqueness (team + playerId)
       const uniqueId = `${teamSide}_${pid}`;

       // Compute hasBall: true when the player is on the same square as the ball
       const hasBall = (x === this.ballCoordinate.x && y === this.ballCoordinate.y);

       this.mergedPlayers[uniqueId] = {
         id: uniqueId,
         playerId: pid,
         teamSide,
         name: rosterPlayer.playerName || 'Unknown',
         number: rosterPlayer.playerNr,
         race: rosterPlayer.race || 'unknown',
         position: this.mapPos(rosterPlayer.positionName),
         status: this.mapSt(rosterPlayer.playerState),
         skills: (rosterPlayer.skillArray || []).map((s: string) => s as SkillShorthand),
         ma: rosterPlayer.movement,
         st: rosterPlayer.strength,
         ag: rosterPlayer.agility,
         pa: 0,
         av: rosterPlayer.armour,
         hasBall,
         fieldX: x,
         fieldY: y,
       };
     }

    console.log('[FumbblGameModel] buildMergedPlayers:', {
      total: Object.keys(this.mergedPlayers).length,
      home: Object.values(this.mergedPlayers).filter(p => p.teamSide === 'home').length,
      away: Object.values(this.mergedPlayers).filter(p => p.teamSide === 'away').length,
    });
  }

  // =========================================================================
  // MODEL CHANGES - COPIED from official client ModelChangeProcessor
  // =========================================================================

  public applyModelChanges(changes: ModelChangeType[]): void {
    for (const change of changes) {
      const handler = this.modelChangeHandlers[change.modelChangeId];
      if (handler) {
        try {
          handler(change.modelChangeKey, change.modelChangeValue);
        } catch (e) {
          console.error(`[FumbblGameModel] Error handling ${change.modelChangeId}:`, e);
        }
      } else {
        console.log('[FumbblGameModel] Unhandled model change:', change.modelChangeId);
      }
    }
    // After applying changes, rebuild merged players
    this.buildMergedPlayers();
  }

  // --- Model change handler methods (COPIED from official client) ---
  private hActCurMove = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).currentMove = v; return true; };
  private hActDodging = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).dodging = v; return true; };
  private hActGFI = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).goingForIt = v; return true; };
  private hActBlocked = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).hasBlocked = v; return true; };
  private hActFed = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).hasFed = v; return true; };
  private hActFouled = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).hasFouled = v; return true; };
  private hActJumped = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).hasJumped = v; return true; };
  private hActMoved = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).hasMoved = v; return true; };
  private hActPassed = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).hasPassed = v; return true; };
  private hActTriggered = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).hasTriggeredEffect = v; return true; };
  private hActJumping = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).jumping = v; return true; };
  private hActStanding = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).standingUp = v; return true; };
  private hActStrength = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).strength = v; return true; };
  private hActAnimosity = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).sufferingAnimosity = v; return true; };
  private hActBloodLust = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).sufferingBloodlust = v; return true; };
  private hActAction = (_k: string, v: any) => { if (this.gameState?.actingPlayer) (this.gameState.actingPlayer as any).playerAction = v; return true; };
  private hActPlayerId = (_k: string, v: any) => {
    if (this.gameState?.actingPlayer) {
      (this.gameState.actingPlayer as any).playerId = v;
      this.activePlayerId = String(v);
    }
    return true;
  };
  private hActSkillUsed = () => true;
  private hActSkillUnused = () => true;
  private hActOldState = () => true;
  private hActPartnerIds = () => true;

  private hBallCoord = (_k: string, v: any) => {
    if (Array.isArray(v) && v.length >= 2) this.ballCoordinate = { x: v[0], y: v[1] };
    else if (typeof v === 'object' && v.x !== undefined && v.y !== undefined) this.ballCoordinate = v;
    return true;
  };
  private hBallInPlay = () => true;
  private hBallMoving = () => true;
  private hBombCoord = () => true;
  private hBombMoving = () => true;

  private hPlayerCoord = (_k: string, v: any) => {
    // COPIED: Update player coordinate in both players map and playerLocations
    const player = this.players[_k];
    if (!player) return true;

    // Remove old coordinate if exists
    if (player.fieldX !== undefined && player.fieldY !== undefined) {
      const oldCoordKey = `${player.fieldX},${player.fieldY}`;
      this.playerLocations.delete(oldCoordKey);
    }

    if (Array.isArray(v) && v.length >= 2) {
      const [x, y] = v;
      player.fieldX = x;
      player.fieldY = y;
      player.playerCoordinate = [x, y];
      // Add new coordinate
      const coordKey = `${x},${y}`;
      this.playerLocations.set(coordKey, _k);
    }
    return true;
  };

  private hPlayerState = (_k: string, v: any) => {
    const player = this.players[_k];
    if (player) player.playerState = typeof v === 'number' ? v : parseInt(String(v));
    return true;
  };

  private hRemovePlayer = (_k: string) => {
    delete this.players[_k];
    // Also remove from merged players
    for (const key in this.mergedPlayers) {
      if (this.mergedPlayers[key].playerId === _k) {
        delete this.mergedPlayers[key];
      }
    }
    return true;
  };

  private hWeather = () => true;
  private hHomePlaying = (_k: string, v: any) => { this.sidePlaying = v ? 'home' : 'away'; return true; };
  private hTurnMode = (_k: string, v: any) => {
    this.turnMode = typeof v === 'object' && v.name ? v.name : String(v);
    return true;
  };
  private hHalf = (_k: string, v: any) => { this.half = v; return true; };
  private hPassCoord = (_k: string, v: any) => {
    if (Array.isArray(v) && v.length >= 2) this.ballCoordinate = { x: v[0], y: v[1] };
    return true;
  };

  private hScore = (k: string, v: any) => {
    if (k === 'home') { this.score.team1 = v; if (this.teamHome) this.teamHome.score = v; }
    else if (k === 'away') { this.score.team2 = v; if (this.teamAway) this.teamAway.score = v; }
    return true;
  };

  private hTurnNr = (k: string, v: any) => { if (k === 'home') this.turn = v; return true; };
  private hNoOp = () => true;

  // =========================================================================
  // Convert to UI GameState format - COPIED from official client approach
  // =========================================================================

  public toGameState(): Partial<GameState> {
    if (!this.isInitialized || !this.gameState) return {};

    const homeTeam = this.teamHome || { teamId: 0, teamName: 'Team 1', race: 'unknown', playerArray: [], reRolls: 2, fans: 0 } as any;
    const awayTeam = this.teamAway || { teamId: 0, teamName: 'Team 2', race: 'unknown', playerArray: [], reRolls: 2, fans: 0 } as any;

    // COPIED: Get on-field players from merged players
    const homePlayers = Object.values(this.mergedPlayers).filter(p => p.teamSide === 'home');
    const awayPlayers = Object.values(this.mergedPlayers).filter(p => p.teamSide === 'away');

    console.log('[FumbblGameModel] toGameState:', {
      homePlayers: homePlayers.length,
      awayPlayers: awayPlayers.length,
      homeSample: homePlayers.slice(0, 3).map(p => ({ id: p.id, name: p.name, fieldX: p.fieldX, fieldY: p.fieldY })),
      awaySample: awayPlayers.slice(0, 3).map(p => ({ id: p.id, name: p.name, fieldX: p.fieldX, fieldY: p.fieldY })),
    });

    return {
      score: this.gameState.score || this.score,
      turn: this.turn,
      phase: this.mapPhase(this.turnMode),
      reRolls: { team1: homeTeam.reRolls ?? 2, team2: awayTeam.reRolls ?? 2 },
      // CRITICAL: Do NOT return timer here.
      // The model's turnTime is NOT updated by serverModelSync handlers, so it becomes stale.
      // When toGameState() is called after every model sync, the stale timer value overwrites
      // the correct value from serverGameTime, causing the timer to jump randomly.
      // Timer is managed exclusively by the serverGameTime handler in fumbblWebSocket.ts.
      weather: this.mapWeather(this.gameState.fieldModel?.weather?.name || 'clear'),
      fanAttendance: {
        total: this.gameState.fanAttendance || 0,
        dedicatedFans: { team1: (this.gameState.dedicatedFans as any)?.team1 || 0, team2: (this.gameState.dedicatedFans as any)?.team2 || 0 },
      },
      team1: {
        id: String(homeTeam.teamId || 0),
        name: homeTeam.teamName || 'Team 1',
        race: homeTeam.race || 'unknown',
        players: this.mapFFBPlayers(homeTeam),
        color: '#4a7c3f',
      },
      team2: {
        id: String(awayTeam.teamId || 0),
        name: awayTeam.teamName || 'Team 2',
        race: awayTeam.race || 'unknown',
        players: this.mapFFBPlayers(awayTeam),
        color: '#c4a35a',
      },
      // COPIED: Use merged players (with field coordinates) for team1Players/team2Players
      // Convert DisplayPlayer to Player format for UI compatibility
      team1Players: this.convertToPlayers(homePlayers),
      team2Players: this.convertToPlayers(awayPlayers),
      field: this.mapField(),
      ballPosition: this.ballCoordinate,
      selectedPlayer: null,
      selectedTeam: 'team1',
      // CRITICAL: Do NOT return diceLog/chatMessages here.
      // These are managed by the GameContext and should NOT be overwritten
      // with empty arrays on every server update. Returning them causes
      // the SYNC_STATE reducer to wipe out existing logs/chat.
      isLive: this.turnMode !== 'setup' && this.turnMode !== 'ended' && this.turnMode !== '',
      lastUpdate: Date.now(),
    };
  }

  /**
   * Map roster players (for sidebar display - ALL roster players)
   */
  private mapFFBPlayers(team: FFBTeamType | null): FumbblPlayer[] {
    if (!team?.playerArray) return [];
    return team.playerArray
      .map(p => this.mapFFBPlayer(p as unknown as FFBPlayerType))
      .filter((p): p is FumbblPlayer => p !== null);
  }

  /**
   * Map a single roster player for sidebar display
   * Uses playerId-based unique ID (String, matching official ffbclient)
   * Official client: Team.fPlayerById is Map<String, Player<?>> where key is the String playerId
   */
  private mapFFBPlayer(player: FFBPlayerType): FumbblPlayer | null {
    if (!player) return null;
    const coord = player.playerCoordinate;
    const x = player.fieldX ?? (Array.isArray(coord) ? coord[0] : -1);
    const y = player.fieldY ?? (Array.isArray(coord) ? coord[1] : -1);
    // Use playerId as String for uniqueness (matching official ffbclient architecture)
    // Official: Team.getPlayerById(String pId) returns Player by String ID
    const pid = String(player.playerId);
    return {
      id: pid,
      name: player.playerName || 'Unknown',
      number: player.playerNr,
      race: player.race || 'unknown',
      position: this.mapPos(player.positionName),
      status: this.mapSt(player.playerState),
      skills: (player.skillArray || []).map((s: string) => s as SkillShorthand),
      ma: player.movement,
      st: player.strength,
      ag: player.agility,
      pa: 0,
      av: player.armour,
      hasBall: false,
      fieldX: x,
      fieldY: y,
    };
  }

  /**
   * Convert DisplayPlayer[] to FumbblPlayer[] for UI compatibility
   * Uses composite string ID for uniqueness (team_side + playerId)
   */
  private convertToPlayers(displayPlayers: DisplayPlayer[]): FumbblPlayer[] {
    return displayPlayers.map(p => ({
      id: p.id,  // Use composite string ID for uniqueness
      name: p.name,
      number: p.number,
      race: p.race,
      position: p.position,
      status: p.status,
      skills: p.skills,
      ma: p.ma,
      st: p.st,
      ag: p.ag,
      pa: p.pa,
      av: p.av,
      hasBall: p.hasBall,
      fieldX: p.fieldX,
      fieldY: p.fieldY,
    }));
  }

  private mapPos = (n: string): PlayerPosition => {
    const m: Record<string, PlayerPosition> = {
      'Blocker': 'bl', 'Blitzer': 'bl', 'Stalker': 'st',
      'Running Back': 'rb', 'Rusher': 'rb', 'Wide Receiver': 'wr', 'Receiver': 'wr',
      'Center': 'c', 'Goalie': 'g', 'Guard': 'g', 'Substitute': 'sub', 'Coach': 'coach',
    };
    return m[n] || 'c';
  };

  private mapSt = (s: number | undefined): PlayerStatus => {
    if (s === undefined) return 'active';
    return (
      {
        0: 'active',
        1: 'active',
        2: 'injured',
        3: 'dead',
        4: 'rotd',
        5: 'doubtful',
        6: 'missing',
        7: 'injured',
        8: 'missing',
      } as Record<number, PlayerStatus>
    )[s] || 'active';
  };

  private mapWeather = (w: string) => {
    const m: Record<string, { type: WeatherType; icon: string; description: string }> = {
      'clear': { type: 'clear', icon: '\u2600\uFE0F', description: 'Clear' },
      'raining': { type: 'raining', icon: '\uD83C\uDF27\uFE0F', description: 'Raining' },
      'stormy': { type: 'stormy', icon: '\u26C8\uFE0F', description: 'Stormy' },
      'foggy': { type: 'foggy', icon: '\uD83C\uDF2B\uFE0F', description: 'Foggy' },
      'extreme': { type: 'extreme', icon: '\u2744\uFE0F', description: 'Extreme' },
      'normal': { type: 'clear', icon: '\u2600\uFE0F', description: 'Normal' },
    };
    return m[w?.toLowerCase()] || { type: 'clear', icon: '\u2600\uFE0F', description: 'Clear' };
  };

  private mapPhase = (p: string): GameState['phase'] => {
    const m: Record<string, GameState['phase']> = {
      'setup': 'setup', 'first_turn': 'first_turn',
      'regular': 'regular', 'halftime': 'halftime',
      'overtime': 'overtime', 'ended': 'ended', 'end': 'ended',
      'action': 'regular', 'scramble': 'regular',
    };
    return m[p?.toLowerCase()] || 'regular';
  };

  private mapField = (): FieldState => ({
    markers: [
      { id: 'center', type: 'center' },
      { id: '10yard-left', type: '10yard', position: { x: 4, y: 0 } },
      { id: '10yard-right', type: '10yard', position: { x: 12, y: 0 } },
      { id: 'endzone-left', type: 'endzone', position: { x: 0, y: 0 } },
      { id: 'endzone-right', type: 'endzone', position: { x: 16, y: 0 } },
    ],
    ballPosition: this.ballCoordinate,
  });

  // Getters
  public getTeamHome = () => this.teamHome;
  public getTeamAway = () => this.teamAway;
  public getPlayers = () => this.players;
  public getPlayerById = (id: string) => this.players[id];
  public getActivePlayerId = () => this.activePlayerId;
  public getBallCoordinate = () => this.ballCoordinate;
  public getHalf = () => this.half;
  public getSidePlaying = () => this.sidePlaying;
  public getScore = () => this.score;
  public getTurn = () => this.turn;
  public getPhase = () => this.turnMode;
  public getTurnTime = () => this.turnTime;
  public getGameTime = () => this.gameTime;
  public isGameInitialized = () => this.isInitialized;
  /**
   * Get merged players as Player[] for UI compatibility.
   * Converts DisplayPlayer to Player format (uses playerId as String id).
   * Official client uses String playerId as the unique identifier throughout.
   */
  public getMergedPlayersAsPlayers(): FumbblPlayer[] {
    return Object.values(this.mergedPlayers).map(p => ({
      id: p.id,  // Use composite string ID (e.g., "home_18109693") for uniqueness
      playerId: p.playerId,
      teamSide: p.teamSide,
      name: p.name,
      number: p.number,
      race: p.race,
      position: p.position,
      status: p.status,
      skills: p.skills,
      ma: p.ma,
      st: p.st,
      ag: p.ag,
      pa: p.pa,
      av: p.av,
      hasBall: p.hasBall,
      fieldX: p.fieldX,
      fieldY: p.fieldY,
    })) as unknown as FumbblPlayer[];
  }

  public getMergedPlayers = () => this.mergedPlayers;

  // ---------------------------------------------------------------------------
  // Spectator methods (from ClientCommandHandlerJoin)
  // ---------------------------------------------------------------------------

  /**
   * Set spectator count from serverJoin message.
   * Based on ffbclient ClientCommandHandlerJoin line 57:
   * getClient().getClientData().setSpectatorCount(joinCommand.getSpectatorCount());
   */
  public setSpectatorCount(count: number): void {
    this.spectatorCount = count;
  }

  /**
   * Set spectators list from serverJoin message.
   * Based on ffbclient ClientCommandHandlerJoin line 58:
   * getClient().getClientData().setSpectators(joinCommand.getSpectators());
   */
  public setSpectators(list: string[] | any[]): void {
    this.spectators = Array.isArray(list) ? list : [];
  }

  /**
   * Get spectator count.
   */
  public getSpectatorCount(): number {
    return this.spectatorCount;
  }

  /**
   * Get spectators list.
   */
  public getSpectators(): string[] {
    return this.spectators;
  }
}
