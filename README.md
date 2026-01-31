# MONOTEST

A custom game engine built from scratch in C++, featuring a hot-swappable Lua scripting layer and multiple rendering backends. Made with the help of Google Antigravity.

The "game" code here is just a testing ground for the engine, so if you are looking for a game to play, go somewhere else.

This repository only stands as an online backup for a personal project that I am working on.
If you are interested in iterating on the engine, you will need to fork this repo.

## IMPORTANT NOTE

With how `game.cpp` is set up, it will likely crash due to the absence of the assets
`./assets/bkg/testbackground.pbm` and `./assets/snd/boing.wav` (I cannot distribute these!)

If you want to build with the state that `game.cpp` is in, you will need to provide these assets.

`./assets/bkg/testbackground.pbm` is specifically a monochrome binary PBM sized 640x480. You can create your own using GIMP or any other image editor that supports PBM exports and monochrome color indexing.

All audio assets are expected to be a Windows ADPCM WAV file. You can convert any audio file to ADPCM using Audacity or any other audio editor that supports ADPCM encoding.

## Features

- **Cross-Platform**: Runs on Linux (X11/XCB) and Windows (D3D11/GDI).
- **Scripting**: Lua 5.1 integration for rapid game logic iteration. (Barely foundational, still needs work)
- **Rendering**: Pixel-perfect scaling, scanline effects, and *monochrome graphics*.
- **Audio**: Miniaudio integration for sound playback.
- **ECS**: Simple Entity-Component-System architecture.

## Build Instructions

### Dependencies (Linux)

- `g++`, `make`
- `libx11-dev` (for X11 platform)
- `libxcb1-dev` (for XCB platform)

See BUILD.txt for more information

### Compilation

The project uses a Makefile with several flags.
Note that the game code `game.cpp` and `game.h` is compiled along with the engine code.

**Standard Build (Linux XCB):**

```bash
make platform=xcb optimize=1
```

**Run:**

```bash
./bin/monotest
```

### Build Options

See BUILD.txt

## Toggle Keys

- **F6**: Toggle Pixel Perfect
- **F7**: Invert Colors
- **F8**: Toggle Interlacing (not available on DX11 backend)
- **F9**: Toggle "Dead Space Color"

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.
