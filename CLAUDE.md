# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

XTux Arena is a multiplayer action game with a dedicated server (`tux_serv`) and X11 client (`xtux`). The project is written in C (C89/C90) and uses recursive Makefiles.

## Build Commands

```bash
make          # Build everything: common lib, client, and server
```

This produces two binaries in the project root:
- `xtux` — X11 game client
- `tux_serv` — dedicated headless server

Build order is enforced: `src/common` (→ `xtuxlib.a`) → `src/ggz` → `src/client` → `src/server`.

**Client dependencies:** X11, Xpm, libm. Headers are expected at `/usr/X11R6` (hardcoded in `src/client/Makefile`).

There is no test suite. Manual testing uses `data/maps/test.map` and `data/maps/ai_test.map`.

## Running

```bash
# Standalone (single-player via menu)
./xtux

# Networked deathmatch
./tux_serv -m penguinland.map -f 30 -c 8   # Terminal 1
./xtux                                       # Terminal 2+, connect to localhost:8390
```

Key server flags: `-m MAP`, `-f FPS` (default 40), `-c MAX_CLIENTS` (default 8), `-p PORT` (default 8390), `-t TIME_LIMIT`, `-k FRAG_LIMIT`, `-e` (exit when empty).

## Architecture

### Component Layout

```
src/common/   → xtuxlib.a — shared code (net, map, entity/weapon types, math, timing)
src/client/   → xtux      — X11 rendering, input, menus, particles
src/server/   → tux_serv  — authoritative game state, AI, physics, hit detection
src/ggz/      → optional GGZ gaming platform integration
data/         → maps, entity/weapon definitions, XPM tile/entity images
```

### Client-Server Model

The server is the sole authority for game state. Each frame the server:
1. Reads buffered client input (`NETMSG_CL_UPDATE`: 8-bit direction + keypress bitmask)
2. Simulates physics, AI, weapons, collision (`world.c`, `ai.c`, `hitscan.c`)
3. Broadcasts `NETMSG_START_FRAME` → `NETMSG_ENTITY` × N → `NETMSG_END_FRAME`

Clients render whatever the server sends; they do no authoritative simulation.

### Network Protocol

Defined in `src/common/net_msg.h` (protocol version 3). Connection flow:
1. Client → `NETMSG_QUERY_VERSION`
2. Server → `NETMSG_VERSION`
3. Client → `NETMSG_JOIN` (name, map, game mode)
4. Client → `NETMSG_READY` (character type, viewport size)
5. Per-frame loop above begins

Message I/O uses 8 KB buffers (`NETBUFSIZ`). Entity updates are only sent within frame boundaries; text messages are sent immediately.

### Entity & Weapon System

Entities and weapons are data-driven, defined in `data/entities` and `data/weapons` (plain-text config files parsed by `src/common/datafile.c`). Entity types include GOODIE (player), BADDIE (AI enemy), PROJECTILE, ITEM, and VIRTUAL. Weapon classes include PROJECTILE, BULLET, ROCKET, BEAM, and SLUG.

Key headers:
- `src/common/xtux.h` — shared typedefs and constants
- `src/common/net_msg.h` — full message type enum and structs
- `src/server/server.h` — server-side global state
- `src/client/client.h` — client-side global state

### Map Format

Text-based tile maps with named layers: `BASE`, `OBJECT`, `TOPLEVEL`, `ENTITY` (spawn points), `EVENTMETA`, and `TEXT`. Each map references a `.table` file defining tile walkability and graphics. See `doc/mapformat` for the full spec.

## Game Modes

- **SAVETHEWORLD** — players vs. AI enemies (campaign/PvE)
- **HOLYWAR** — free-for-all deathmatch (PvP, supports frag/time limits)
