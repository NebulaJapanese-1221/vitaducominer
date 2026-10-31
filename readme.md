# VitaDucoMiner

Unofficial DuinoCoin miner homebrew for the PlayStation Vita (PS Vita / PSTV).

## What is this?

VitaDucoMiner is a GUI-based DuinoCoin mining application for the PSVita, built
with SDL2 and the official VitaSDK. It is **not** affiliated with or endorsed by
the DuinoCoin project — it is a community homebrew implementation.

## Features

- **Graphical user interface** — Dashboard, Settings, About, and Donation screens
- **DUCO-S1 (SHA-1) proof-of-work mining** with real-time difficulty tracking
- **Live stats** — hashrate (H/s), accepted/rejected shares, difficulty, nonce, uptime
- **Hashrate history graph** — rolling 60-second line graph on the dashboard
- **Connection status** — online/offline indicator, server:port, local IP
- **Activity log** — recent events and reject reasons (wrong difficulty, stale job, bad nonce, etc.)
- **Tap-friendly touch controls** — all buttons work with touchscreen input
- **Persistent configuration** — settings saved to `ux0:/vitaducominer/config.json` (server, port, username, worker, mining key, donation address, auto-start)
- **Auto-start mining** — optionally begin mining immediately on launch
- **Worker name support** — mine as `username.worker`
- **Mining key** — optional DuinoCoin mining key for authenticated shares
- **Automatic pool discovery** — fetch pool from `http://server.duinocoin.com/getPool` (threaded, non-blocking)
- **Reconnection resilience** — exponential backoff auto-reconnect on connection loss
- **Donation screen** — display your DuinoCoin donation address
- **CMake-only build system** — no Makefiles

## Build Requirements

- [VitaSDK](https://github.com/VitaSDK/VitaSDK) installed (`VITASDK` env var set)
- CMake 3.16+
- vdpm packages: `sdl2`

## Building

```sh
export VITASDK=/path/to/vitasdk
./build.sh          # configure + build -> build/vitaducominer.vpk
./build.sh clean    # remove build directory (sce_sys is preserved)
./verify-build.sh   # clean build + verify output artifacts
```

The output `build/vitaducominer.vpk` can be installed on a PSVita (e.g. via
VitaShell or QCMA).

## Usage

1. Install the VPK on your PSVita.
2. Launch `VitaDucoMiner` from LiveArea.
3. On the **Dashboard**, tap **START MINING** to begin (or enable auto-start in Settings).
4. Use **SETTINGS** to configure server, port, username, worker, mining key, and donation address.
5. Tap **FETCH POOL** to auto-discover a DuinoCoin pool from `getPool`.
6. Tap **SAVE** to persist settings to `ux0:/vitaducominer/config.json`.
7. Tap **DONATE** to view your donation address.
8. Tap **QUIT** to close the application.

## Controls

- Tap buttons to activate them (visual feedback on press).
- Tap a settings field to edit its value (on-screen text input).
- **BACK** returns to the dashboard.
- **QUIT** closes the application.

## Project Structure

```
CMakeLists.txt       # CMake-only build (Vita toolchain, SDL2, SceNet/SceHttp/SceIo stubs, self+vpk)
build.sh             # Build/clean wrapper
verify-build.sh      # Clean build verification script
readme.md            # This file
src/main.c           # GUI main loop, config loading, auto-start, sysmodule loading
src/miner.c          # Miner state, PoW loop, TCP client, reconnection with backoff
src/net.c            # SceNet TCP connect/send/recv + local IP via SceNetCtl
src/ui.c             # SDL2 GUI (dashboard, settings, about, donate, bitmap font, touch buttons, hashrate graph)
src/duco-s1.c        # SHA-1 + DUCO-S1 proof-of-work
src/http.c           # SceHttp GET client
src/json.c           # Minimal JSON key/value parser
src/pool.c           # getPool discovery client (sync + threaded async)
src/config.c         # Persistent config save/load via SceIo (ux0:/vitaducominer/config.json)
sce_sys/             # VPK icons and LiveArea assets
```

## License

This project is unofficial DuinoCoin software. DuinoCoin and its logo are
trademarks of their respective owners. This homebrew is provided as-is with no
warranty. Use at your own risk.
