# Terminal Poker Platform

A comprehensive terminal-based poker platform supporting multiple variants, AI opponents, and networking.

Copyright 2025 Rhett Creighton

## 🎮 Demo Programs

### `poker_demo_9_player_beautiful` - **The Flagship Demo**
Our best demonstration showcasing the platform's capabilities:
- **9-player full ring** with perfect circular table layout
- **Smooth chip animations** - Watch chips fly to the pot with realistic arc physics
- **Card replacement animations** - Cards visually discard and draw
- **Professional UI** - Modern design with player boxes and betting display
- **Transparent animation system** - Chips seamlessly blend with any background

```bash
./poker_demo_9_player_beautiful
```

### `poker_demo_27_lowball` - **2-7 Triple Draw Showcase**
A complete 6-player 2-7 Triple Draw Lowball demo featuring:
- **Sophisticated draw animations** - Cards fly away and new ones arrive
- **Hand evaluation display** - Real-time lowball hand strength
- **Multiple betting rounds** - Full game flow demonstration

```bash
./poker_demo_27_lowball
```

## Current Status: Building the Platform

The demos above showcase the UI and animation capabilities. The full platform with playable games is under active development.

## Features Implemented

- **2-7 Triple Draw Lowball** - Complete implementation with proper rules
- **AI Opponents** - Multiple personality types (Fish, Rock, TAG, LAG, Maniac, Calling Station)
- **Tournament System** - Multi-player tournaments with AI
- **Modular Architecture** - Pluggable game variants and UI layouts
- **Character-based UI** - Professional terminal graphics

## Planned Features

- Texas Hold'em, Omaha, 7-Card Stud variants
- Networked multiplayer
- Advanced tournament structures
- Statistics and hand history

## Building the Full Platform

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Quick Play

```bash
# Build everything (cleans and builds from scratch):
./build.sh

# Run the game (one key to play):
./run.sh
```

**That's it! Two scripts - build once, then hit `./run.sh` to play anytime.**

## Architecture

This project is organized as a collection of focused modules:

- **poker-engine**: Core game logic and rules
- **poker-ui**: Terminal user interface with notcurses
- **poker-ai**: Artificial intelligence opponents
- **poker-network**: Multiplayer networking

See [CLAUDE.md](CLAUDE.md) for detailed development documentation.

## License

Licensed under the Apache License, Version 2.0 - See [LICENSE](LICENSE) file for details.