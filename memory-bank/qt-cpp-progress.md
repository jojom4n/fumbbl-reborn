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
  - `72dba40` phase 4 — QDS docking layer (submodule @ v5.1.1) + layout persistence
  - `4cedbf9` phase 5 — GameModel port (spectator)
  - `e5d8ca2` phase 6 — core→UI wiring + connect dialog + report parser
  - `4fa9b2a` fix — live connection bugs (sender() in functor connect,
    binary WS frames) — see "LIVE VERIFIED" below
  - `969a4a2` memory-bank update
  - `bfc5576` phase 7 — field click-to-select + real Player Info panel
    (see "DONE 13/ott pm" below)

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
## ✅ LIVE VERIFIED (13/ott, commit 4fa9b2a)
Full spectator flow verified against the REAL FUMBBL server on a live
match (game 1953002, XVIII Chaos vs BBT18 Mjöd och Mål):
OAuth → access_token → one-time session token → WS open →
clientPing/clientRequestVersion/clientJoin → serverPong loop +
serverVersion 3.3.1 + serverUserSettings + **serverJoin (spectator)** +
**serverGameState** + continuous **serverGameTime** + 18
**serverModelSync** in 45 s. `spectator_smoke`: **SPECTATOR FLOW OK**,
exit 0. Offline ctest still 3/3 green.

**Fresh OAuth app credentials** were issued by the user and saved to the
standard file (perm 600, OUTSIDE the repo — nothing to gitignore):
`~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json`
(new clientId `KQ0aCF2J…`; the old pair was rejected with
`{"error":"Authorization failed"}`).

**TWO REAL BUGS found by the live test (fixed in 4fa9b2a — don't
re-learn these):**
1. **`sender()` is useless in functor (lambda) connects.**
   `connect(obj, &Signal, ctx, [this](T arg){…})` — inside the lambda,
   `QObject::sender()` does NOT return the emitter (it returns stale
   thread-local state, usually nullptr). FumbblAuth's QNAM `finished`
   handler did `qobject_cast<QNetworkReply*>(sender())` → nullptr →
   **silent return, OAuth never completed, no error emitted at all**.
   Rule: pass the argument explicitly to helper methods.
2. **The FUMBBL server answers with BINARY WebSocket frames** (the
   official Java client exchanges binary: `sendBinary(bytes)`;
   binary→UTF-8 string→`LZString.decompressFromUTF16`). We only
   connected `QWebSocket::textMessageReceived` → serverPong/
   serverVersion/… were all dropped silently. Fix: also connect
   `binaryMessageReceived` (UTF-8 decode → same handler). The TS client
   handles both (ArrayBuffer branch).

**Verified cross-language codec:** C++ `compressToUTF16` output
**decodes correctly with the server's Java LZString** (ffb-common
`LZString.decompressFromUTF16`) and vice versa. Text frames are
accepted by the server (`onBinaryMessage` → `onTextMessage`), so we
keep sending text like the TS client.

**Useful debug techniques that found the bugs:** a standalone QNAM
program (isolates the library), a copy of fumbblauth.cpp with DBG
prints (reply arrived HTTP 200 but `accessTokenReady` never fired),
a raw QWebSocket probe (serverPong came back as BINARY), `javac
-sourcepath ffb-common/src/main/java` to test the server's Java codec
directly. Note: the FUMBBL server logs decode failures to ITS debug log
and stays SILENT toward the client (no error frame, no close).

**LIVE test recipe (works):**
```bash
# find a live game id:
TOKEN=$(curl -s -X POST https://fumbbl.com/api/oauth/token \
  --data-urlencode "grant_type=client_credentials" \
  --data-urlencode "client_id=$ID" --data-urlencode "client_secret=$SECRET" \
  | jq -r .access_token)
curl -s -H "Authorization: Bearer $TOKEN" https://fumbbl.com/api/match/current | jq .
#   (NOTE: curl MUST use --data-urlencode — the secret contains '+' and '/';
#    plain -d sends literal '+' which the server decodes as SPACE →
#    "Authorization failed". The C++ client percent-encodes, it's fine.)
./build/spectator_smoke --game <ID> --duration 45
# expect: RESULT: SPECTATOR FLOW OK, exit 0
```
Credentials are auto-read from
`~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json`
(key `fumbbl_credentials`, fallback `credentials`).

## ✅ DONE 13/ott pm — phase 7: click-to-select + real Player Info (bfc5576)

**Player selection:** left click on a field token (or roster row) selects
the player: Player Info panel fills with real data + gold ring on the
token. Empty-square click deselects. Selection is kept across model
updates (panel refreshes; removal deselects). Path:
FieldView::playerClicked(playerId) / RosterWidget::playerSelected(number)
→ MainWindow::selectPlayer(id) → PlayerInfoWidget::showPlayer(PlayerInfoData).

**GameModel extensions (live payload shape — verified against a REAL
live capture):**
- Live players carry: passing, playerStatus ("active"), playerType
  ("Regular"/"Star"), positionId (NO positionName!), skillValuesMap
  (e.g. Hatred→"treeman", Loner→"4"), usedSkills, lastingInjuries
  (["Seriously Hurt (MNG)"]), recoveringInjury, urlPortrait/urlIconSet
  (relative to https://cdn.fumbbl.com/).
- Position name/shorthand/keywords are resolved via positionId against
  teamX.roster.positionArray (e.g. 66250 → "Werewolf" / "W" /
  [Blitzer,Undead,Werewolf]).
- PlayerState encoding: base = id & 0xFF (1 STANDING, 2 MOVING, 3 PRONE,
  4 STUNNED, 5 KO, 6 BH, 7 SI, 8 RIP, 9 RES, 10 MISS, 11 FALLING,
  12 BLOCKED, 13 BAN, 14 EXH, 15 DRAGGED, 16 PICKED_UP, 17 HIT_ON_GROUND,
  18 SETUP_PREVENTED, 19 IN_THE_AIR); flags: 0x100 ACTIVE, 0x200
  CONFUSED, 0x400 ROOTED, 0x800 HYPNOTIZED, 0x1000 STAB_TARGET, 0x2000
  USED_PRO, 0x4000 BLITZ_TARGET, 0x8000 BLOCK_TARGET, 0x10000
  GAZE_TARGET, 0x20000 EYE_GOUGED, 0x40000 CHOMPED (e.g. 257 =
  STANDING+ACTIVE). Ports: GameModel::stateText (getButtonText),
  stateFlags.
- fieldModel.playerDataArray entries: { playerCoordinate, playerId,
  playerState, cards[], cardEffects[] } — 26 entries incl. box players.
- actingPlayer.playerId can be NULL between turns (model handles it).
- teamX.roster.positionArray = full roster definition (positionName,
  shorthand, keywords, stats, urlIconSet/urlPortrait, cost, quantity).
- turnDataHome/Away: apothecaries, blitzUsed, inducementSet (cardsActive),
  turnNr, ... (turnNr: home wins if non-zero, else away).

**New fixture:** tests/liveGameState_fixture.json (REAL capture, game
1953034, Big Monsters Big Spooky vs OwaOgre, 13+13 players, 71KB) —
produced with the new `spectator_smoke --dump-gamestate <file>` (one-shot
raw serverGameState dump). The old serverGameState_fixture.json is a
SYNTHETIC LZString vector (players carry positionName/playerCoordinate
inline; still used for model-change tests).

**--demo flag** (main.cpp): `--demo <fixture.json>` loads a serverGameState
capture into the window + selects the acting (or first) player — pairs
with --screenshot for visual verification of real-data rendering.

**Roster row bug found+fixed:** the row format showed the POSITION with a
hardcoded "-22s" suffix and never the player NAME (name param unused);
also "P:" meant armour. Now: `# (status) Name (Position)  S: A: M: AV: [skills]`.

**Verified:** ctest 3/3 (new: live-fixture parsing, stateText/Flags, click
select/deselect/selection-follows-sync), offscreen screenshot with live
data looks right, live spectator_smoke SPECTATOR FLOW OK (80 modelSyncs,
1 spectator talk seen live).

## ✅ DONE 13/ott — live spectator verification
See "LIVE VERIFIED" above. Also: the earlier old-credentials failure was
a red herring on the curl side (encoding); the real blockers were the two
bugs in 4fa9b2a.

## NEXT STEPS (in order)
1. ✅ **DONE** Live smoke test + fixes (see "LIVE VERIFIED" above).
1.5. ✅ **DONE** — Vendor QDS + swap the dock layer (see "DONE 12/ott" above).
2. ✅ **DONE** GameModel port (`src/core/gamemodel.{h,cpp}`) + `findPlayerByNumber(
   number, side)` helper: QObject with
   `initialize(QJsonObject game)` + `applyModelChanges(QJsonArray)` +
   `stateUpdated()` signal. Essential handlers only (rest = no-op, like TS):
   ball coord, player coord/state, removePlayer, half, homePlaying, turnMode
   (object {name} or string), passCoordinate, turnNr (only "home" key, TS
   faith), teamResultSetScore (home/away keys), actingPlayerSetPlayerId.
   Statics: isOnField (x 0..25, y 0..14), isBoxX (-1..-7 / 30..36, exact
   FieldCoordinate.java constants), baseState (& 0xFF, PlayerState bit
   flags), stateLabel (KO/BH/SI/RIP/RES/MISS/BAN/PRN/STN). FieldTokens
   computed on demand with hasBall/isActing. VERIFIED: gamemodel_test
   (4 tests) against the REAL serverGameState capture (vector 6 of
   lzstring_vectors.json, copied to tests/serverGameState_fixture.json —
   Orcs United vs Dwarves FC, 22 field players).
3. ✅ **DONE** Wire core -> UI: MainWindow now OWNS a `GameModel` +
   `SpectatorSession` and wires session signals → model → UI. Controller
   entry points are PUBLIC (called directly by tests, driven by session
   signals in production): `onGameState`, `onModelSync`, `onGameTime`,
   `onTalk`, `onJoined`, `applyModelToUi`, `onSessionProgress/Fatal`,
   `onWsClosed`, `onConnectRequested`. `model()` accessor exposed.
   - FieldView::setPlayers + setBall; box coords filtered by the MODEL
     (isOnField/isBoxX) so the field only gets on-field tokens.
   - Rosters: ALL playerArray players, `GameModel::stateLabel()` for
     off-field states (KO/BH/SI/RIP/RES/MISS/BAN/PRN/STN).
   - Chat (serverTalk coach+talks[], STAFF/DEV prefix), dice log via
     the new `reportparser.{h,cpp}` (port of the TS reportParser.ts:
     ~30 skill rolls + pass/blitz/block/injury/reroll/turn/system/etc.;
     unknown reportIds → neutral line, nothing lost).
   - Timer: serverGameTime turnTime = ELAPSED ms → header shows
     `30 - floor(turnTime/1000)`, clamped ≥ 0.
   - **Gotcha (Qt 6.12):** emitting a signal WITH arguments through the
     meta-object is NOT a stable public API — `QMetaObject::activate(
     sender, sigIdx, void**argv)` SEGFAULTS (null QJsonObject delivered),
     and `QMetaMethod::invoke`+QGenericArgument / the template activate all
     silently deliver nothing. So tests must drive the PUBLIC controller
     methods directly, not emit session signals. (Session layer is covered
     by the live spectator_smoke tool instead.)
   - **Qt 6.12 gotchas (widgets):** member `QObject::findChild<T>(parent,
     name)` 2-arg overload is GONE (use 1-arg by name, or the STATIC
     `QObject::findChild<T>(&parent, name)`); `QDialogButtonBox::NoButton`
     (singular); `QString::arg(int,QString)` 2-arg overload doesn't exist
     (chain `.arg(a).arg(b)`); `QStringLiteral(var)` illegal (needs a
     string literal — use a `const QString` constant); a signal and a
     method with the SAME name (e.g. `joined()`) make `&Class::joined`
     ambiguous in connect() — rename the accessor (`isJoined()`).
4. ✅ **DONE** Connect dialog (File > Connect…): ConnectDialog collects
   clientId/secret/username/gameId + "remember". Credentials saved to the
   SAME JSON file the TS client + spectator_smoke use
   (`~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json`,
   key `fumbbl_credentials`, fallback read key `credentials`); last gameId
   kept in QSettings `connect/lastGameId`. m_connectAction now ENABLED.
5. **Layout persistence**: QDS layout serialization (DockManager
   toJson/fromJson) into QSettings on close/open (plumbing already in
   MainWindow::savedLayout/restoreLayout as a fallback).
6. ✅ **DONE** (phase 7, bfc5576) PlayerInfo real rendering + field click
   selection — see "DONE 13/ott pm" above.
6.5. Possible polish: portraits/icons from urlPortrait/urlIconSet
   (relative to https://cdn.fumbbl.com/), skill tooltips, hover tooltip on
   field tokens, team value/treasury in header, cards-in-play display
   (fieldModel playerDataArray[].cards / turnDataX.inducementSet.cardsActive).
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
activeContext.md — are authoritative). The Java sources in
`/home/saruman/Projects/ffb` (ffb-server, ffb-client-logic, ffb-common)
are the AUTHORITATIVE reference for server-side behavior (CommandSocket
message handling, LZString, net command factories) and can be compiled
with `javac -sourcepath ffb-common/src/main/java` for cross-checks.

## Phase 8: Procedural Fantasy Assets (generated, not downloaded)
- **Generator**: `qt-client/tools/gen_assets.cpp` — pure QPainter, no external art.
  CMake targets: `gen_assets` (binary), `generate_assets` (runs it into `assets/`).
  Regenerate: `cmake --build build --target gen_assets && ./build/gen_assets`.
- **Asset catalog (87 PNGs, committed under `qt-client/assets/`)**:
  - `players/<archetype>_{home,away}.png` — 96x96 fantasy mini busts with face:
    blocker (horned helm + spikes), runner (spiky hair + headband tail),
    blitzer (wild hair, glowing eyes, tusks, snarl), catcher (bandana + knot),
    thrower (full helmet + T-visor + crest). 5 archetypes x 2 team colors.
  - `skills/<slug>.png` — 64x64 icons for 51 real FUMBBL skills + `generic`.
  - `cards/<slug>.png` — 64x64 icons for 24 real inducement cards + `generic`.
  - `skill_names.txt`, `card_names.txt` — "Name<TAB>slug" maps (real server names).
- **Runtime loader**: `src/core/assets.{h,cpp}` — locates dir via
  `FUMBBL_ASSETS_DIR` (compile def) > `<exe>/../assets` > `CWD/assets`;
  caches pixmaps; `skillIconSlug()`/`cardIconSlug()` resolve names (fallback
  `generic`); `playerBust(archetype, home)`.
- **GameModel**: `FFBPlayer.cards` (active card names from
  `fieldModel.playerDataArray[].cards`); `FieldToken.archetype`;
  `GameModel::playerArchetype(player)` maps position keywords
  Blitzer/Thrower/Catcher/Runner -> 2/4/3/1, else 0 (blocker).
- **FieldView**: tokens are now busts (squashed + flat when prone/stunned/KO)
  with gold-rimmed number badges; acting player gets a white dashed ring;
  ball indicator moved to top-left corner. Plain ellipse fallback if no assets.
- **PlayerInfoWidget**: bust portrait (52px) next to name; skill list items get
  skill icons; new "In-play cards" section with card icons.
- **Tests**: `gamemodel_test::playerArchetypes`, `ui_wiring_test::assetsLoad`
  (busts non-null, slug maps, fallback). 3/3 green.
- **Live re-verified** (game 1953281, 45s): SPECTATOR FLOW OK.
- Qt 6.12 gotchas hit: `QPixmap::load(QFile*)` removed; no
  `drawPixmap(QRectF, QPixmap)` 2-arg overload (use 3-arg with source rect);
  `QListWidgetItem` has no `setIconSize` (it's on the view).

## ✅ DONE 14/ott — Phase 9: per-piece faces, all 31 BB2025 races (d7f3209)
The user REJECTED the phase-8 approach (5 archetype busts): the deliverable is
ONE FACE per named roster piece of every BB2025 race (127 unique pieces), in
the style of `Esempio facce.png` (chibi fantasy miniature, dramatic rim
lighting, dark background), **face only** (no shoulders/pedestal/props) because
each face sits in a square frame in the player's field cell. Team color is a
RUNTIME ring behind the face, never baked into the asset.

- **Rosters**: all 31 BB2025 team rosters extracted from dadidimerda.it
  (warhammer-community tier-list PDF + JSON-LD) →
  `faces-work/bb2025_rosters.json`. 127 unique deduped pieces; shared
  creatures (Ogre, Minotaur, Troll, Treeman, Rat Ogre, Ghoul/Wight...) get ONE
  canonical face reused by every team that fields them.
- **Assets (committed)**: `qt-client/assets/faces/<slug>.png` (127) +
  `qt-client/assets/faces.txt` (Name<TAB>slug, 127) +
  `qt-client/assets/race_faces.txt` (race → representative slug, 32 lines).
- **Lookup** (`assets.cpp`): `playerFace(positionName, raceHint)` — exact
  position-name match (case-insensitive) → race representative (covers custom
  star names like "Count Luthor von Drakenborg") → generic human. `faceSlug()`
  exposes the slug for tests.
- **UI**: FieldView draws a runtime team-colored circle behind the face
  (`setHomeColor/setAwayColor` + `setHomeRace/setAwayRace`); PlayerInfoWidget
  draws the face in a deterministic team-colored ring; MainWindow feeds the
  model's team races. `GameModel::FieldToken` carries `positionName` (not
  archetype); `playerArchetype()` removed from model/UI.
- **Generation pipeline** (see next section for the ComfyUI details): prompts
  written per piece AFTER reviewing each piece's miniature (SearXNG refs in
  `faces-work/refs*`, team montages in `faces-work/montages/`), then rendered
  with the local ComfyUI (CPU-offloaded) + Blood_bowl LoRA.

## ✅ DONE 15/ott — Phase 10: fullscreen field + transparent overlay HUD (f7f84b5)
User requirements: field rendered much larger (fullscreen), NO white side
stripes, panels TOTALLY transparent (title bar included) and overlaid on the
field, still resizable + drag-and-drop, and "Reset Layout" must restore the
DEFAULT SIZES (the old QDS reset only restored positions).

Decision: QDS floating windows are separate top-levels (Wayland z-order is
compositor-controlled → the field could end up above the panels). The HUD is
now a SINGLE window:
- `FieldView` fills the whole central "stage" widget — `fieldRect()` = full
  widget rect, no margins (cells follow the window aspect). Window opens
  maximized.
- New `src/ui/panel.{h,cpp}` `FloatingPanel`: child of the stage,
  `WA_TranslucentBackground`, painted ~90%-opaque dark rounded rect + 1px
  border + accent top line (home green / away gold / neutral). Title strip
  (26px) = drag zone (clamped so 60px of title stays reachable); 6px edge
  zones resize on left/right/bottom + corners (min 200x120, clamped to stage);
  × button hides the panel; `raise()` on press.
- Panels: home roster, away roster, player info, dice log, chat, protocol log.
  Default geometry is relative to the stage size; applied on the FIRST stage
  resize (event filter on the stage — a `show()` before the top-level is shown
  does not make `isVisible()` true, so the one-shot must wait for the first
  sizing). Clamped on every later stage resize.
- Persistence: small JSON `{"panels":{name:{x,y,w,h,v}}}` in QSettings key
  `mainwindow/panelLayout` (supersedes the old QDS XML key). Reset Layout →
  `resetLayout()` (public, testable) → default geometry + all visible.
  closeEvent never persists an all-hidden state (footgun: a saved
  all-hidden layout traps the user in an empty HUD — this actually happened
  during development).
- QDS is no longer linked into `fumbbl-qt`/tests (submodule kept, untouched,
  still buildable).
- **Test** `ui_wiring_test::panelLayoutPersistAndReset`: default layout lands
  after first sizing; user move+resize; `resetLayout()` restores the EXACT
  default geometry (position AND size); `savedLayout()/restoreLayout()`
  round-trips the user's size. `cellCenter` in the tests updated to the
  margin-less field math. ctest 3/3.

## Phase 9 generation pipeline (faces-work/, git-ignored)
- ComfyUI: local install `/home/saruman/ComfyUI-Installs/ComfyUI/ComfyUI`,
  MUST run CPU-offloaded (the assistant's llama-server owns the GPU):
  `./.venv/bin/python main.py --listen 127.0.0.1 --port 8188 --disable-auto-launch --cpu --extra-model-paths-config /home/saruman/.local/share/comfyui-desktop-2/shared_model_paths.yaml`
- Models (shared dir): `z_image_turbo_int8_convrot.safetensors` (UNET) +
  `qwen_3_4b.safetensors` (CLIP, type `qwen_image`) + `ae.safetensors` (VAE) +
  `Blood_bowl.safetensors` LoRA @ 0.75. KSampler: 9 steps, cfg 1.0, euler,
  beta. 512x512 ≈ 3 min/face on CPU (1024 ≈ 7+ min; 512 is enough for tokens).
- Scripts (persistent, in `faces-work/`):
  - `face_prompts.py` — the 127 per-piece prompts (species carries the race
    language; the piece gets its own hair/expression/eyes/one identity accent;
    shared creatures keep one canonical look).
  - `gen_faces_batch.py` — resumable batch (staging in `faces-work/staging/`,
    log `gen.log`, MD5-derived deterministic seeds per slug, retries).
  - `wf_base.json` — base workflow template.
  - `collect_refs.py` / `slow_retry_refs.py` — SearXNG miniature references
    into `refs/` (SearXNG at 192.168.0.2:8032; JSON image fields `img_src` or
    `thumbnail`; queries `blood bowl <piece> miniature`; slow_retry paces to
    survive rate limits).
  - `make_montages.py` — per-team contact sheets in `montages/` (used to
    review each race's visual language before writing prompts).
  - `sync.sh` — every 120s copies new `staging/*.png` into
    `qt-client/assets/faces/` (cp -n) so the client picks up faces live.
- **Workflow for NEW/REDO faces**: restart ComfyUI if down → edit prompt in
  `face_prompts.py` → run batch (skips existing slugs; delete the staging PNG
  to force a redo) → review in staging → sync.sh picks it up → screenshot the
  client with `--demo tests/liveGameState_fixture.json --screenshot`.
