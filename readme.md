# VitaDucoMiner

Unofficial DuinoCoin miner homebrew for the PlayStation Vita (PS Vita / PSTV).

## What is this?

VitaDucoMiner is a GUI-based DuinoCoin mining application for the PSVita, built
with SDL2 and the official VitaSDK. It is **not** affiliated with or endorsed by
the DuinoCoin project — it is a community homebrew implementation.

## Features

- Graphical user interface (dashboard, settings, about screens)
- DUCO-S1 (SHA-1) proof-of-work mining
- Real-time hashrate, accepted/rejected shares, difficulty, nonce, and uptime
- Connection status and local IP display
- Hashrate graph and activity log
- Tap-friendly touch controls
- Customizable server, port, username, and mining key
- Automatic mining pool discovery via `http://server.duinocoin.com/getPool`
- CMake-only build system (no Makefiles)

## Build Requirements

- [VitaSDK](https://github.com/VitaSDK/VitaSDK) installed (`VITASDK` env var set)
- CMake 3.16+
- vdpm packages: `sdl2`, `zlib`

## Building

```sh
export VITASDK=/path/to/vitasdk
./build.sh          # configure + build -> build/vitaducominer.vpk
./build.sh clean    # remove build directory (sce_sys is preserved)
```

The output `build/vitaducominer.vpk` can be installed on a PSVita (e.g. via
VitaShell or QCMA).

## Usage

1. Install the VPK on your PSVita.
2. Launch `VitaDucoMiner` from LiveArea.
3. On the **Dashboard**, tap **START MINING** to begin.
4. Use **SETTINGS** to configure your server, port, username, and mining key.
5. Tap **FETCH POOL** to auto-discover a DuinoCoin pool from `getPool`.
6. Tap **SAVE** to apply settings, then return to the dashboard and start mining.

## Controls

- Tap buttons to activate them.
- Tap a settings field to edit its value (on-screen text input).
- **BACK** returns to the dashboard.
- **QUIT** closes the application.
```

## License

This project is unofficial DuinoCoin software. DuinoCoin and its logo are
trademarks of their respective owners. This homebrew is provided as-is with no
warranty. Use at your own risk.
