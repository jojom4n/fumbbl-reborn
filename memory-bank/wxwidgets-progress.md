# Qt/C++ Client — Progress & Next Steps (as of 11/ott, branch `qt-cpp`)

## Decision (user-approved)
Abandon the Tauri+React dashboard as the UI direction. Build a **native
C++/Qt 6 client** (Qt 6.12 is installed on the dev machine together with
moc/uic/rcc, g++ 16, CMake 4.4, Ninja). Rust+Qt (qmetaobject) was
considered but rejected in favor of C++ (Qt's reference language, mature
tooling, built-in QWebSocket/QtNetwork, QDockWidget = the Office/VS
docking model). SDL was rejected (not a GUI toolkit).

Goal: Office/VS-style UI — native top menu bar, movable toolbars,
drag-and-drop dockable panels, dark FUMBBL theme.

**Current mode: SPECTATOR ONLY** (user: "il client servirà per giocare, ma
per ora manteniamoci sempre sul fare gli spettatori, come debug").

## Git state
- `main` = working Tauri+React client, tagged **`v0.1.0-spectator`**
  (local tag; **push to origin still pending** — no GitHub credentials in
  the dev environment: `git push origin main && git push origin v0.1.0-spectator`)
- `qt-cpp` = the new client, 3 commits on top of `main`:
  - `ed13d95` phase 1 — Qt6 skeleton (menus, docks, field, theme, --smoke)
  - `e0cf38e` phase 2 — LZString UTF16 codec (byte-identical to JS, 13/13 vectors)
  - `4571f04` phase 3 — network core (auth + WS spectator client) + live smoke tool

## What exists (qt-client/)
```
qt-client/
├── CMakeLists.txt            # Qt6 (Core Gui Widgets Network WebSockets), Ninja,
│                             # CMAKE_RUNTIME_OUTPUT_DIRECTORY pinned to build/
├── src/
│   ├── main.cpp              # QApplication, dark Fusion theme, --smoke flag
│   ├── core/
│   │   ├── lzstring.{h,cpp}  # port of lz-string (WTFPL): compressToUTF16 /
│   │   │                     # decompressFromUTF16 — VERIFIED byte-identical
│   │   ├── fumbblauth.{h,cpp}# OAuth2 client_credentials + one-time session token
│   │   ├── fumbblws.{h,cpp}  # WS client: 2s ping (immediate first), LZString
│   │   │                     # frames, version->join sequence, reconnect, routing
│   │   └── spectatorsession.{h,cpp}
│   │                         # full connectAsSpectator flow + progress signals
│   └── ui/
│       ├── mainwindow.{h,cpp}    # QMenuBar File/View/Help, movable header
│       │                         # QToolBar, docks: home/away roster, player info,
│       │                         # dice log, chat, protocol log; View toggles +
│       │                         # Reset Layout; saveState/restoreLayout plumbing
│       ├── fieldview.{h,cpp}     # 26x15 field widget (x 0-25 left=home TD,
│       │                         # y 0-14 top), end zones, center line, tokens, ball
│       ├── rosterwidget / dicelogwidget / chatwidget / headerbar / playerinfowidget
│   └── (UI widgets are placeholders for data: no model wiring yet)
├── tools/
│   └── spectator_smoke.cpp # HEADLESS LIVE test: OAuth -> token -> WS -> join,
│                           # logs every protocol msg, exit 0 on join+gameState
└── tests/
    ├── gen_vectors.cjs      # node generator (uses the repo's node_modules/lz-string)
    ├── lzstring_vectors.json# 13 reference vectors (incl. full serverGameState)
    └── lzstring_test.cpp    # QtTest: decompress(ref)==in, compress(in)==ref EXACT
```

## Build / verify (all green at commit time)
```bash
cd qt-client
cmake -S . -B build -G Ninja            # + -DBUILD_TESTING=ON (default)
cmake --build build
ctest --test-dir build                  # lzstring_test: PASS
QT_QPA_PLATFORM=offscreen ./build/fumbbl-qt --smoke   # UI boots headless, exit 0
```
**LIVE test (next session, needs a running match):**
```bash
# find a live game id:  GET /api/match/current (Bearer access token)
#   (last seen live: game 1952497, "Checks and Balances" vs …)
./build/spectator_smoke --game <ID> --duration 15
# expect: PROGRESS lines, ">> send clientPing/clientRequestVersion/clientJoin",
# "<< recv serverVersion/serverJoin/serverGameState/serverModelSync/…",
# RESULT: SPECTATOR FLOW OK
```
Credentials are auto-read from
`~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json`
(key `fumbbl_credentials`, fallback `credentials`).

## NEXT STEPS (in order)
1. **Run the live smoke test** (above) and fix anything it reveals
   (most likely nothing — protocol facts were ported 1:1 and the codec is
   verified byte-identical).
2. **GameModel port** (`src/core/gamemodel.{h,cpp}`): port the TS
   `fumbblGameModel.ts` — `initialize(game)` (teams, players map,
   fieldModel.playerDataArray coordinates, ball, half, turnMode, turn from
   turnDataHome/Away.turnNr, score from teamHome/Away.score) +
   `applyModelChanges()` with the ESSENTIAL handlers for a spectator
   (fieldModelSetBallCoordinate, fieldModelSetPlayerCoordinate,
   fieldModelSetPlayerState, fieldModelRemovePlayer, gameSetHalf,
   gameSetHomePlaying, gameSetTurnMode, gameSetPassCoordinate,
   turnDataSetTurnNr, teamResultSetScore, actingPlayerSetPlayerId;
   the remaining ~90 modelChangeIds = no-op, exactly like the TS model).
   Reference: /home/saruman/Projects/ffb (ffb-client-logic = official TS
   client, ffb-common = server model incl. ModelChangeId.java,
   PlayerState.java).
3. **Wire core -> UI**: MainWindow slots for gameState/modelSync/gameTime/
   talk/joined → setTeams/setScore/setTurn/setTimerSeconds/setSpectators,
   FieldView::setPlayers (filter box coords: x==-1..-7 or x>=30 → box),
   rosters (ALL playerArray players, state mapping 5=KO,6=BH,7=SI,8=RIP,
   9=reserve, 13=banned), chat, dice log (port reportParser.ts 60+ reports —
   can start with the common ones: blockRoll/dodgeRoll/passRoll/injury/
   turnEnd). Timer: serverGameTime turnTime = ELAPSED ms; UI shows
   30 - floor(turnTime/1000).
4. **Connect dialog** (File > Connect…): clientId/secret/username/gameId +
   "remember" (QSettings or the same JSON file), enable m_connectAction
   (currently disabled).
5. **Layout persistence**: QSettings saveState()/restoreState() on
   close/open (plumbing already in MainWindow::savedLayout/restoreLayout).
6. **PlayerInfo** real rendering (from the selected player: stats, skills,
   status) + field click selection.
7. (Later, out of spectator scope) playing mode: clientAction wiring,
   radial menu, turn-phase filtering — only after spectator is solid.

## Qt 6.12 gotchas (learned the hard way — don't re-learn them)
- `QWebSocket`: ctor `QWebSocket(origin, QWebSocketProtocol::VersionLatest, parent)`,
  connect with `open(QUrl)` (NO connectToHost, NO PlainTextProtocol),
  `closeCode()` returns `QWebSocketProtocol::CloseCode`.
- `QMainWindow` new dock API: `DockOption` enum (AllowTabbedDocks,
  AnimatedDocks, GroupedDragging, …) — `DockWidgetFeature` does NOT exist;
  NO `resetDockOptions()` (use `setDockOptions({})`); move a dock with
  `QDockWidget::setDockLocation(area)` (no QMainWindow::setDockWidgetArea).
- Inside lambdas you must `this->` member calls and qualify enums
  (`QMainWindow::AllowTabbedDocks`).
- `Qt6CoreMacros.cmake` forces `CMAKE_RUNTIME_OUTPUT_DIRECTORY` on the first
  Qt target → pin `set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})`
  BEFORE find_package; in add_test use `$<TARGET_FILE:target>`.
- NO `QVector::anyOf`, NO `QMainWindow::setDockWidgetArea`, NO
  `QString::arg(a,b,c,…)` with mixed types (chain `.arg()`), NO
  `QStringLiteral("lit" + var)`.
- JS-faithful porting traps hit in lzstring: JS arrays auto-extend
  (std::vector needs explicit resize); `c = dictSize-1` after adding a new
  dictionary entry is mandatory in decompress; compressToUTF16 appends a
  trailing space; decompress resetValue=16384 (2^14), 15 bits/char.
- Session token endpoint returns a JSON-encoded STRING (with quotes) →
  parse/strip quotes or the server answers "Wrong Password".

## Memory-bank housekeeping
When resuming: update this file + activeContext.md. The TS client remains
the behavioral reference for everything protocol-related (its memory-bank
notes — fumbbl-protocol-research.md, the "Protocol Discovery Log" in
activeContext.md — are authoritative).
