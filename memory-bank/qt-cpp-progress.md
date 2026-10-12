# Qt/C++ Client — Progress & Next Steps (as of 12/ott, branch `qt-cpp`)

## ✅ DONE 12/ott — QDS vendored + dock layer swapped + layout persistence
Phase 4 committed. The dock layer in `src/ui/mainwindow.cpp` is now the
QT Advanced Docking System; layout persists to QSettings (key
`mainwindow/dockLayout`) on close, restored at startup; "Reset Layout"
restores a remembered default state. `--screenshot <file>` CLI flag added
to `main.cpp` (works with `QT_QPA_PLATFORM=offscreen`; pairs with `--smoke`).

**QDS source (USER-CONFIRMED, do not second-guess):**
https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
(Any other QDS/ADS repo — mfreiholz, JanWLevy — is stale/unofficial per
user. mfreiholz v1.0.0 has the OLD `ADS_NS`/qmake API; githubuser0xFFFF is
the current one: CMake, v5.1.1, LGPL v2.1.)

**Referenced as a GIT SUBMODULE at `qt-client/third_party/QDS`**,
pinned to tag **v5.1.1** (commit 023ce95) — the user does NOT want the
QDS source replicated in the repo ("un link, non una replica"), and the
source must stay UNTOUCHED ("Non toccare il codice sorgente di QADS,
devi solo usarlo, non modificarlo"). Clone it with:
`git clone --recurse-submodules` or `git submodule update --init`.
CMake wires it:
```
cmake: set(BUILD_STATIC ON ... FORCE); set(BUILD_EXAMPLES OFF ... FORCE)
add_subdirectory(third_party/QDS QDS)
target_link_libraries(fumbbl-qt PRIVATE ... ads::qtadvanceddocking)
```
Static build → no runtime .so. On Qt 6.9+ it links Qt6::GuiPrivate + xcb
(present on the dev box; CMake configures fine).

**REAL QDS API (corrects the older notes above — those described the
JanWLevy fork that is NOT what we have):**
- namespace `ads` (NOT `Qds::`); headers are FLAT in src/:
  `#include "DockManager.h"`, `#include "DockWidget.h"`,
  `#include "DockAreaWidget.h"` — there is NO `ads/` subdirectory prefix.
- `ads::CDockWidget` inherits **QFrame** (NOT QDockWidget).
  Ctor: `new ads::CDockWidget(dockManager, title, parent)`; content via
  `setWidget(w)` (insert modes AutoScrollArea / ForceNoScrollArea).
- `ads::CDockManager` ctor `new ads::CDockManager(this)` (parent =
  QMainWindow) **auto-calls setCentralWidget(this)** — it IS the central
  widget; our FieldView goes inside it as the central dock:
  `dockManager->setCentralWidget(fieldDock)`.
- Add/split: `addDockWidget(area, w)` (new area) / `addDockWidget(area,
  w, targetArea)` (relative placement). **Split orientation is FIXED per
  area** (`internal::dockAreaInsertParameters` in ads_globals.cpp):
  Left/Right areas split HORIZONTALLY, Top/Bottom areas split
  VERTICALLY. So to get "protocol log ABOVE home roster" (left column,
  vertical) add the protocol with `ads::TopDockWidgetArea` relative to the
  roster's area; to get "playerInfo | diceLog | chat" side-by-side in the
  bottom band, chain `ads::RightDockWidgetArea` relative to the previous
  area's returned `CDockAreaWidget*`.
- Tabs: `addDockWidgetTab(area, w)` / `addDockWidgetTabToArea(w, area)`.
- Central title bar hidden via static
  `ads::CDockManager::setConfigFlag(ads::CDockManager::HideSingleCentralWidgetTitleBar,
  true)` + the central dock must have no title bar actions.
- Dark theme: `dockManager->setColorSchemeMode(
  ads::CDockManager::ColorSchemeMode::FollowPalette)` (follows our dark
  QPalette; don't fight the built-in stylesheet).
- Lock the field anchor: `fieldDock->setFeatures(
  ads::CDockWidget::DockWidgetFocusable)` (no closable/movable/floatable).
  Feature bits: Closable=0x001, Movable=0x002, Floatable=0x004,
  Focusable=0x020, Pinnable=0x200.
- View-menu toggles: `dock->toggleViewAction()` (same call as QDockWidget).
- State: `QByteArray saveState(int version)` / `bool restoreState(bytes,
  version)` — XML, matched by dock objectName; version mismatch = no-op.
  We use version 1 (`kLayoutStateVersion`), captured into `m_defaultLayout`
  after initial construction for "Reset Layout".

**Layout verified offscreen** (`--smoke --screenshot`): left column
(protocol above roster), centered field anchor (no title bar), away roster
right, bottom band playerInfo|diceLog|chat side by side. Persistence
confirmed writing `mainwindow/dockLayout` to the QSettings conf.

## Original decision (valid)
Abandon the Tauri+React dashboard as the UI direction. Build a **native
C++/Qt 6 client** (Qt 6.12 + moc/uic/rcc, g++ 16, CMake 4.4, Ninja are
installed). Rust+Qt (qmetaobject) was considered but rejected in favor
of C++ (Qt's reference language, mature tooling, built-in
QWebSocket/QtNetwork). SDL was rejected (not a GUI toolkit).

Goal: Office/VS-style UI — native top menu bar, movable toolbars,
drag-and-drop dockable panels, dark FUMBBL theme.

**Current mode: SPECTATOR ONLY** (user: "il client servirà per giocare, ma
per ora manteniamoci sempre sul fare gli spettatori, come debug").

## Original decision (valid)
Abandon the Tauri+React dashboard as the UI direction. Build a **native
C++/Qt 6 client** (Qt 6.12 + moc/uic/rcc, g++ 16, CMake 4.4, Ninja are
installed). Rust+Qt (qmetaobject) was considered but rejected in favor
of C++ (Qt's reference language, mature tooling, built-in
QWebSocket/QtNetwork). SDL was rejected (not a GUI toolkit).

Goal: Office/VS-style UI — native top menu bar, movable toolbars,
drag-and-drop dockable panels, dark FUMBBL theme.

**Current mode: SPECTATOR ONLY** (user: "il client servirà per giocare, ma
per ora manteniamoci sempre sul fare gli spettatori, come debug").

## Git state
- `main` = working Tauri+React client, tagged **`v0.1.0-spectator`**
  (local tag; **push to origin still pending** — no GitHub credentials in
  the dev environment: `git push origin main && git push origin v0.1.0-spectator`)
- `qt-cpp` = the new client, commits on top of `main`:
  - `ed13d95` phase 1 — Qt6 skeleton (menus, docks, field, theme, --smoke)
  - `e0cf38e` phase 2 — LZString UTF16 codec (byte-identical to JS, 13/13 vectors)
  - `4571f04` phase 3 — network core (auth + WS spectator client) + live smoke tool
  - `c9c599a` chore — stop tracking qt-client/build/ artifacts (gitignore)
  - phase 4 (QDS dock layer + layout persistence) — commit in progress 12/ott

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
   verified byte-identical). Needs a live match + the user's go-ahead.
1.5. ✅ **DONE** — Vendor QDS + swap the dock layer (see "DONE 12/ott" above).
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
5. **Layout persistence**: QDS layout serialization (DockManager
   toJson/fromJson) into QSettings on close/open (plumbing already in
   MainWindow::savedLayout/restoreLayout as a fallback).
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
