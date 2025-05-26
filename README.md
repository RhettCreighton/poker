# Terminal Poker Game

A beautiful, professional poker game for the terminal using notcurses and sprite graphics.

## 🎯 Project Status

**85-90% Complete** - Professional 2-7 Triple Draw Lowball with sprite graphics

## 🚀 Quick Start

```bash
# Build and run the game
./build.sh && ./poker_game

# Run sprite library demo
cd sprite-experiments
cc -o demo_sprite_library demo_sprite_library.c ../mvc/view/sprite_renderer.c -I.. -lnotcurses-core -lnotcurses -lm
./demo_sprite_library
```

## 📁 Project Structure

```
poker/
├── CLAUDE.md                    # CRITICAL: Read this first! Contains all discoveries
├── README.md                    # This file
├── build.sh                     # Build script with fallbacks
├── poker_game.c                 # Main game implementation
├── assets/                      # Visual assets
│   ├── backgrounds/            # poker-background.jpg
│   └── sprites/cards/          # Card PNG files (symlink)
├── mvc/                        # MVC architecture
│   ├── view/
│   │   ├── sprite_renderer.h   # Sprite library API
│   │   └── sprite_renderer.c   # Implementation
│   └── ...
├── sprite-experiments/         # Working experiments and demos
│   ├── demo_sprite_library.c   # Shows how to use sprite library
│   └── exp*.c                  # Various experiments (09-19)
└── common/                     # Game logic libraries
```

## 🎨 Key Features

- **Sprite Graphics**: Real PNG card images (3x5 optimal size)
- **Smooth Animations**: Bezier curve card dealing
- **Multiple Variants**: 2-7 Triple Draw implemented, Texas Hold'em ready
- **AI Players**: Personality-based opponents
- **Professional UI**: poker-background.jpg with proper rendering

## 📚 For Future Development

1. **ALWAYS read CLAUDE.md first** - Contains critical discoveries
2. **Use the sprite library**: `#include "mvc/view/sprite_renderer.h"`
3. **Run experiments**: `sprite-experiments/` has working examples
4. **Check assets/README.md**: Explains asset organization

## 🔧 Technical Notes

- Requires notcurses with pixel blitter support
- Cards use dedicated planes (prevents rendering issues)
- Background must use dedicated plane (prevents blurring)
- Sprite caching provides ~6% performance improvement

## 📝 License

Apache 2.0 - Copyright 2025 Rhett Creighton