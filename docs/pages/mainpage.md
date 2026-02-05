/** \mainpage MONOTEST Engine API

# Welcome to the MONOTEST Engine

This is the public API documentation for the compiled engine library.

## Getting Started

The engine is exposed as a singleton `mtengine::Engine`. You interact with it primarily through:

- **Initialization**: `mtengine::Engine::init()`
- **Asset Management**: `mtengine::Engine::load_bkg_image()`
- **Game Loop**: `mtengine::Engine::play()`

## Presentation Features

The engine supports several display modes:

- **Pixel Perfect Mode**: Integer scaling -- the default mode.
- **Stretched Mode**: Stretched to fill the screen.
- **Inverted Colors**: Global 1-bit inversion - `set_invert_colors()`
- **Dead Space Color**: Background color for letterboxing - `set_dead_space_is_white()`

The user can toggle between Pixel Perfect Mode and Stretched Mode with F6.

See definition `mtengine::Engine` for the full class reference.
*/
