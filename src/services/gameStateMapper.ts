// =============================================================================
// Game State Mapper
// Maps FUMBBL WebSocket/GameType data to our local GameState format
// =============================================================================

import {
  GameType as FumbblGameType,
  TeamType as FumbblTeamType,
  PlayerType as FumbblPlayerType,
  ReportType as FumbblReportType,
  FieldModelType as FumbblFieldModelType,
} from '../types/fumbblProtocol';

import {
  GameState,
  Player,
  PlayerPosition,
  PlayerStatus,
  WeatherType,
  FanAttendance,
  ReRolls,
  FieldState,
  DiceLogEntry,
  DiceLogType,
  SkillShorthand,
} from '../types/bloodbowl';

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

/**
 * Convert FUMBBL weather string to our WeatherType
 */
function mapWeather(weather: string): { type: WeatherType; icon: string; description: string } {
  const weatherMap: Record<string, { type: WeatherType; icon: string; description: string }> = {
    'clear': { type: 'clear', icon: '☀️', description: 'Clear' },
    'raining': { type: 'raining', icon: '🌧️', description: 'Raining' },
    'stormy': { type: 'stormy', icon: '⛈️', description: 'Stormy' },
    'foggy': { type: 'foggy', icon: '🌫️', description: 'Foggy' },
    'extreme': { type: 'extreme', icon: '❄️', description: 'Extreme' },
    'normal': { type: 'clear', icon: '☀️', description: 'Normal' },
  };

  return weatherMap[weather.toLowerCase()] || { type: 'clear', icon: '☀️', description: weather };
}

/**
 * Convert FUMBBL phase string to our phase type
 */
function mapPhase(phase: string): GameState['phase'] {
  const phaseMap: Record<string, GameState['phase']> = {
    'setup': 'setup',
    'first_turn': 'first_turn',
    'regular': 'regular',
    'halftime': 'halftime',
    'overtime': 'overtime',
    'ended': 'ended',
    'end': 'ended',
  };
  return phaseMap[phase.toLowerCase()] || 'regular';
}

/**
 * Convert FUMBBL player status to our PlayerStatus
 */
function mapPlayerStatus(status: string): PlayerStatus {
  const statusMap: Record<string, PlayerStatus> = {
    'active': 'active',
    'injured': 'injured',
    'dead': 'dead',
    'rotd': 'rotd',
    'doubtful': 'doubtful',
    'missing': 'missing',
    'unconscious': 'injured',
  };
  return statusMap[status.toLowerCase()] || 'active';
}

/**
 * Convert FUMBBL position to our PlayerPosition
 */
function mapPlayerPosition(positionName: string): PlayerPosition {
  const posMap: Record<string, PlayerPosition> = {
    'assassin': 'ap',
    'blocker': 'bl',
    'stalker': 'st',
    'running back': 'rb',
    'wide receiver': 'wr',
    'center': 'c',
    'left halfback': 'lh',
    'right halfback': 'rh',
    'goalie': 'g',
    'middle goalie': 'mg',
    'forward goalie': 'fg',
    'left halfback 2': 'lh2',
    'right halfback 2': 'rh2',
    'substitute': 'sub',
    'coach': 'coach',
  };
  return posMap[positionName.toLowerCase()] || 'c';
}

/**
 * Convert FUMBBL skill shorthand mapping
 */
function mapSkillShorthand(skillName: string): string {
  const skillMap: Record<string, string> = {
    'dodge': 'AG',
    'block': 'B',
    'mighty blows': 'ST',
    'catch': 'C',
    'anchor': 'AN',
    'no kicks left': 'NK',
    'sure hands': 'SV',
    'keeper': 'K',
    'tackle': 'T',
    'move': 'M',
    'secure ball': 'S',
    'blitz': 'L',
    'pass': 'P',
    'hand-off': 'H',
    'foul': 'F',
    'special': 'E',
    'frenzy': 'FB',
    'tough': 'TB',
    'stiff arm': 'ST',
    'nerves': 'NU',
    'athletic': 'ATH',
    'pick up': 'PU',
    'strong arm': 'SA',
    'clean catch': 'CC',
    'wicked claw': 'WC',
    'unfailing courage': 'UC',
    'roller ball': 'RB',
    'bloodlust': 'BL',
    'brawlers tenacity': 'BT',
    'dark rituals': 'DR',
    'guardians ward': 'GW',
    'hard contact': 'HC',
    'iron will': 'IW',
    'juggernaut constitution': 'JC',
    'lifeblood': 'LB',
    'magic resistance': 'MA',
    'never catches': 'NC',
    'on the ball': 'OB',
    'pile driver': 'PD',
    'rags to riches': 'RH',
    'sure hands dodge': 'SD',
    'sure hands kick': 'SK',
    'wound duration': 'WD',
    'vitality': 'VA',
    'unkillable': 'UK',
    'extra skeleton': 'ES',
    'secret weapon': 'SW',
    'siren limb': 'SL',
    'agent of chaos': 'AoC',
    'ambidextrous': 'AMB',
    'apo tropaion': 'APOT',
    'aura of never ending adoration': 'AoNEA',
    'big eye': 'BE',
    'brawler': 'BRAWL',
    'come again': 'CA',
    'come on you lads': 'COYL',
    'curved throws': 'CT',
    'daemonbone arm': 'DBA',
    'dauntless': 'DAUNT',
    'dead hand': 'DH',
    'diving catch': 'DC',
    'diving run': 'DRUN',
    'dumb fouls': 'DF',
    'finesse': 'FIN',
    'freak': 'FREAK',
    'fumble fortunes': 'FF',
    'gallant captain': 'GC',
    'grab the ball': 'GTB',
    'grudge': 'GRUDGE',
    'hairy': 'HAIRY',
    'hold ball': 'HB',
    'ivory tower': 'IT',
    'lacrosse hands': 'LH',
    'leader': 'LEAD',
    'let down': 'LD',
    'long throw': 'LT',
    'manhunter': 'MH',
    'matador': 'MAT',
    'mighty thews': 'MT',
    'outwit': 'OW',
    'peculiar': 'PEC',
    'pick off pass': 'POP',
    'poor learner': 'PL',
    'proud face': 'PF',
    'puck': 'PUCK',
    'quick saver': 'QS',
    'rock jaw': 'RJ',
    'running pass': 'RP',
    'scurry': 'SCURRY',
    'special scout': 'SSCOUT',
    'stinker': 'STINK',
    'unyielding': 'UNY',
    'wrangling': 'WRAN',
    'wicket': 'WICKET',
    'wonderlegs': 'WL',
  };
  return skillMap[skillName.toLowerCase()] || skillName.toUpperCase().replace(/\s+/g, '');
}

// -----------------------------------------------------------------------------
// Player Mapper
// -----------------------------------------------------------------------------

/**
 * Convert FUMBBL PlayerType to our Player type
 */
function mapPlayer(fumbblPlayer: FumbblPlayerType, _teamId: number): Player {
  // Determine position from positionName or infer from number
  const position = fumbblPlayer.positionName
    ? mapPlayerPosition(fumbblPlayer.positionName)
    : mapPlayerPosition(fumbblPlayer.number <= 11 ? 'c' : 'sub');

  return {
    id: fumbblPlayer.id,
    name: fumbblPlayer.name,
    number: fumbblPlayer.number,
    race: fumbblPlayer.race,
    position,
    status: mapPlayerStatus(fumbblPlayer.status),
    skills: fumbblPlayer.skills.map(s => mapSkillShorthand(s.name || s.shortName || '') as SkillShorthand),
    ma: fumbblPlayer.ma,
    st: fumbblPlayer.st,
    ag: fumbblPlayer.ag,
    pa: fumbblPlayer.pa,
    av: fumbblPlayer.av,
    hasBall: fumbblPlayer.hasBall,
    fieldX: fumbblPlayer.fieldX,
    fieldY: fumbblPlayer.fieldY,
  };
}

// -----------------------------------------------------------------------------
// Team Mapper
// -----------------------------------------------------------------------------

/**
 * Convert FUMBBL TeamType to our Team type
 * NOTE: Server may send teams without players array (players come separately via modelChangeList)
 */
function mapTeam(fumbblTeam: FumbblTeamType, isTeam1: boolean): { team: import('../types/bloodbowl').Team; players: Player[] } {
  // CRITICAL FIX: Server may not include players in team object during initial gameState
  // Players come separately via serverModelSync messages
  const players = (fumbblTeam.players || []).map(p => mapPlayer(p, fumbblTeam.id));

  const team: import('../types/bloodbowl').Team = {
    id: String(fumbblTeam.id),
    name: fumbblTeam.name || `Team ${isTeam1 ? 1 : 2}`,
    race: fumbblTeam.race || 'unknown',
    logoUrl: fumbblTeam.logo ? `https://fumbbl.com/api/team/${fumbblTeam.id}/logo` : undefined,
    players,
    color: fumbblTeam.color || (isTeam1 ? '#4a7c3f' : '#c4a35a'),
    secondaryColor: fumbblTeam.secondaryColor,
  };

  return { team, players };
}

// -----------------------------------------------------------------------------
// Field Mapper
// -----------------------------------------------------------------------------

/**
 * Convert FUMBBL FieldModelType to our FieldState
 */
function mapField(_field: FumbblFieldModelType): FieldState {
  const markers: FieldState['markers'] = [
    { id: 'center', type: 'center' },
    { id: '10yard-left', type: '10yard', position: { x: 4, y: 0 } },
    { id: '10yard-right', type: '10yard', position: { x: 12, y: 0 } },
    { id: 'endzone-left', type: 'endzone', position: { x: 0, y: 0 } },
    { id: 'endzone-right', type: 'endzone', position: { x: 16, y: 0 } },
  ];

  return {
    markers,
    ballPosition: { x: 8, y: 5 }, // Default center - will be overridden by game state
  };
}

// -----------------------------------------------------------------------------
// Report/DiceLog Mapper
// -----------------------------------------------------------------------------

/**
 * Convert FUMBBL ReportType to our DiceLogEntry
 */
function mapReport(report: FumbblReportType): DiceLogEntry {
  // Map report type to dice log type
  let diceLogType: DiceLogType;
  switch (report.type.toLowerCase()) {
    case 'block':
    case 'block_roll':
      diceLogType = 'block_roll';
      break;
    case 'armor':
    case 'armor_roll':
      diceLogType = 'armor';
      break;
    case 'injury':
      diceLogType = 'injury';
      break;
    case 'dodge':
      diceLogType = 'dodge';
      break;
    case 'catch':
      diceLogType = 'catch';
      break;
    case 'pass':
      diceLogType = 'pass';
      break;
    case 'touchdown':
      diceLogType = 'touchdown';
      break;
    case 'casualty':
      diceLogType = 'casualty';
      break;
    case 'fumble':
      diceLogType = 'action';
      break;
    case 'tackle':
      diceLogType = 'action';
      break;
    default:
      diceLogType = 'action';
  }

  // Determine color based on type
  let color: string | undefined;
  if (report.type.toLowerCase() === 'casualty' || report.type.toLowerCase() === 'injury') {
    color = 'text-red-500 font-bold';
  } else if (diceLogType === 'block_roll') {
    color = 'text-yellow-400';
  } else if (diceLogType === 'armor') {
    color = 'text-orange-400';
  }

  // Determine result
  let result: DiceLogEntry['result'];
  if (report.blockAttackerResult === 'success' || report.blockDefenderResult === 'success') {
    result = 'success';
  } else if (report.blockAttackerResult === 'failure' || report.blockDefenderResult === 'failure') {
    result = 'failure';
  }

  return {
    id: report.id,
    type: diceLogType,
    timestamp: report.turn ? report.turn * 1000 : Date.now(),
    text: report.description || report.message || report.type,
    color,
    dice: report.dice || report.blockAttackerRolls || report.blockDefenderRolls || report.armorRoll || report.injuryRoll,
    target: undefined,
    result,
    turn: report.turn,
  };
}

// -----------------------------------------------------------------------------
// Main Game State Mapper
// -----------------------------------------------------------------------------

/**
 * Map FUMBBL GameType to our local GameState
 * This is the main function that converts server data to UI format
 */
export function mapFumbblGameState(fumbblGame: FumbblGameType): Partial<GameState> {
  // Map teams
  const team1Data = mapTeam(fumbblGame.homeTeam, true);
  const team2Data = mapTeam(fumbblGame.awayTeam, false);

  // Determine which team's turn it is
  const isTeam1Turn = fumbblGame.turnTeam === fumbblGame.homeTeam.id;

  // Map weather
  const weather = mapWeather(fumbblGame.weather || 'clear');

  // Map fan attendance
  const fanAttendance: FanAttendance = {
    total: fumbblGame.fanAttendance || 0,
    dedicatedFans: {
      team1: fumbblGame.dedicatedFans?.team1 || 0,
      team2: fumbblGame.dedicatedFans?.team2 || 0,
    },
  };

  // Map re-rolls
  const reRolls: ReRolls = {
    team1: fumbblGame.rerolls?.team1 ?? fumbblGame.homeTeam.rerolls,
    team2: fumbblGame.rerolls?.team2 ?? fumbblGame.awayTeam.rerolls,
  };

  // Map field
  const fieldState = mapField(fumbblGame.field);

  // Map ball position from turn data
  const ballPosition = fumbblGame.turnData?.ballPosition || { x: 8, y: 5 };
  fieldState.ballPosition = ballPosition;

  // Map reports to dice log entries
  const diceLog: DiceLogEntry[] = (fumbblGame.reports || []).map(mapReport);

  // Determine if game is live
  const isLive = fumbblGame.status === 'live' || fumbblGame.status === 'playing';

  // Calculate time remaining from turn data or phase
  let timeRemaining = 0;
  if (fumbblGame.turnData?.phase === 'action') {
    timeRemaining = Math.max(0, 120 - (fumbblGame.turn * 10)); // Simplified timer
  }

  return {
    score: fumbblGame.score || { team1: 0, team2: 0 },
    turn: fumbblGame.turn,
    phase: mapPhase(fumbblGame.phase),
    reRolls,
    timer: timeRemaining,
    weather,
    fanAttendance,
    team1: team1Data.team,
    team2: team2Data.team,
    team1Players: team1Data.players,
    team2Players: team2Data.players,
    field: fieldState,
    ballPosition,
    selectedPlayer: null,
    selectedTeam: isTeam1Turn ? 'team1' : 'team2',
    diceLog,
    chatMessages: [],
    isLive,
    lastUpdate: Date.now(),
  };
}

/**
 * Map incremental model changes to GameState updates
 */
export function mapModelChanges(
  _currentState: GameState,
  _changes: import('../types/fumbblProtocol').ModelChangeType[]
): Partial<GameState> {
  // For now, return empty - full implementation would process each change type
  return {};
}

/**
 * Map reports to dice log entries (for incremental updates)
 */
export function mapReportsToDiceLog(reports: FumbblReportType[]): DiceLogEntry[] {
  return reports.map(mapReport);
}

// -----------------------------------------------------------------------------
// Type Re-exports
// -----------------------------------------------------------------------------

export type { FumbblGameType, FumbblTeamType, FumbblPlayerType, FumbblReportType };