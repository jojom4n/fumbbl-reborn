// =============================================================================
// gen_vectors.js — generates reference LZString UTF16 test vectors.
//
// Uses the EXACT same lz-string library the TS client uses
// (node_modules/lz-string) so the C++ port is verified byte-for-byte.
//
// Usage: node tests/gen_vectors.js   (from qt-client/, with node_modules
//        resolvable — run from the repo root or set NODE_PATH)
// Output: tests/lzstring_vectors.json
// =============================================================================
"use strict";

const path = require("path");
const fs = require("fs");

const LZString = require(path.join(__dirname, "..", "..", "node_modules", "lz-string"));

function utf16leToBase64(s) {
    return Buffer.from(s, "utf16le").toString("base64");
}

// Realistic FUMBBL-style game state (serverGameState.game shape).
function makeGameState() {
    const players = [];
    for (let i = 0; i < 22; ++i) {
        players.push({
            playerId: "1810969" + i,
            playerName: "Player " + i,
            playerNr: i + 1,
            playerType: i < 11 ? "home" : "away",
            positionName: ["Blocker", "Blitzer", "Stalker", "Running Back", "Wide Receiver"][i % 5],
            race: "Orcs",
            agility: 2 + (i % 4),
            strength: 2 + ((i + 1) % 4),
            movement: 1 + (i % 3),
            armour: i % 3,
            skillArray: ["Block", "Dodge", "Nimble", "Loner", "Hacksilver"],
            playerCoordinate: [3 + (i % 20), 2 + (i % 11)],
            playerState: [1, 1, 1, 3, 1, 9][i % 6],
        });
    }
    return {
        netCommandId: "serverGameState",
        game: {
            teamHome: {
                teamId: "9931",
                teamName: "Orcs United",
                coach: "saruman",
                race: "Orcs",
                reRolls: 2,
                fans: 12,
                dedicatedFans: 5,
                playerArray: players.slice(0, 11),
                roster: { rosterId: "1", rosterName: "Orcs", apothecary: false },
                score: 0,
            },
            teamAway: {
                teamId: "9932",
                teamName: "Dwarves FC",
                coach: "foobar",
                race: "Dwarves",
                reRolls: 1,
                fans: 9,
                dedicatedFans: 3,
                playerArray: players.slice(11),
                roster: { rosterId: "2", rosterName: "Dwarves", apothecary: true },
                score: 0,
            },
            actingPlayer: {
                playerId: "18109690",
                currentMove: 0,
                goingForIt: false,
                hasBlocked: false,
                hasFed: false,
                hasFouled: false,
                hasMoved: false,
                hasPassed: false,
                standingUp: true,
                sufferingBloodlust: false,
                sufferingAnimosity: false,
                playerAction: { name: "MoveAction" },
            },
            fieldModel: {
                ballCoordinate: [8, 5],
                ballInPlay: true,
                ballMoving: false,
                bombCoordinate: null,
                playerDataArray: players.map((p) => ({
                    playerId: p.playerId,
                    playerCoordinate: p.playerCoordinate,
                    playerState: p.playerState,
                })),
                weather: { name: "clear" },
            },
            turnMode: { name: "regular" },
            turnDataHome: { turnNr: 7 },
            turnDataAway: { turnNr: 7 },
            turnTime: 12345,
            gameTime: 456789,
            half: 1,
            homePlaying: true,
        },
    };
}

const inputs = [
    "",
    "x",
    "hello",
    '{"netCommandId":"clientPing","timestamp":1735689600000}',
    '{"netCommandId":"clientJoin","clientMode":"spectator","coach":"saruman","password":"ddec93be11e06d2e71838dd22a89bdf5","gameId":12345,"gameName":"","teamId":"","teamName":""}',
    '{"netCommandId":"clientTalk","talk":"gg wp!"}',
    JSON.stringify(makeGameState()),
    // Dictionary growth: highly repetitive text
    "abcdefgh".repeat(200),
    // Long pseudo-random text (worst case for the dictionary)
    Array.from({ length: 3000 }, (_, i) => String.fromCharCode(33 + ((i * 7919) % 95))).join(""),
    // 16-bit character path (non-ASCII)
    "héllo wörld — 血碗 🏈 [éàùîõç] ünïcode",
    // Whitespace and newlines
    "line1\nline2\ttab  \r\nend",
    // All printable ASCII
    Array.from({ length: 95 }, (_, i) => String.fromCharCode(32 + i)).join(""),
    // Single repeated char (long run)
    "a".repeat(5000),
];

const vectors = inputs.map((input) => {
    const compressed = LZString.compressToUTF16(input);
    // Sanity: the JS library must round-trip.
    const roundTrip = LZString.decompressFromUTF16(compressed);
    if (roundTrip !== input)
        throw new Error("JS library round-trip failed!");
    return {
        input,
        compressedUtf16Le: utf16leToBase64(compressed),
        compressedLength: compressed.length,
    };
});

const outPath = path.join(__dirname, "lzstring_vectors.json");
fs.writeFileSync(outPath, JSON.stringify({ version: 1, vectors }, null, 2));
console.log(`Wrote ${vectors.length} vectors to ${outPath}`);
