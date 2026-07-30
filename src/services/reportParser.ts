// =============================================================================
// Report Parser — Converts FUMBBL server reports to DiceLogEntry
// Based on ffbclient ReportMessageBase.java and ReportId.java
// =============================================================================

import { DiceLogEntry } from '../types/bloodbowl';

// -----------------------------------------------------------------------------
// ID counter for dice log entries
// -----------------------------------------------------------------------------
let _diceLogIdCounter = 0;

// -----------------------------------------------------------------------------
// Report Parser
// -----------------------------------------------------------------------------

/**
 * Parse a FUMBBL server report and convert it to one or more DiceLogEntry objects.
 * Reports contain dice roll results, injury reports, pass results, etc.
 * 
 * The server sends reports in the reportList.reports array within serverModelSync messages.
 * Each report has an id (ReportId) and various fields depending on the report type.
 */
export function parseReportToDiceLogs(report: any, turn: number = 0): DiceLogEntry[] {
  const entries: DiceLogEntry[] = [];
  const reportId = report.reportId || '';
  const timestamp = Date.now();

  // Extract common fields
  const dice = report.dice || report.rolls || null;
  const success = report.success || report.result === 'SUCCESS';
  const failure = report.failure || report.result === 'FAILURE';

  // Helper to create an entry
  const createEntry = (
    type: DiceLogEntry['type'],
    text: string,
    dice?: number[],
    result?: 'success' | 'failure' | 'partial',
    color?: string,
    target?: number
  ) => {
    _diceLogIdCounter++;
    return {
      id: _diceLogIdCounter,
      type,
      timestamp,
      text,
      dice,
      result,
      color,
      target,
      turn,
    };
  };

  // Parse based on report type
  switch (reportId) {
    // --- Roll Reports ---
    case 'blockRoll':
    case 'block': {
      const attacker = report.playerNames?.[0] || report.attackerName || '';
      const defender = report.playerNames?.[1] || report.defenderName || '';
      const attackerRolls = report.attackerRolls || report.attackerDice || [];
      const defenderRolls = report.defenderRolls || report.defenderDice || [];
      const attackerSuccess = report.attackerResult === 'SUCCESS';
      const defenderSuccess = report.defenderResult === 'SUCCESS';
      const armorRolls = report.armorRolls || [];
      const armorBreak = report.armorBreak || false;

      if (attackerRolls.length > 0) {
        entries.push(createEntry(
          'block_roll',
          `${attacker} blocks ${defender}`,
          attackerRolls,
          attackerSuccess ? 'success' : 'failure',
          attackerSuccess ? 'text-yellow-300' : 'text-gray-300',
          5
        ));
      }

      if (attackerSuccess && defenderRolls.length > 0) {
        entries.push(createEntry(
          'armor',
          `${defender} armor roll`,
          defenderRolls,
          defenderSuccess ? 'success' : 'failure',
          'text-yellow-300',
          5
        ));
      }

      if (armorRolls.length > 0) {
        entries.push(createEntry(
          'armor',
          `${defender} armor ${armorBreak ? 'broken!' : 'holds'}`,
          armorRolls,
          armorBreak ? 'failure' : 'success',
          armorBreak ? 'text-orange-300' : 'text-gray-300'
        ));
      }

      // If both failed, check for injury
      if (attackerSuccess && !defenderSuccess && report.injuryRolls) {
        entries.push(createEntry(
          'injury',
          `${defender} injury roll`,
          report.injuryRolls,
          report.injuryResult === 'FAILURE' ? 'failure' : 'success',
          report.injuryResult === 'FAILURE' ? 'text-red-400' : 'text-gray-300'
        ));
      }

      if (report.casualty && report.playerNames?.[1]) {
        const injuryName = report.injuryName || report.casualtyType || 'injured';
        entries.push(createEntry(
          'casualty',
          `${report.playerNames[1]} is ${injuryName}!`,
          undefined,
          'failure',
          'text-red-400'
        ));
      }

      break;
    }

    case 'dodgeRoll':
    case 'dodge': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'dodge',
        `${name} dodge roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300',
        5
      ));
      break;
    }

    case 'catchRoll':
    case 'catch': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'catch',
        `${name} catch roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'passRoll':
    case 'pass': {
      const passer = report.playerNames?.[0] || report.passOriginName || 'Player';
      const target = report.playerNames?.[1] || report.passTargetName || '';
      const passType = report.passType || 'pass';
      const result = success ? 'successful' : 'failed';
      entries.push(createEntry(
        'pass',
        `${passer} ${passType} ${target} - ${result}`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'escapeRoll':
    case 'leapRoll':
    case 'jumpRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'dodge',
        `${name} escape roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'pickUpRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'catch',
        `${name} pick up roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'interceptionRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'catch',
        `${name} interception roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'goForItRoll': {
      entries.push(createEntry(
        'catch',
        `Go for it roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'blitzRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} blitz`,
        dice,
        success ? 'success' : 'failure',
        'text-blue-300'
      ));
      break;
    }

    case 'tackleRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'block_roll',
        `${name} tackle roll`,
        dice,
        success ? 'success' : 'failure',
        'text-yellow-300'
      ));
      break;
    }

    // --- Injury Reports ---
    case 'injury': {
      const name = report.playerNames?.[0] || report.playerName || 'Player';
      const injuryName = report.injuryName || report.name || 'injured';
      entries.push(createEntry(
        'casualty',
        `${name} is ${injuryName}!`,
        report.injuryRolls || dice,
        'failure',
        'text-red-400'
      ));
      break;
    }

    case 'apothecaryRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} apothecary roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    // --- Reroll Reports ---
    case 'reRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} uses a re-roll`,
        undefined,
        'partial',
        'text-blue-300'
      ));
      break;
    }

    // --- Game State Reports ---
    case 'turnEnd': {
      const turnNum = report.turn || turn;
      entries.push(createEntry(
        'system',
        `--- Turn ${turnNum} ended ---`,
        undefined,
        undefined,
        'text-gray-500'
      ));
      break;
    }

    case 'startHalf': {
      const half = report.half || 1;
      entries.push(createEntry(
        'system',
        `--- Half ${half} starts ---`,
        undefined,
        undefined,
        'text-gray-400 font-bold'
      ));
      break;
    }

    case 'throwIn': {
      const team = report.teamName || report.playerNames?.[0] || 'Team';
      entries.push(createEntry(
        'system',
        `${team} throw-in`,
        undefined,
        undefined,
        'text-gray-400'
      ));
      break;
    }

    case 'handOver': {
      const team = report.teamName || report.playerNames?.[0] || 'Team';
      entries.push(createEntry(
        'system',
        `${team} hand-over`,
        undefined,
        undefined,
        'text-gray-400'
      ));
      break;
    }

    case 'scatterBall': {
      const x = report.coordinate?.x ?? report.x ?? '?';
      const y = report.coordinate?.y ?? report.y ?? '?';
      entries.push(createEntry(
        'system',
        `Ball scattered to [${x}, ${y}]`,
        undefined,
        undefined,
        'text-gray-400'
      ));
      break;
    }

    case 'foul': {
      const fouler = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'system',
        `${fouler} commits a foul!`,
        undefined,
        'failure',
        'text-orange-400'
      ));
      break;
    }

    case 'coinThrow': {
      const winner = report.winnerName || report.playerNames?.[0] || 'Team';
      entries.push(createEntry(
        'system',
        `${winner} wins the coin toss!`,
        undefined,
        undefined,
        'text-gray-400'
      ));
      break;
    }

    case 'kickoffResult': {
      const result = report.result || report.description || 'Kickoff';
      entries.push(createEntry(
        'system',
        result,
        undefined,
        undefined,
        'text-gray-400'
      ));
      break;
    }

    case 'weather': {
      const weather = report.weatherType || report.name || 'Unknown';
      entries.push(createEntry(
        'weather',
        `Weather: ${weather}`,
        undefined,
        undefined,
        'text-cyan-300'
      ));
      break;
    }

    case 'spectators':
    case 'fanFactor': {
      const value = report.value || report.fanFactor || '?';
      entries.push(createEntry(
        'fan',
        `Fan Attendance: ${value}`,
        dice,
        undefined,
        'text-purple-300'
      ));
      break;
    }

    case 'playerAction': {
      const name = report.playerNames?.[0] || 'Player';
      const action = report.action || report.description || 'acted';
      entries.push(createEntry(
        'action',
        `${name} ${action}`,
        dice,
        undefined,
        'text-gray-300'
      ));
      break;
    }

    case 'skillUse': {
      const name = report.playerNames?.[0] || 'Player';
      const skill = report.skillName || report.skill || 'skill';
      entries.push(createEntry(
        'action',
        `${name} uses ${skill}`,
        undefined,
        'partial',
        'text-cyan-300'
      ));
      break;
    }

    case 'pushback': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} pushed back`,
        undefined,
        undefined,
        'text-gray-300'
      ));
      break;
    }

    case 'pilingOn': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'block_roll',
        `${name} piling on!`,
        dice,
        success ? 'success' : 'failure',
        'text-yellow-300'
      ));
      break;
    }

    case 'leader': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} uses Leader`,
        undefined,
        'partial',
        'text-cyan-300'
      ));
      break;
    }

    case 'standUpRoll':
    case 'jumpUpRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} stands up`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'regenerationRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} regeneration roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'bloodLustRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} blood lust roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-red-400'
      ));
      break;
    }

    case 'dauntlessRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'dodge',
        `${name} dauntless roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'rightStuffRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'block_roll',
        `${name} right stuff roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'solidDefenceRoll': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'block_roll',
        `${name} solid defence roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'quickSnapRoll': {
      entries.push(createEntry(
        'action',
        `Quick snap roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'confusionRoll': {
      entries.push(createEntry(
        'catch',
        `Confusion roll`,
        dice,
        success ? 'success' : 'failure',
        success ? 'text-green-300' : 'text-gray-300'
      ));
      break;
    }

    case 'scatteredPlayer':
    case 'scatterPlayer': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} scattered`,
        undefined,
        undefined,
        'text-gray-300'
      ));
      break;
    }

    case 'swoopPlayer': {
      const name = report.playerNames?.[0] || 'Player';
      entries.push(createEntry(
        'action',
        `${name} swoops!`,
        undefined,
        'partial',
        'text-orange-300'
      ));
      break;
    }

    case 'timeoutEnforced': {
      const team = report.teamName || 'Team';
      entries.push(createEntry(
        'system',
        `${team} timeout enforced!`,
        undefined,
        'failure',
        'text-red-400'
      ));
      break;
    }

    case 'winnings': {
      entries.push(createEntry(
        'system',
        `Game over!`,
        undefined,
        undefined,
        'text-gray-400 font-bold'
      ));
      break;
    }

    case 'mostValuablePlayers': {
      const team = report.teamName || 'Team';
      entries.push(createEntry(
        'system',
        `${team} Most Valuable Players selected`,
        undefined,
        undefined,
        'text-gray-400'
      ));
      break;
    }

    // --- Default: Try to create a generic entry from the report ---
    default: {
      // If the report has a description, use it
      const desc = report.description || report.message || `${reportId} report`;
      const name = report.playerNames?.[0] || '';
      const text = name ? `${name}: ${desc}` : desc;

      entries.push(createEntry(
        'action',
        text,
        dice,
        success ? 'success' : (failure ? 'failure' : undefined),
        undefined
      ));
      break;
    }
  }

  return entries;
}

/**
 * Parse multiple FUMBBL reports into DiceLogEntry array.
 */
export function parseReportsToDiceLogs(reports: any[], turn: number = 0): DiceLogEntry[] {
  const entries: DiceLogEntry[] = [];
  for (const report of reports) {
    const parsed = parseReportToDiceLogs(report, turn);
    entries.push(...parsed);
  }
  return entries;
}