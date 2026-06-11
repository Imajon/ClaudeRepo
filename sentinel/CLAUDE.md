# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is SENTINEL//OS

A browser-based escape room / hacking game prop. Players defend a network against cyberattacks through 10 sequential mini-games. Designed to run on 1 or 3 screens simultaneously.

## Running the project

```powershell
cd sentinel
node server.js          # starts WebSocket server on port 3000
# then open http://localhost:3000/SENTINEL_MAIN.html
```

No build step. All logic is plain HTML+JS served statically by `server.js`. The only dependency is the `ws` npm package (already installed in `node_modules/`).

## Architecture

The project is a set of standalone HTML files — no framework, no bundler.

| File | Role |
|------|------|
| `SENTINEL_MAIN.html` | Primary game console: terminal, server list, step orchestration, all game logic |
| `SENTINEL_MAP.html` | World map display (passive in multi-screen mode, receives WS events) |
| `SENTINEL_CAM.html` | Surveillance camera display (passive in multi-screen mode) |
| `server.js` | Node.js WebSocket relay server — broadcasts messages between all connected windows |

### Two display modes

Controlled by `const USE_EXTERNAL_SCREENS` at the top of `SENTINEL_MAIN.html`:
- `false` (default): map and cameras render inside the main window's center column (`#mid-col`)
- `true`: center column is hidden; map/cam windows connect via WebSocket and receive events

### Game flow (in SENTINEL_MAIN.html)

The 10 steps are `async` functions called sequentially in `runGame()`. Each step:
1. Shows a `showBanner()` overlay with instructions, waits for Enter (`waitBanner()`)
2. Activates its specific challenge (keypress, sequence, face-match, map, cam, or text commands)
3. Resolves via a Promise → scores/damages HP → calls next step

**Challenge types:**
- `chKeypress(n, color)` — rapid key mashing (visual blocks in `#kpzone`)
- `chSequence(len)` — memorize & reproduce digit sequence (`#seqp` overlay)
- `chFace(n)` — identify a face from canvas-drawn profiles (`#facep` overlay)
- `chMap(n)` — type city initials on the world map (`#map-panel`, see city table in `README_SENTINEL.md`)
- `chCam(suspect)` — identify the compromised camera (`#camp` overlay or external `SENTINEL_CAM.html`)
- Text commands typed in `#ki` — handled in the `inp` keydown listener via `handleCmd()`

### State variables (global, top of script)

```js
hp, score, step, alive           // core game state
chActive, seqActive, faceActive, mapActive, camActive  // which challenge is running
exfilGB, exfilPerSrv[]           // exfiltration data leak meter
SRVS[]                           // 8 server objects {n, d, s} where s: 0=OK, 2=ATK, 3=DEAD
```

### Scoring & HP

- `addScore(pts, color)` — adds points, triggers burst animation
- `damage(amt, reason)` — reduces HP, triggers flash + sound; calls `endGame(false)` at 0
- `endGame(success)` — shows `#debrief` overlay with per-step results and medal rank

### Audio

`audioCtx` (Web Audio API) is created on first user gesture. `snd(type)` generates sounds procedurally — no audio files.

### Medals

Two medal systems: `#medal-center` (full-screen pop animation on step completion) and `#medal-col` (persistent column on right side of terminal). Medals are SVG drawn dynamically based on performance tier (GOLD/SILVER/BRONZE/IRON).

## Key conventions

- All CSS uses CSS custom properties defined in `:root` — use `var(--c-*)` for any color changes, never hardcode hex values inline.
- Terminal output uses `logLine(text, cssClass)`. Classes: `sys`, `atk`, `warn`, `cmd`, `ok`, `inf`, `seq`, `face`, `cam`.
- The `SENTINEL_MAINb.html` and `SENTINEL_MAIN_5.html` files are older backup/variant versions — `SENTINEL_MAIN.html` is the canonical file.
