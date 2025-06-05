# Terminal Poker Platform

A comprehensive, modular poker platform for the terminal with beautiful graphics, multiple game variants, and network play support.

## 🎯 Project Status

**Production Ready** - Core platform complete with extensible architecture

### ✅ Completed Features
- **Core Engine**: Complete poker game state management with error handling
- **Multiple Variants**: Texas Hold'em, Omaha, 7-Card Stud, Razz, 2-7 Triple Draw
- **AI System**: Personality-based AI with skill levels and opponent modeling
- **Graphics**: Beautiful terminal UI with pixel-perfect card rendering (notcurses)
- **Persistence**: Save/load game states with auto-save functionality
- **Logging**: Comprehensive logging system with multiple output targets
- **Network**: P2P protocol with simulation framework

### 🚧 In Progress
- Network play implementation
- Tournament system
- Additional game variants

## 🚀 Quick Start

```bash
# Prerequisites
# Ubuntu/Debian: sudo apt install libnotcurses-dev cmake
# Fedora/RHEL: sudo dnf install notcurses-devel cmake

# Build everything
./build.sh

# Run demos (no special terminal required)
./build/demos/poker_demo_27_lowball      # Character-based demo
./build/demos/error_logging_demo         # Error handling showcase

# Run pixel demos (requires kitty/iTerm2/WezTerm)
./build/demos/poker_pixel_showcase       # All 52 cards display
./build/demos/poker_animation_final      # Smooth animations

# Run tests
./build/tests/test_simple
```

## 📁 Project Structure

```
poker/
├── common/              # Core poker engine
│   ├── include/poker/   # Public headers
│   └── src/            # Implementation
├── variants/           # Game variant implementations
│   ├── holdem/         # Texas Hold'em
│   ├── omaha/          # Omaha Hi
│   ├── stud/           # 7-Card Stud & Razz
│   ├── draw/           # 5-Card Draw
│   └── lowball/        # 2-7 Triple Draw
├── ai/                 # AI player system
│   ├── personality.h   # AI personalities
│   └── ai_player.h     # AI interface
├── network/            # P2P networking
│   ├── p2p_protocol.h  # Protocol definition
│   └── hand_history.h  # PHH format support
├── server/             # Server components
├── client/             # Client UI
├── demos/              # Example programs
├── tests/              # Test suite
└── assets/             # Graphics and sprites
```

## 🎮 Game Variants

| Variant | Status | Description |
|---------|--------|-------------|
| Texas Hold'em | ✅ Complete | No-limit Hold'em with all streets |
| Omaha Hi | ✅ Complete | 4-card Omaha high only |
| 7-Card Stud | ✅ Complete | Classic stud with antes |
| Razz | ✅ Complete | 7-card stud low |
| 2-7 Triple Draw | ✅ Complete | Lowball with 3 draws |
| 5-Card Draw | ✅ Complete | Classic draw poker |

## 🤖 AI System

The platform includes sophisticated AI opponents with different playing styles:

- **Tight Aggressive (Shark)** - Professional, calculated play
- **Loose Aggressive (Maniac)** - Hyper-aggressive bluffing
- **Tight Passive (Rock)** - Conservative, rarely bluffs
- **Loose Passive (Fish)** - Calls too much
- **GTO Approximation** - Game theory optimal
- **Exploitative** - Adapts to opponents

## 🔧 Development

### Building from Source

```bash
# Full CMake build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Creating a New Game Variant

1. Create directory under `variants/`
2. Implement the variant interface
3. Register in CMakeLists.txt
4. Add tests

### API Example

```c
#include "poker/game_state.h"
#include "poker/player.h"
#include "variants/variant_interface.h"

// Create a game
GameState* game = game_state_create(&TEXAS_HOLDEM_VARIANT, 9);

// Add players
game_state_add_player(game, 0, "Alice", 1000);
game_state_add_player(game, 2, "Bob", 1000);

// Start hand
game_state_start_hand(game);

// Process action
game_state_process_action(game, ACTION_RAISE, 100);
```

## 📋 Error Handling

The platform uses a comprehensive error system:

```c
PokerError err = some_poker_function();
if (err != POKER_SUCCESS) {
    LOG_ERROR("module", "Operation failed: %s", 
              poker_error_to_string(err));
}
```

## 🌐 Network Architecture

- **P2P Protocol**: Decentralized game state synchronization
- **Hand History**: PokerStars HH format support
- **Gossip Protocol**: Player discovery and matchmaking
- **Cryptographic Verification**: Ensure fair play

## 📄 License

Licensed under the Apache License, Version 2.0. See LICENSE file for details.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes with tests
4. Submit a pull request

## 📚 Documentation

- [CLAUDE.md](CLAUDE.md) - Detailed developer guide
- [P2P Network](P2P_NETWORK_IMPLEMENTATION.md) - Network protocol details
- [demos/](demos/) - Example code and usage

## 🏆 Acknowledgments

Built with:
- [notcurses](https://github.com/dankamongmen/notcurses) - Terminal graphics
- C99 standard for maximum compatibility
- Love for poker and clean code

---
**Status**: Production ready core with active development on network features