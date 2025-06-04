/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

# Poker Animation Demos

This directory contains advanced poker game demonstrations with pixel-perfect card rendering and smooth animations.

## Key Files

### Documentation
- `poker_animation_spec.md` - Complete specification of 100 animation tests for professional poker game

### Production-Ready Demos
- `poker_animation_final_pixel.c` - Final pixel-based implementation with PNG card sprites
- `poker_pixel_10player_professional.c` - Professional 10-player poker game with full features
- `poker_pixel_10player_lowball_v2.c` - Enhanced 2-7 triple draw lowball variant

### Testing
- `poker_animation_test.c` - Automated test suite for animation specifications
- `run_animation_tests.sh` - Script to run all animation tests

## Building

From the project root:
```bash
./build.sh
```

Or compile individual demos:
```bash
cc -o demos/poker_animation_final_pixel demos/poker_animation_final_pixel.c -lnotcurses-core -lnotcurses -lm
```

## Running

All demos should be run from the project root directory:
```bash
./demos/poker_animation_final_pixel
./demos/poker_pixel_10player_professional
```

## Requirements

- Pixel-capable terminal (kitty, iTerm2, WezTerm) for pixel demos
- notcurses library installed
- PNG card assets in `assets/sprites/cards/`