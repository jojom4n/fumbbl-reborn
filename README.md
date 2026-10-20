# FUMBBL Reborn — Native FUMBBL Client

A native **C++ / Qt 6** client for [FUMBBL](https://fumbbl.com).

> **Status: spectator for now, game client in the making.** Today the client
> lets you **watch any live match** in real time. The end goal is a full
> **game client** — playing your own team — and the protocol/model layer is
> being built with that in mind. Spectating is the first milestone.

## Disclaimer

This is an **unofficial, fan-made** project. It is **not affiliated with,
endorsed by, sponsored by, or connected in any way to FUMBBL** or its
operators and maintainers. “FUMBBL”, the FUMBBL service, its name, logo and
all related material are the property of their **legitimate owners**. This
client only connects to the public FUMBBL service (OAuth2 + the public
WebSocket command channel) to spectate — and eventually to play — publicly
available matches. It does not modify, copy or redistribute any FUMBBL
proprietary content. This software is provided **“as is”, without warranty of
any kind**.

## License

This project is licensed under the **GNU General Public License v3.0 or
later** — see [LICENSE](LICENSE).

## Features (spectator, current)

- **Live spectating** — File ▸ **Spectate Game…** opens a dialog with the
  list of live matches, filterable by **race, coach, team name and division**
  (or enter a Game ID directly).
- **Real-time field** — a fullscreen pitch driven by the official protocol:
  players, the ball, and a **face per piece** for all 31 BB2025 races
  (127 unique pieces, with race-aware name resolution).
- **Translucent HUD** — floating panels (score, turn,
  dice log, chat, roster, player info): fully translucent, resizable,
  drag-and-drop, snapping to an invisible grid, with a persistent layout and
  a *Reset Layout* action.
- **Dice log** — every roll reported with the **player's name** and skills.
- **Chat & spectators** — live match chat and the current spectator list.
- **Protocol robustness** — OAuth2 client-credentials + one-time session
  token, automatic **join retry with a fresh token**, and **visible server
  errors** (no more silent failures).

## Roadmap (game client)

- [ ] Spectate (this milestone)
- [ ] Play your own team as a coach (in-game commands)
- [ ] Team management / roster view
- [ ] Replays

## Requirements

- **Qt 6** — Core, Gui, Widgets, Network, **WebSockets**
- CMake ≥ 3.16 and a C++17 compiler

> On **Arch Linux** Qt 6 WebSockets is a separate package:
> `sudo pacman -S qt6-websockets` (a missing package surfaces as
> `Could NOT find Qt6WebSockets`).

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

```bash
./build/fumbbl-qt
```

On first run, enter your FUMBBL credentials (Client ID, Client Secret,
Username) in the **Spectate Game…** dialog. They are stored locally with
`0600` permissions at
`~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json`.

Then pick a live match and watch.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

## Project layout

```
CMakeLists.txt     top-level build (Qt 6)
LICENSE            GPLv3
src/
  main.cpp         entry point
  core/            protocol layer: OAuth (fumbblauth), WebSocket (fumbblws),
                   session (spectatorsession), game model (gamemodel),
                   report parser (reportparser), match list (matchlist),
                   face assets (assets)
  ui/              Qt widgets: main window, field view, HUD panels,
                   roster / dice-log / chat / player-info, Spectate dialog
assets/            face PNGs + lookup tables (faces.txt, race_roster.txt,
                   race_faces.txt, ...)
tests/             unit & UI-wiring tests (QtTest)
tools/             dev tools: spectator_smoke, ui_connect_probe, gen_assets
```

## Legacy

The previous **TS/React + Tauri** dashboard is preserved in the
**`legacy/ts-react`** branch. This branch (`main`) is the native Qt/C++
client.
