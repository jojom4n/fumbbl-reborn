# Changelog — fumbbl-qt

Format: one entry per version, date, key points. The version of
record is `project(fumbbl-qt VERSION ...)` in `CMakeLists.txt`
(single source of truth; `src/main.cpp` reads it via `FUMBBL_VERSION`).

## 0.2.22 — 2026-11-11
- **Third-party licensing (Qt 6 LGPLv3 compliance)**: added
  `LICENSE-THIRD-PARTY.md` documenting the third-party components —
  Qt 6 under LGPLv3 (linked dynamically against unmodified shared
  libraries; source access statement per LGPL §4) and lz-string under
  WTFPL (the ported UTF-16 codec in `src/core/lzstring.cpp`). The
  README links to it, and both license files now ship at the root of
  every release artifact.

## 0.2.23 — 2026-11-11
- **Inducement badges in the match header**: every inducement a team
  fields is now drawn as a small badge next to its re-roll dots
  (home right of the dots, away mirrored). 21 artwork icons generated
  (Qwen-Image, 1024px — crisp at 4K) covering the full BB2025 FFB
  inducement set: mascots, prayers, coaches, cheerleaders, weather
  mage, wizard (Fireball / Zap!), kegs, bribes, mortuary assistant,
  plague doctor, riotous rookies, throw a rock, master chef, biased
  referee, coaching staff, Bugman, star players, mercenaries, plus a
  distinct wandering-apothecary badge and a roster apothecary badge.
  Hover shows the inducement name, effect and units still available.
- **Consumed inducements go gray**: the badge turns grayscale when its
  charges are exhausted (`value − uses ≤ 0`, live server data — a bribe
  burns a charge on every use, the mascot per half, the wizard once
  per game); passive inducements never consume and stay in color, and
  a server recharge brings the badge back. Multi-unit inducements show
  a small count disc with the units still available.

## 0.2.21 — 2026-11-11
- **Stadium surround**: the space outside the stadium (letterbox and
  zoomed-out view) is no longer a flat gray band. Each stadium's
  manifest now carries a `surround` color matching its mood (the night
  arenas get a deep near-black; a day stadium can declare a light
  neutral), and the stadium's edges feather into it with a soft
  gradient — the arena reads as lit and deliberate instead of pasted
  on a backdrop.

## 0.2.20 — 2026-11-10
- **Live matches now show the real score.** Live `serverGameState`
  payloads carry the score ONLY in `gameResult.teamResultX.score` (no
  top-level or per-team `score`), which the model ignored — every live
  match rendered 0:0 until the next score update. The parser now reads
  `gameResult` as a third score source, pinned by a new test on a real
  live capture (game 1962007, saved as `tests/liveGameState_hero.json`).
- **README + screenshot set refreshed**: real live-match screenshots
  (hero, field, player card, dice log with a real captured report
  stream, live match list), roadmap now links the five milestone issue
  boards, first-run text updated for the startup sign-in gate.
- **Tools**: the live report capturer now reads the `password_md5=`
  credentials line (it only accepted plaintext `password=`, so after the
  app's keychain migration it hung waiting on the OS keychain — which
  offscreen processes never reach).

## 0.2.19 — 2026-11-09
- **Report card v1 at full time**: when the turn mode hits `endGame`,
  a non-modal card shows the final score (names, coaches, races) and
  both rosters — number, player, position, SPP (base with red
  in-match gain) and final state. It stays open over the ended match
  and closes when the next one starts. Test covers lifecycle, rosters,
  SPP gain and no double card.

## 0.2.18 — 2026-11-09
- **Waiting board**: while a match is being joined, a centered
  "HOME VS AWAY" card (names from the live match list) with the live
  session status appears on the stage; it disappears on game state or
  fatal session error.

## 0.2.17 — 2026-11-09
- **Match list: division tabs + auto-refresh**: the Spectate dialog
  now has All/Competitive/League (plus per-division) tabs and polls
  the live list every 20 s (skipped while a fetch is in flight); the
  selected match survives reloads and the status line shows the match
  count + update time.

## 0.2.16 — 2026-11-09
- **Player card: SPP column and injury states**: the card shows SPP
  base with a red `+gain` for in-match growth, and the state line is
  colored by severity (red for Prone/KO/Badly Hurt/Seriously
  Hurt/Killed, amber for Stunned, gray for box/reserve/missing).

## 0.2.15 — 2026-11-09
- **Carrier/turnover fix**: the ball carrier is the STANDING
  baseState==1 player (0 is UNKNOWN, not a state) — the previous
  check misidentified the carrier on many turns.

## 0.2.14 — 2026-11-09
- **Toast system v1**: broadcast ribbons (CENTER TOUCHDOWN, 2ND HALF,
  EXTRA TIME, FULL TIME) in the center and edge pills (+SPP,
  COMPLETION, REGENERATION, RAISE THE DEAD, TIMEOUT, injury, TURNOVER,
  WATCH-OUT) on the left, both self-fading and mouse-transparent.
  Header half display: PRE-GAME / 1ST HALF / 2ND HALF / EXTRA TIME.

## 0.2.13 — 2026-11-09
- **Model: the toast/SPP data layer** — catch and regeneration roll
  reports, per-player SPP tracking (base/current, first-sight
  baseline), derived ball carrier, inducement sets from team sync
  ( Keg/Bribes/Prayer add-remove), and a toast-event classifier for
  completions, regeneration, raise the dead, timeouts and injuries.

## 0.2.12 — 2026-11-09
- **Tools**: the live smoke/capture tools accept the `password=`
  (plaintext) credentials variant next to `password_md5=`.

## 0.2.11 — 2026-11-08
- **Codebase hygiene**: every comment now cites only our own ground
  truth (the ffb Java sources, the official Java client, live
  captures). No external implementations are referenced anywhere in
  the tracked sources, and one factual slip was corrected (the
  LZString stream is byte-identical to the FFB Java server, not to a
  "JS client"). No behavior change.

## 0.2.10 — 2026-11-08
- **The go-for-it extra move is finally visible**: the red trail and
  the step numbers never appeared for a GFI after a plain move. Root
  cause (ground truth, ffb-common `ActingPlayer.setPlayerAction`): the
  server returns WITHOUT emitting a model-sync when the action does
  not change — and a GFI extra move re-activates the SAME "move"
  action — so the client's `m_movePending` flag was never set and the
  GFI steps were silently appended to the previous (white) route.
  The model now opens a fresh route the moment the actor steps after
  a successful GFI roll, and the GFI step badges are unmistakable:
  red disc, white number (the trail keeps the same red). Regression
  test with the exact wire flow (no action re-emit) plus a permanent
  `gfi_noreemit.png` scene in `move_numbers_preview`.

## 0.2.9 — 2026-11-07
- **Login: the late burst of the verification match no longer leaves a
  frozen match on the field** (0.2.8 regression, reproduced live with
  real credentials). The server keeps delivering the verification
  match's frames (game state, model sync) WHILE the socket is in
  ClosingState — even AFTER the post-login wipe — and the wipe had
  already lifted the suppression: the match rendered and stayed there,
  frozen. The suppression now stays on until the verification socket
  is REALLY closed (close 1000), with a final wipe at close; picking a
  real match (Spectate Game) lifts the suppression as a safety net.
  Client-initiated closes (logout, login teardown) no longer emit
  "WebSocket closed: 1000" which clobbered the "Logged in as …"
  status.
- **Windows build fixed in CI**: Qt for Windows does not ship the DBus
  module (it was REQUIRED and broke the configure), and the
  credential store used modern-SDK forms MinGW-w64's wincred.h does
  not have: the 5-arg `CredReadA` (MinGW keeps the legacy 4-arg
  signature without the count out-param), `LPSTRUCT_CREDENTIALA`,
  `CredFreeA`, and a malformed `const_cast` on the blob. All replaced
  with MinGW-compatible forms, verified against the CI MinGW log.

## 0.2.8 — 2026-11-07
- **Login: the verification match no longer touches the UI at all**
  (structural fix, not a timing one). The FUMBBL protocol verifies the
  password ONLY as part of joining a game (verified in ffb-server
  source: `FumbblRequestCheckAuthorization` approves the join — the
  client inevitably becomes a spectator of that match and the server
  immediately starts streaming its state). Now, from the start of the
  verification join until the post-login wipe, ALL of that match's data
  (game state, model sync, clock, chat) is suppressed in the
  controller: the field stays on the empty stadium during and after
  login until the user picks a match from Spectate Game. The failure
  branches (rejection/close/fatal) lift the suppression.

## 0.2.7 — 2026-11-07
- **Credentials file: never plaintext again.** The 0600 file
  (~/.config/fumbbl-reborn/creds) now stores `password_md5=<hex>` —
  the MD5 of the password, which IS the protocol credential (the join
  sends HMAC-MD5(MD5(password), challenge); the client no longer ever
  needs the clear text). The same value that already goes into the
  keychain; no extra library, no invented format. On restart the
  dialog prefills coach + sentinel (bullets) and uses the MD5; typing
  a new password overwrites it.
- **Login: the join burst's match is no longer left on screen.** The
  server sends gameState/modelSync in the same burst as serverJoin:
  those frames were already queued when the login was closed and
  re-rendered the match AFTER the cleanup. The match-state wipe now
  runs on the next event-loop pass (after the queued frames; the
  socket is meanwhile closing so no more states arrive).

## 0.2.6 — 2026-11-07
- **Remember me: the password is no longer lost when there is no
  keychain**: the 0600 file was "stripped" (password removed)
  IMMEDIATELY at login, before the async keychain write was even
  confirmed — and the store lives in the dialog that dies right after
  accept(). Without Secret Service on the bus (e.g. Niri without
  Kwallet running) the password ended up NOWHERE: empty field on
  restart. Now: the typed password stays in the 0600 file as a safety
  net until the store confirms (strip on confirmation or on next
  startup when the lookup finds the entry); without a keychain the
  file stays the stable fallback.
- **Login no longer opens a random match**: the verification join
  landed on a live match (the last spectated one, else the first) and
  the client just STAYED ON IT. Login only enables the interface: once
  verified, the verification session is closed (silent teardown, close
  1000 with no message), the match state is reset and the status shows
  "Logged in as … — choose a match". The match is picked from Spectate
  Game.

## 0.2.5 — 2026-11-07
- **Login right after a wrong password**: the server closes the
  connection (1000) in the same burst as the rejection status; that
  close arrived in the gate as a failure of the in-flight
  verification: dialog open with "Connection closed (1000: )" and the
  login not completed, even though the session was already live. Now:
  the rejection is reported BEFORE the attempt's socket is torn down
  (the teardown close arrives after the state is already consumed, and
  stays silent), and the gate ignores closes that belong to a
  PREVIOUS attempt anyway (e.g. the logout close in flight at retry
  time).
- **Logout live from the first login**: the active coach was set only
  when picking a match from the Spectate dialog; with the login gate it
  stayed empty (and Logout greyed) until then. The server-confirmed
  coach at login enables it immediately.

## 0.2.4 — 2026-11-07
- **The X of the login dialog really closes the client**: in 0.2.3 the
  quit emitted by the gate was lost — the gate runs BEFORE the main
  event loop (the dialog's `exec()`) and the `app.exec()` that started
  afterwards did not remember the quit: the dialog closed but the
  client kept running. Now `main.cpp` exits when the gate closes
  without a login (X / Esc / Quit).

## 0.2.3 — 2026-11-07
- **Login button live while typing**: the enabled/disabled state
  followed only the coach field losing focus — with the dialog
  prefilled the button stayed grey and unclickable. It is now
  recomputed on every edit of either field.
- **The X of the login dialog closes the client**: before it only
  closed the dialog and left the user in the full UI without login.
  Closing the gate (X / Esc) is now the same decision as the "Quit"
  button — the client closes, the UI is never delivered
  unauthenticated.
- **No crash on exit with an in-flight keychain**: if the client
  closed while a keychain lookup/write was still in progress (slow
  D-Bus round trip), the worker thread delivered its result to an
  already-destroyed object (segfault). Workers now check the store
  still exists before delivering; excess results are dropped.

## 0.2.2 — 2026-11-06
- **Client login at startup**: modal dialog (Coach + Password +
  "Remember me") over the stadium — nothing is clickable (not even the
  menus) until the login is verified. The credentials are verified
  against the FUMBBL server at login time (join attempt: the server
  checks the password in the join —
  `FumbblRequestCheckAuthorization`): ok → full UI (you are already in
  the verification match's stadium, the last watched one or the first
  live); rejected → server error in the dialog, retry. "Remember me"
  (default on) → keychain `{coach, MD5}`.
- **Password field always bullets**: echo mode Password at all times
  (including while typing); with a keychain entry the field shows 8
  bullets (sentinel) — the first keypress replaces them; a typed
  password always wins over the stored (stale) MD5.
- **"Spectate" dialog without credentials**: only match list + filters
  + Game ID + Spectate/double click — it uses the client login's
  credentials. The leftover "Load matches" button is removed too.
- **Logout (Ctrl+K) → login gate reopened**: session ended + remembered
  credential removed from the file (the keychain entry stays); the user
  enters new credentials or closes the client from the dialog's "Quit"
  button.
- **Chat log: word wrap** — long messages wrap instead of running out
  of the panel.

## 0.2.1 — 2026-11-06
- **Log out (shared PC)**: `File ▸ Log out` (Ctrl+K) ends the active
  coach's session and removes the remembered credential: the next
  player's dialog opens empty. The keychain entry stays: a returning
  coach re-types only the name, never the password.
- **Coach switch in the dialog**: editing the coach field discards the
  typed password and the previous coach's stored MD5, and the new
  coach is resolved from the keychain (if already remembered, no
  re-typing).
- **Typed password wins**: if there is a password in the field, the
  login uses its MD5 even when the keychain holds a stale one (coach
  changed, site password changed).
- **Player Info does not appear at startup**: the panel is hover-only
  on the field — the saved "visible" state was a leftover of the last
  hover, not a choice. Saved position/size remain.

## 0.2.0 — 2026-11-06
- **Official FFB login (coach + password)**: replaces the REST-token
  OAuth2. The client now performs the same handshake as the official
  Java client: `clientRequestVersion` → `clientPasswordChallenge` →
  challenge/response (HMAC-MD5 of `MD5(password)`) → `clientJoin`, all
  over the WebSocket. No Client ID/Secret, no REST token for auth.
  Flow verified live against fumbbl.com.
- **System keychain for credentials** ("Remember credentials", default
  on): the keychain (Windows Credential Manager, macOS Keychain, Linux
  Secret Service) stores `{coach, MD5(password)}` — never the plain
  text. Without an available keychain: fallback to a local `0600`
  file (coach + password), as before.
- **"Spectate Game…" dialog rewritten**: Coach/Password fields
  (instead of Client ID/Secret/Username), keychain warning, pre-fill
  from saved credentials; if the keychain has the MD5 the password no
  longer needs re-typing.
- **No auto-retry on rejected login**: a rejection `serverStatus`
  (e.g. "Wrong Password") is a visible, stopping error — the server
  rate limits repeated failures. Auto-retry (with a fresh challenge)
  remains only for the silent timeout with no response.
- Tests: `passwordchallenge_test` with ground-truth vectors generated
  from the official Java class; the dialog's auth contracts in
  `ui_wiring_test`.

## 0.1.1 — 2026-11-03
- **Distributable releases** (GitHub Releases, tag `v*`): the
  `FUMBBL-Reborn` binary, a self-contained Linux bundle (tar.gz +
  AppImage) and a Windows zip, with Qt and plugins included
  (`qt.conf` pins the flat layout; the tls plugin is included for
  HTTPS to fumbbl.com). CI: prebuilt Qt 6.8.3 via
  `jurplel/install-qt-action` (no source builds) + AppImage with
  `linuxdeploy` (see `.github/workflows/release.yml`).
- **App icon**: green/gold helmet generated with Qwen-Image
  (`share/icon.png` for AppImage/bundle, `share/icon.ico` embedded in
  the Windows exe).
- Real client versioning (replaces the "round" bookkeeping): single
  version in CMake, never hardcoded in code.
- Full code cleanup: short technical comments (no references to
  development rounds), removal of dead APIs (e.g.
  `GameModel::findPlayerByNumber`, `FieldView::setTurn`, the unused
  team-name setters, `HeaderView::fixedHeight`).
- The client chat now transmits over the WebSocket when the session is
  joined (`FumbblWs::sendChat`).
- The box strip is repainted once (no redundant painting in the zoomed
  pass).
- Verification tools renamed: `round13_preview` → `hud_preview`,
  `round14_preview` → `move_numbers_preview` (functional names).
- Removed the duplicated `DICE_RCC` block in CMake.

## 0.1.0 — 2026-11-02
- First tracked version of the native Qt6 spectator client
  (stadium, field, dice, action log, chat, dugout, rain).
