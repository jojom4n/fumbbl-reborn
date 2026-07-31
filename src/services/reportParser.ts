// =============================================================================
// Report Parser — Converts FUMBBL server reports to DiceLogEntry
// Based on ffb-common Report*.java classes and IJsonOption.java
// =============================================================================

import { DiceLogEntry, DiceLogType } from '../types/bloodbowl';

let _diceLogIdCounter = 0;

function formatRoll(roll: number, minimumRoll: number, successful: boolean, reRolled: boolean = false): string {
  const result = successful ? '✓' : '✗';
  const reroll = reRolled ? ' [RR]' : '';
  return `${result} [${roll}] vs ${minimumRoll}${reroll}`;
}

function getStatName(r: any): string {
  const s = r.statBasedRollModifier;
  if (s && s.stat) return typeof s.stat === 'object' ? s.stat.name : s.stat;
  return '';
}
function getStatValue(r: any): number | undefined {
  const s = r.statBasedRollModifier;
  return s && s.value !== undefined ? s.value : undefined;
}

function formatModifiers(r: any): string {
  const m = r.rollModifiers || [];
  if (m.length === 0) return '';
  const names = m.map((x: any) => typeof x === 'object' && x.name ? x.name : String(x));
  return ` (${names.join(', ')})`;
}

function formatArr(r: any, key: string): string {
  const m = r[key] || [];
  if (m.length === 0) return '';
  const names = m.map((x: any) => typeof x === 'object' && x.name ? x.name : String(x));
  return ` [${names.join(', ')}]`;
}

const ce = (type: DiceLogType, text: string, dice?: number[], result?: 'success' | 'failure' | 'partial', color?: string, target?: number, turn?: number): DiceLogEntry => {
  _diceLogIdCounter++;
  return { id: _diceLogIdCounter, type, timestamp: Date.now(), text, dice, result, color, target, turn };
};

const psn: Record<string, string> = {
  STANDING: 'standing', PRONE: 'prone', STUNNED: 'stunned', KNOCKED_OUT: 'knocked out',
  BADLY_HURT: 'badly hurt', SERIOUS_INJURY: 'serious injury', RIP: 'RIP', BANNED: 'banned',
};

function enumName(v: any): string {
  if (!v) return '';
  return typeof v === 'object' ? (v.name || '') : String(v);
}

// -----------------------------------------------------------------------------
// Parse a single report → DiceLogEntry[]
// -----------------------------------------------------------------------------
export function parseReportToDiceLogs(report: any, turn: number = 0): DiceLogEntry[] {
  const e: DiceLogEntry[] = [];
  const id = report.reportId || '';
  const pid = report.playerId || '';
  const did = report.defenderId || '';
  const aid = report.attackerId || '';
  const tid = report.choosingTeamId || '';
  const roll = report.roll;
  const minr = report.minimumRoll;
  const ok = report.successful;
  const rr = report.reRolled;
  const sn = getStatName(report);
  const sv = getStatValue(report);
  const sl = sv !== undefined ? `${sn}: ${sv}` : '';
  const fm = formatModifiers(report);
  const lbl = sl ? `${sl}${fm}` : fm;

  const sr = (name: string, type: DiceLogType, colorOk?: string, colorFail?: string) => {
    const c = ok ? (colorOk || 'text-green-300') : (colorFail || 'text-red-300');
    const t = lbl ? `${lbl}` : '';
    e.push(ce(type, `${pid} ${name} ${formatRoll(roll, minr, ok, rr)}${t ? ` ${t}` : ''}`, [roll], ok ? 'success' : 'failure', c, minr, turn));
  };

  switch (id) {
    // Skill rolls (single die)
    case 'dodgeRoll': sr('Dodge', 'dodge'); break;
    case 'catchRoll': sr('Catch', 'catch'); break;
    case 'escapeRoll': sr('Escape', 'dodge'); break;
    case 'leapRoll': sr('Leap', 'dodge'); break;
    case 'goForItRoll': sr('Go For It', 'catch'); break;
    case 'pickUpRoll': sr('Pick Up', 'catch'); break;
    case 'interceptionRoll': sr('Intercept', 'catch'); break;
    case 'standUpRoll': sr('Stand Up', 'action'); break;
    case 'jumpUpRoll': sr('Jump Up', 'dodge'); break;
    case 'regenerationRoll': sr('Regeneration', 'action'); break;
    case 'rightStuffRoll': sr('Right Stuff', 'block_roll', 'text-green-300', 'text-yellow-300'); break;
    case 'bloodLustRoll': sr('Blood Lust', 'action'); break;
    case 'dauntlessRoll': sr('Dauntless', 'dodge'); break;
    case 'solidDefenceRoll': sr('Solid Defence', 'block_roll', 'text-green-300', 'text-yellow-300'); break;
    case 'quickSnapRoll': sr('Quick Snap', 'action'); break;
    case 'foulAppearanceRoll': sr('Foul Appearance', 'action'); break;
    case 'hypnoticGazeRoll': sr('Hypnotic Gaze', 'dodge'); break;
    case 'lookIntoMyEyesRoll': sr('Look Into My Eyes', 'dodge'); break;
    case 'steadyFootingRoll': sr('Steady Footing', 'dodge'); break;
    case 'dwarfenWisdomRoll': sr('Dwarfen Wisdom', 'action', 'text-cyan-300'); break;
    case 'gettingEvenRoll': sr('Getting Even', 'action', 'text-cyan-300'); break;
    case 'saboteurRoll': sr('Saboteur', 'action', 'text-orange-400'); break;
    case 'chompRoll': sr('Chomp', 'action', 'text-orange-400'); break;
    case 'dodgySnackRoll': sr('Dodgy Snack', 'action', 'text-orange-400'); break;
    case 'weatherMageRoll': sr('Weather Mage', 'weather', 'text-cyan-300'); break;
    case 'apothecaryRoll': sr('Apothecary', 'action'); break;

    // Blitz roll (has stat info)
    case 'blitzRoll': {
      const c = 'text-blue-300';
      e.push(ce('action', `${pid} Blitz ${formatRoll(roll, minr, ok, rr)}${lbl ? ` ${lbl}` : ''}`, [roll], ok ? 'success' : 'failure', c, minr, turn));
      break;
    }

    // Pass roll (extra fields)
    case 'passRoll': {
      const pr = report.passResult;
      const rt = pr ? ` (${enumName(pr)})` : '';
      const bt = report.bomb ? ' [BOMB]' : '';
      const ht = report.hailMaryPass ? ' [Hail Mary]' : '';
      e.push(ce('pass', `${pid} Pass ${formatRoll(roll, minr, ok, rr)}${lbl ? ` ${lbl}` : ''}${rt}${bt}${ht}`, [roll], ok ? 'success' : 'failure', ok ? 'text-green-300' : 'text-red-300', minr, turn));
      break;
    }

    // Throw Team Mate
    case 'throwTeamMateRoll': {
      e.push(ce('pass', `${pid} Throw Team Mate ${formatRoll(roll, minr, ok, rr)}${lbl ? ` ${lbl}` : ''}`, [roll], ok ? 'success' : 'failure', ok ? 'text-green-300' : 'text-red-300', minr, turn));
      break;
    }

    // Confusion (no playerId, generic)
    case 'confusionRoll': {
      e.push(ce('catch', `Confusion ${formatRoll(roll, minr, ok, rr)}${lbl ? ` ${lbl}` : ''}`, [roll], ok ? 'success' : 'failure', ok ? 'text-green-300' : 'text-red-300', minr, turn));
      break;
    }

    // Prayer roll
    case 'prayerRoll': {
      const pn = enumName(report.prayer);
      e.push(ce('action', `${tid} Prayer: ${pn || '?'} ${formatRoll(roll, minr, ok, rr)}`, [roll], ok ? 'success' : 'failure', ok ? 'text-green-300' : 'text-red-300', minr, turn));
      break;
    }

    // Team Captain
    case 'teamCaptainRoll': {
      e.push(ce('action', `${tid} Team Captain ${formatRoll(roll, minr, ok, rr)}`, [roll], ok ? 'success' : 'failure', 'text-cyan-300', minr, turn));
      break;
    }

    // Block roll (dice array)
    case 'blockRoll': {
      const d = report.blockRoll || [];
      e.push(ce('block_roll', `${tid} chooses defender${did ? ` → ${did}` : ''}`, d.length > 0 ? d : undefined, 'partial', 'text-yellow-300', undefined, turn));
      break;
    }

    // Block start
    case 'block': {
      e.push(ce('block_roll', `Block: ${aid} vs ${did}`, undefined, undefined, 'text-yellow-300', undefined, turn));
      break;
    }

    // Injury
    case 'injury': {
      const iType = enumName(report.injuryType);
      const ar = report.armorRoll || [];
      const ir = report.injuryRoll || [];
      const cr = report.casualtyRoll || [];
      const am = formatArr(report, 'armorModifiers');
      const im = formatArr(report, 'injuryModifiers');
      const ab = report.armorBroken;
      const ist = report.injury;
      const isn = psn[ist] || ist || 'unknown';
      const si = enumName(report.seriousInjury);
      if (ar.length > 0) e.push(ce('armor', `${did} Armor${ab ? ' BROKEN!' : ' holds'}${am ? ` ${am}` : ''}`, ar, ab ? 'failure' : 'success', ab ? 'text-orange-300' : 'text-gray-300', undefined, turn));
      if (ir.length > 0) e.push(ce('injury', `${did} Injury${im ? ` ${im}` : ''}${iType ? ` (${iType})` : ''}`, ir, 'partial', 'text-yellow-300', undefined, turn));
      if (cr.length > 0) e.push(ce('casualty', `${did} is ${isn}!${si ? ` (${si})` : ''}`, cr, 'failure', 'text-red-400', undefined, turn));
      if (ar.length === 0 && ir.length === 0 && cr.length === 0) e.push(ce('casualty', `${did} is ${isn}!${si ? ` (${si})` : ''}`, undefined, 'failure', 'text-red-400', undefined, turn));
      break;
    }

    // Re-Roll
    case 'reRoll': e.push(ce('action', `${pid} uses a Re-Roll`, undefined, 'partial', 'text-blue-300', undefined, turn)); break;

    // Player Action
    case 'playerAction': {
      const an = enumName(report.playerAction);
      e.push(ce('action', `${pid} ${an || 'acted'}`, undefined, undefined, 'text-gray-300', undefined, turn));
      break;
    }

    // Skill Use
    case 'skillUse': {
      const sn2 = enumName(report.skill);
      const su = enumName(report.skillUse);
      e.push(ce('action', `${pid} uses ${sn2 || 'skill'}${su ? ` (${su})` : ''}`, undefined, 'partial', 'text-cyan-300', undefined, turn));
      break;
    }

    // Turn/Half
    case 'turnEnd': e.push(ce('system', '--- Turn ended ---', undefined, undefined, 'text-gray-500', undefined, turn)); break;
    case 'startHalf': e.push(ce('system', `--- Half ${report.half} starts ---`, undefined, undefined, 'text-gray-400 font-bold', undefined, turn)); break;

    // Throw-In / Hand-Over
    case 'throwIn': e.push(ce('system', `${tid} Throw-In`, undefined, undefined, 'text-gray-400', undefined, turn)); break;
    case 'handOver': e.push(ce('system', `${tid} Hand-Over`, undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Scatter Ball
    case 'scatterBall': {
      const c = report.coordinate;
      e.push(ce('system', `Ball scattered to [${c?.x ?? '?'}, ${c?.y ?? '?'}]`, undefined, undefined, 'text-gray-400', undefined, turn));
      break;
    }

    // Scatter Player
    case 'scatterPlayer': {
      const c = report.coordinate;
      e.push(ce('action', `${pid} scattered to [${c?.x ?? '?'}, ${c?.y ?? '?'}]`, undefined, undefined, 'text-gray-300', undefined, turn));
      break;
    }

    // Foul
    case 'foul': e.push(ce('system', `${pid} commits a Foul!${report.foulUsed ? ' (Foul used)' : ''}`, undefined, 'failure', 'text-orange-400', undefined, turn)); break;

    // Coin Toss
    case 'coinThrow': e.push(ce('system', `Coin Toss: ${report.coinThrowHeads ? 'Heads' : 'Tails'}`, undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Kickoff
    case 'kickoffResult': e.push(ce('system', `Kickoff: ${enumName(report.kickoffResult) || 'Result'}`, undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Weather
    case 'weather': e.push(ce('weather', `Weather: ${enumName(report.weather) || 'Unknown'}`, undefined, undefined, 'text-cyan-300', undefined, turn)); break;
    case 'weatherMageResult': e.push(ce('weather', `Weather changed to: ${enumName(report.weather) || 'Unknown'}`, undefined, undefined, 'text-cyan-300', undefined, turn)); break;

    // Spectators / Fans
    case 'spectators': e.push(ce('fan', `Spectators: ${report.spectators}`, undefined, undefined, 'text-purple-300', undefined, turn)); break;
    case 'fanFactor': e.push(ce('fan', `Fan Factor: ${report.fanFactor}`, undefined, undefined, 'text-purple-300', undefined, turn)); break;
    case 'dedicatedFans': e.push(ce('fan', `Dedicated Fans: rolled ${report.dedicatedFansRoll} → ${report.dedicatedFansResult}`, report.dedicatedFansRoll !== undefined ? [report.dedicatedFansRoll] : undefined, undefined, 'text-purple-300', undefined, turn)); break;
    case 'cheeringFans': e.push(ce('fan', 'Cheering Fans activated', undefined, 'partial', 'text-purple-300', undefined, turn)); break;

    // Pushback
    case 'pushback': e.push(ce('action', `${pid} Pushback`, undefined, undefined, 'text-gray-300', undefined, turn)); break;

    // Piling On
    case 'pilingOn': {
      const d = report.rolls || [];
      e.push(ce('block_roll', `${pid} Piling On${ok !== undefined ? ` ${formatRoll(d[0] || 0, minr || 0, ok, rr)}` : ''}`, d.length > 0 ? d : undefined, ok ? 'success' : 'failure', 'text-yellow-300', undefined, turn));
      break;
    }

    // Leader
    case 'leader': e.push(ce('action', `${pid} uses Leader`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Swoop
    case 'swoopPlayer': e.push(ce('action', `${pid} Swoops!`, undefined, 'partial', 'text-orange-300', undefined, turn)); break;
    case 'swoopDirectionRoll': e.push(ce('action', `${pid} Swoop Direction: ${enumName(report.direction) || '?'}`, undefined, undefined, 'text-orange-300', undefined, turn)); break;

    // Timeout
    case 'timeoutEnforced': e.push(ce('system', `Timeout enforced for ${tid}!`, undefined, 'failure', 'text-red-400', undefined, turn)); break;

    // Game end
    case 'winnings': e.push(ce('system', '=== Game Over ===', undefined, undefined, 'text-gray-400 font-bold', undefined, turn)); break;
    case 'mostValuablePlayers': e.push(ce('system', 'Most Valuable Players selected', undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Receive choice
    case 'receiveChoice': e.push(ce('system', `${tid} chooses to ${report.receiveChoice ? 'receive' : 'kick'}`, undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Apothecary choice
    case 'apothecaryChoice': e.push(ce('action', `${tid} Apothecary choice for ${pid || did || 'player'}`, undefined, 'partial', 'text-blue-300', undefined, turn)); break;

    // Bribes
    case 'bribesRoll': {
      const d = report.rolls || [];
      e.push(ce('action', `Bribes ${formatRoll(d[0] || 0, minr || 0, ok, rr)}`, d.length > 0 ? d : undefined, ok ? 'success' : 'failure', 'text-yellow-300', minr, turn));
      break;
    }

    // Referee
    case 'referee': e.push(ce('system', 'Referee decision', undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Play card
    case 'playCard': e.push(ce('action', `${tid} plays card: ${enumName(report.card) || '?'}`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Inducement
    case 'inducement': e.push(ce('action', `${tid} uses ${enumName(report.inducementType) || '?'}`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Trap Door
    case 'trapDoor': e.push(ce('action', `${pid} fell through a Trap Door!`, undefined, 'failure', 'text-orange-400', undefined, turn)); break;

    // Animal Savagery
    case 'animalSavagery': e.push(ce('action', `${pid} goes wild (Animal Savagery)!`, undefined, 'failure', 'text-orange-400', undefined, turn)); break;

    // Punt
    case 'puntDirectionRoll': e.push(ce('action', `Punt Direction: ${enumName(report.direction) || '?'}${report.directionRoll !== undefined ? ` [${report.directionRoll}]` : ''}`, report.directionRoll !== undefined ? [report.directionRoll] : undefined, undefined, 'text-gray-300', undefined, turn)); break;
    case 'puntDistanceRoll': {
      const d = report.distanceRoll || [];
      e.push(ce('action', `Punt Distance: ${report.distance || '?'}${d.length > 0 ? ` [${d.join(', ')}]` : ''}`, d.length > 0 ? d : undefined, undefined, 'text-gray-300', undefined, turn));
      break;
    }

    // Swarming
    case 'swarmingPlayersRoll': e.push(ce('action', `Swarming: ${report.swarmingPlayerActual || '?'} player(s)${report.swarmingPlayerRoll !== undefined ? ` [${report.swarmingPlayerRoll}]` : ''}`, report.swarmingPlayerRoll !== undefined ? [report.swarmingPlayerRoll] : undefined, undefined, 'text-gray-300', undefined, turn)); break;

    // Chomp removed
    case 'chompRemoved': e.push(ce('action', `${pid} Chomp removed`, undefined, undefined, 'text-gray-300', undefined, turn)); break;

    // Then I Started Blastin
    case 'thenIStartedBlastin': e.push(ce('action', `${pid} Then I Started Blastin'!`, undefined, 'failure', 'text-red-400', undefined, turn)); break;

    // Breathe Fire
    case 'breatheFire': e.push(ce('action', `${pid} Breathe Fire!`, undefined, 'failure', 'text-orange-400', undefined, turn)); break;

    // Catch of the Day
    case 'catchOfTheDay': e.push(ce('action', `${pid} Catch of the Day!`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Raiding Party
    case 'raidingParty': e.push(ce('action', `${pid} Raiding Party!`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Hit and Run
    case 'hitAndRun': e.push(ce('action', `${pid} Hit and Run!`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Indomitable
    case 'indomitable': e.push(ce('action', `${pid} Indomitable!`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Old Pro
    case 'oldPro': e.push(ce('action', `${pid} Old Pro!`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Pick Me Up
    case 'pickMeUp': e.push(ce('action', `${pid} Pick Me Up!`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Fumblerooskie
    case 'fumblerooskie': e.push(ce('action', `${pid} Fumblerooskie!`, undefined, 'partial', 'text-orange-400', undefined, turn)); break;

    // Cloud Burster
    case 'cloudBurster': e.push(ce('action', `${pid} Cloud Burster!`, undefined, 'failure', 'text-red-400', undefined, turn)); break;

    // Baleful Hex
    case 'balefulHex': e.push(ce('action', `${pid} Baleful Hex!`, undefined, 'failure', 'text-red-400', undefined, turn)); break;

    // All You Can Eat
    case 'allYouCanEat': e.push(ce('action', `${pid} All You Can Eat!`, undefined, 'partial', 'text-orange-400', undefined, turn)); break;

    // Biased Ref
    case 'biasedRef': e.push(ce('action', `Biased Referee!`, undefined, 'partial', 'text-yellow-300', undefined, turn)); break;

    // Staller detected
    case 'stallerDetected': e.push(ce('system', `${pid} is stalling!`, undefined, 'failure', 'text-red-400', undefined, turn)); break;

    // Throw at stalling player
    case 'throwAtStallingPlayer': e.push(ce('action', `${pid} thrown at stalling player`, undefined, 'failure', 'text-orange-400', undefined, turn)); break;
    case 'throwAtPlayer': e.push(ce('action', `${pid} thrown at player`, undefined, 'failure', 'text-orange-400', undefined, turn)); break;

    // Thrown Keg
    case 'thrownKeg': e.push(ce('action', `${pid} Thrown Keg!`, undefined, 'partial', 'text-orange-400', undefined, turn)); break;

    // Brilliant Coaching rerolls lost
    case 'brilliantCoachingReRoll': e.push(ce('system', `Brilliant Coaching re-rolls lost`, undefined, 'failure', 'text-yellow-300', undefined, turn)); break;
    case 'pumpUpTheCrowdReRollLost': e.push(ce('system', `Pump Up The Crowd re-rolls lost`, undefined, 'failure', 'text-yellow-300', undefined, turn)); break;
    case 'showStarReRollLost': e.push(ce('system', `Show Star re-rolls lost`, undefined, 'failure', 'text-yellow-300', undefined, turn)); break;
    case 'pumpUpTheCrowdReRoll': e.push(ce('action', `${tid} Pump Up The Crowd Re-Roll`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;
    case 'showStarReRoll': e.push(ce('action', `${tid} Show Star Re-Roll`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Block Re-Roll
    case 'blockReRoll': e.push(ce('action', `${tid} Block Re-Roll`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Bribery and Corruption Re-Roll
    case 'briberyAndCorruptionReRoll': e.push(ce('action', `${tid} Bribery and Corruption Re-Roll`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Select blitz/gaze target
    case 'selectBlitzTarget': e.push(ce('action', `${pid} selects blitz target`, undefined, undefined, 'text-gray-300', undefined, turn)); break;
    case 'selectGazeTarget': e.push(ce('action', `${pid} selects gaze target`, undefined, undefined, 'text-gray-300', undefined, turn)); break;

    // Bomb explodes after catch
    case 'bombExplodesAfterCatch': e.push(ce('action', 'Bomb explodes after catch!', undefined, 'failure', 'text-red-400', undefined, turn)); break;

    // Place ball direction
    case 'placedBallDirection': e.push(ce('action', `${pid} places ball`, undefined, undefined, 'text-gray-300', undefined, turn)); break;

    // Skill wasted
    case 'skillWasted': e.push(ce('action', `${pid} skill wasted`, undefined, 'failure', 'text-yellow-300', undefined, turn)); break;

    // Two for One
    case 'twoForOne': e.push(ce('action', `${pid} Two for One!`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Modified pass/dodge result
    case 'modifiedPassResult': e.push(ce('pass', `${pid} pass result modified`, undefined, 'partial', 'text-yellow-300', undefined, turn)); break;
    case 'modifiedDodgeResultSuccessful': e.push(ce('dodge', `${pid} dodge result modified → success`, undefined, 'success', 'text-green-300', undefined, turn)); break;

    // Mascot used
    case 'mascotUsed': e.push(ce('action', `${tid} Mascot used`, undefined, 'partial', 'text-cyan-300', undefined, turn)); break;

    // Team event
    case 'teamEvent': e.push(ce('system', `${tid} team event`, undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Player event
    case 'playerEvent': e.push(ce('action', `${pid} player event`, undefined, undefined, 'text-gray-300', undefined, turn)); break;

    // Event (generic)
    case 'event': e.push(ce('system', 'Event triggered', undefined, undefined, 'text-gray-400', undefined, turn)); break;

    // Default: log unknown report type
    default: {
      e.push(ce('system', `[${id}] ${JSON.stringify(report).substring(0, 120)}`, undefined, undefined, 'text-gray-600', undefined, turn));
      break;
    }
  }

  return e;
}

// -----------------------------------------------------------------------------
// Parse multiple reports → flat DiceLogEntry[]
// -----------------------------------------------------------------------------
export function parseReportsToDiceLogs(reports: any[], turn: number = 0): DiceLogEntry[] {
  const entries: DiceLogEntry[] = [];
  for (const report of reports) {
    entries.push(...parseReportToDiceLogs(report, turn));
  }
  return entries;
}