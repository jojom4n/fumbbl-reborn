// =============================================================================
// Blood Bowl 2025 - TypeScript Type Definitions
// Reference: https://bloodbowlbase.ru/bb2025/core_rules/
// =============================================================================

// -----------------------------------------------------------------------------
// Player Status
// -----------------------------------------------------------------------------

export type PlayerStatus =
  | 'active'        // Player is on the field and available
  | 'injured'       // Player is injured (Dirt, Broken Leg, etc.)
  | 'dead'          // Player is permanently dead
  | 'rotd'          // Rest of Turn Down (concussion, etc.)
  | 'doubtful'      // Player is doubtful (can play but risk injury)
  | 'missing';      // Player is absent (suspension, etc.)

// -----------------------------------------------------------------------------
// Blood Bowl 2025 Actions (9 official actions)
// -----------------------------------------------------------------------------

export type BloodBowlAction =
  | 'move'          // M — Move up to MA squares (always available)
  | 'secureBall'    // S — Secure the ball (when player has the ball)
  | 'block'         // B — Block/attack adjacent opponent (when adjacent)
  | 'blitz'         // L — Blitz: grab ball + move 1 square (always available)
  | 'pass'          // P — Forward pass (when player has the ball)
  | 'handOff'       // H — Hand-off to adjacent teammate (when player has ball)
  | 'throwTeamMate' // T — Throw a teammate (when has Strong Arm)
  | 'foul'          // F — Commit a tactical foul (always available)
  | 'special';      // E — Special action (depends on player/ability)

// Action metadata for radial menu display
export interface ActionMetadata {
  action: BloodBowlAction;
  hotkey: string;     // M, S, B, L, P, H, T, F, E
  label: string;      // Display label
  icon?: string;      // Optional icon identifier
  requiresBall: boolean;
  requiresAdjacent: boolean;
  requiresStrongArm: boolean;
}

// -----------------------------------------------------------------------------
// Blood Bowl 2025 Skills
// -----------------------------------------------------------------------------

// Shorthand skill aliases used in FUMBBL UI
export type SkillShorthand =
  | 'AG'   // Dodge
  | 'D'    // Dodge (shorthand)
  | 'B'    // Block
  | 'ST'   // Mighty Blows / Strength
  | 'C'    // Catch
  | 'AN'   // Anchor
  | 'NK'   // No Kicks Left
  | 'SV'   // Sure Hands
  | 'K'    // Keeper
  | 'T'    // Tackle
  | 'M'    // Move
  | 'S'    // Secure Ball
  | 'L'    // Blitz
  | 'H'    // Hand-Off
  | 'P'    // Pass
  | 'F'    // Foul
  | 'E'    // Special
  | 'AP'   // Assassin
  | 'SV'   // Sure Hands
  | 'PD'   // Piledriver
  | 'RH'   // Rags to Riches
  | 'W'    // Wicked Claw
  | 'UF'   // Unfailing
  | 'FF'   // Frenzy of the Fallen
  | 'KV'   // Killer Hand
  | 'SS'   // Stiff Spring
  | 'RH'   // Rags to Riches
  | 'CL'   // Claw
  | 'SP'   // Special
  | 'EX'   // Extra
  | 'BD'   // Bloodlust
  | 'BT'   // Brawler's Tenacity
  | 'DR'   // Dark Rituals
  | 'FB'   // Frenzy
  | 'GW'   // Guardian's Ward
  | 'HC'   // Hard Contact
  | 'IW'   // Iron Will
  | 'JC'   // Juggernaut's Constitution
  | 'LM'   // Lifeblood
  | 'MA'   // Magic Resistance
  | 'NC'   // Never Catch
  | 'OT'   // On the Ball
  | 'PA'   // Pass
  | 'PK'   // Pick Up
  | 'RC'   // Raging Catch
  | 'SD'   // Sure Hands Dodge
  | 'SK'   // Sure Hands Kick
  | 'SP'   // Special
  | 'ST'   // Stiff Arm
  | 'SV'   // Sure Hands
  | 'T'    // Tackle
  | 'TB'   // Tough Bastard
  | 'UK'   // Unkillable
  | 'VA'   // Vitality
  | 'WD'   // Wound Duration
  | 'anchor'          // Cannot be pushed or knocked down
  | 'athletic'        // Can dive pass, dive for end zone, pick up loose ball
  | 'block'           // Can attempt blocks
  | 'claw'            // Natural weapon (attack without hand)
  | 'catch'           // Can catch passes
  | 'cleanliness'     // No dirty player penalty
  | 'dodge'           // Can dodge
  | 'foul_computer'   // Can commit fouls (computer controlled)
  | 'hand_to_claw'    // Can transfer ball from claw to hand
  | 'intangible'      // Cannot be blocked (ghosts, etc.)
  | 'jungle_fighting' // +1 to block rolls in jungle
  | 'knock_down'      // Successful block knocks down opponent
  | 'mighty_blows'    // Successful block causes armor break on 10+
  | 'nerves'          // Can attempt to pick up loose ball
  | 'pass'            // Can throw passes
  | 'pick_up'         // Can pick up loose ball
  | 'pretty_killer'   // +1 to block rolls vs specific race
  | 'stiff_arm'       // Can stiff arm during movement
  | 'strong_arm'      // Can throw team-mate
  | 'tackle'          // Successful block causes opponent to miss next turn
  | 'unyielding'      // Can stand up after being knocked down
  | 'wrestle'         // Can wrestle opponent down

  // Advanced Skills
  | 'acrobatic'       // Can throw team-mate without Strong Arm
  | 'agent_of_chaos'  // Reroll first dodge each turn
  | 'ambidextrous'    // Can throw with either hand
  | 'apotropaic'      // Ward off magic (Warrior Priest)
  | 'aura_of_neverending_adoration' // Fan Attendance +1 each half
  | 'big_eye'         // Reroll miss on long pass
  | 'brawler'         // Block without needing adjacent opponent
  | 'come_again'      // Reroll failed dodge once per turn
  | 'come_on_you_lads' // Reroll missed touchdown celebration
  | 'curved_throws'   // Can throw spirals (deflect passes)
  | 'daemonbone_arm'  // Mighty blows on 9+
  | 'dauntless'       // Reroll failed courage tests
  | 'dead_hand'       // Passes cannot be deflected
  | 'diving_catch'    // Can dive for ball after catch
  | 'diving_run'      // Can dive for end zone
  | 'dumb_fouls'      // Computer commits fouls automatically
  | 'extra_skeleton'  // Can be used as substitute
  | 'finesse'         // Choose which die to use for block
  | 'freak'           // Reroll first failed dodge each turn
  | 'fumble_fortunes' // Reroll first fumble each turn
  | 'gallant_captain' // Reroll failed courage for adjacent teammates
  | 'grab_the_ball'   // Can grab ball from ground without athletic
  | 'grudge'          // +1 to block vs specific race
  | 'hairy'           // -1 to injury rolls
  | 'hold_ball'       // Cannot be knocked down with ball
  | 'ivory_tower'     // Magic resistance
  | 'keeper'          // Goalies get +1 to catch rolls
  | 'lacrosse_hands'  // Can catch with one hand
  | 'leader'          // Reroll courage tests for adjacent teammates
  | 'let_down'        // Adjacent teammates reroll failed dodge
  | 'long_throw'      // Passes can deflect
  | 'magic_resistance' // Resist magic effects
  | 'manhunter'       // +1 to block vs humans
  | 'matador'         // Reroll first failed dodge when marked
  | 'mighty_thews'    // Ignore wrestle
  | 'never_catches'   // Cannot catch (golems, etc.)
  | 'on_the_ball'     // +1 to pass rolls
  | 'outwit'          // Reroll first failed dodge each turn
  | 'peculiar'        // Ignore first casualty per game
  | 'pick_off_pass'   // Can intercept passes (defenders)
  | 'poor_learner'    // -1 to block rolls
  | 'pretty_killer'   // +1 to block vs specific race
  | 'proud_face'      // Ignore first casualty per game
  | 'puck'            // Reroll failed catches
  | 'quick_saver'     // Reroll first injury roll per game
  | 'rags_to_riches'  // Dedicated Fans +1 at start
  | 'rock_jaw'        // Reroll first injury roll per game
  | 'running_pass'    // Can pass while moving
  | 'scurry'          // Reroll first failed dodge each turn
  | 'secret_weapon'   // Special ability
  | 'siren_limb'      // Replace injured limb
  | 'snort'           // Reroll first failed dodge each turn
  | 'special_scout'   // Scout with bonus
  | 'stinker'         // Opponents -1 to block when adjacent
  | 'sure_hands'      // Cannot fumble
  | 'troll_blood'     // Reroll injury rolls once per game
  | 'wicket'          // Can block with 10+
  | 'wonderlegs'      // Move 2 extra squares
  | 'wrangling'       // Prevent stiff arm

// -----------------------------------------------------------------------------
// Player Positions
// -----------------------------------------------------------------------------

export type PlayerPosition =
  | 'ap'      // Assassin (ball carrier)
  | 'bl'      // Blocker
  | 'st'      // Stalker
  | 'rb'      // Running Back
  | 'wr'      // Wide Receiver
  | 'c'       // Center
  | 'lh'      // Left Halfback
  | 'rh'      // Right Halfback
  | 'g'       // Goalie
  | 'mg'      // Middle Goalie
  | 'fg'      // Forward Goalie
  | 'lh2'     // Left Halfback 2
  | 'rh2'     // Right Halfback 2
  | 'sub'     // Substitute
  | 'coach';  // Coach (not on field)

// -----------------------------------------------------------------------------
// Player
// -----------------------------------------------------------------------------

export interface Player {
  id: number;
  name: string;
  number: number;
  race: string;              // Orc, Elf, Human, Dwarf, etc.
  position: PlayerPosition;
  status: PlayerStatus;
  skills: SkillShorthand[];
  ma: number;                // Movement value
  st: number;                // Strength
  ag: number;                // Agility
  pa: number;                // Pass value
  av: number;                // Armor Value
  hasBall?: boolean;         // Whether player currently has the ball
  // UI state
  fieldX?: number;           // X position on field (0-16)
  fieldY?: number;           // Y position on field (0-9)
}

// -----------------------------------------------------------------------------
// Team
// -----------------------------------------------------------------------------

export interface Team {
  id: string;
  name: string;
  race: string;
  logoUrl?: string;          // URL to team logo
  players: Player[];
  color: string;             // Primary team color
  secondaryColor?: string;   // Secondary team color
}

// -----------------------------------------------------------------------------
// Weather
// -----------------------------------------------------------------------------

export type WeatherType =
  | 'clear'
  | 'raining'
  | 'stormy'
  | 'foggy'
  | 'extreme'
  | 'none';

export interface Weather {
  type: WeatherType;
  icon: string;
  description: string;
}

// -----------------------------------------------------------------------------
// Re-Rolls
// -----------------------------------------------------------------------------

export interface ReRolls {
  team1: number;    // Number of re-rolls for team 1
  team2: number;    // Number of re-rolls for team 2
}

// -----------------------------------------------------------------------------
// Fan Attendance (BB2025)
// -----------------------------------------------------------------------------

export interface FanAttendance {
  total: number;             // Total Fan Attendance value
  dedicatedFans: {
    team1: number;           // Dedicated fans for team 1
    team2: number;           // Dedicated fans for team 2
  };
  lastRoll?: {
    timestamp: number;
    dice: number[];
    mod: number;
  };
}

// -----------------------------------------------------------------------------
// Field
// -----------------------------------------------------------------------------

export interface FieldPosition {
  x: number;    // 0-16 (17 columns)
  y: number;    // 0-9 (10 rows)
}

export interface FieldMarker {
  id: string;
  type: 'endzone' | '10yard' | 'center' | 'sideline';
  position?: FieldPosition;
}

export interface FieldState {
  markers: FieldMarker[];
  ballPosition: FieldPosition;
  // Active effects
  pow?: FieldPosition;       // Power of Ynnehead explosion
  defenderDown?: FieldPosition[];
  bothDown?: FieldPosition[];
  pushDirection?: 'left' | 'right';
}

// -----------------------------------------------------------------------------
// Dice Log Entry
// -----------------------------------------------------------------------------

export type DiceLogType =
  | 'block'         // Block attempt
  | 'block_roll'    // Block dice roll result
  | 'armor'         // Armor roll
  | 'injury'        // Injury roll
  | 'dodge'         // Dodge roll
  | 'catch'         // Catch roll
  | 'pass'          // Pass result
  | 'touchdown'     // Touchdown celebration
  | 'casualty'      // Casualty result
  | 'action'        // General action (Blitz, Foul, etc.)
  | 'weather'       // Weather change
  | 'fan'           // Fan Attendance change
  | 'system';       // System message

export interface DiceLogEntry {
  id: number;
  type: DiceLogType;
  timestamp: number;
  text: string;      // Formatted display text
  color?: string;    // Override color (red for casualty, etc.)
  dice?: number[];   // Dice values rolled
  target?: number;   // Target number
  result?: 'success' | 'failure' | 'partial';
  turn?: number;     // Game turn this entry belongs to
}

// -----------------------------------------------------------------------------
// Chat Message
// -----------------------------------------------------------------------------

export interface ChatMessage {
  id: number;
  sender: string;
  senderColor?: string;
  text: string;
  timestamp: number;
  type?: 'general' | 'team' | 'system';
}

// -----------------------------------------------------------------------------
// Game State (Global)
// -----------------------------------------------------------------------------

export interface GameState {
  // Match info
  score: { team1: number; team2: number };
  turn: number;
  phase: 'setup' | 'first_turn' | 'regular' | 'halftime' | 'overtime' | 'ended';
  reRolls: ReRolls;
  timer: number;              // Seconds remaining
  weather: Weather;
  fanAttendance: FanAttendance;

  // Teams
  team1: Team;
  team2: Team;
  team1Players: Player[];
  team2Players: Player[];

  // Field
  field: FieldState;
  ballPosition: FieldPosition;

  // Selection
  selectedPlayer: Player | null;
  selectedTeam: 'team1' | 'team2';

  // Dice Log
  diceLog: DiceLogEntry[];

  // Chat
  chatMessages: ChatMessage[];

  // Game status
  isLive: boolean;
  lastUpdate: number;
}

// -----------------------------------------------------------------------------
// Action Result
// -----------------------------------------------------------------------------

export interface ActionResult {
  success: boolean;
  action: BloodBowlAction;
  player: Player;
  message: string;
  diceLog?: DiceLogEntry[];
  newState?: Partial<GameState>;
}

// -----------------------------------------------------------------------------
// Radial Menu State
// -----------------------------------------------------------------------------

export interface RadialMenuItem {
  action: BloodBowlAction;
  hotkey: string;
  label: string;
  angle: number;    // Angle in degrees (0-360)
  available: boolean;
}

export interface RadialMenuState {
  visible: boolean;
  position: FieldPosition;    // Position on field where menu appeared
  items: RadialMenuItem[];
  selectedPlayer: Player | null;
}

// -----------------------------------------------------------------------------
// API Types
// -----------------------------------------------------------------------------

export interface OAuthTokenResponse {
  access_token: string;
  token_type: string;
  expires_in: number;
}

export interface AuthTokenResponse {
  session_token: string;
  match_id?: string;
  team_id?: string;
}

export interface MatchInfo {
  matchId: string;
  team1: { id: string; name: string; score: number };
  team2: { id: string; name: string; score: number };
  turn: number;
  phase: string;
  startTime: number;
  timeRemaining: number;
}