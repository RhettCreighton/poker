# Terminal Poker Platform

A comprehensive terminal-based poker platform supporting multiple variants, AI opponents, and networking.

Copyright 2025 Rhett Creighton

## Current Status: Playable 2-7 Triple Draw

The project currently features a fully playable 2-7 Triple Draw Lowball game:

```bash
# Quick start - compile and play the standalone game:
gcc -o poker_game standalone_27_draw.c -lm
./poker_game
```

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
# Play 2-7 Triple Draw immediately:
./poker_game
```

## Architecture

This project is organized as a collection of focused modules:

- **poker-engine**: Core game logic and rules
- **poker-ui**: Terminal user interface with notcurses
- **poker-ai**: Artificial intelligence opponents
- **poker-network**: Multiplayer networking

See [CLAUDE.md](CLAUDE.md) for detailed development documentation.

## License

Licensed under the Apache License, Version 2.0 - See [LICENSE](LICENSE) file for details.