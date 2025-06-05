# Terminal Poker Game - Developer Guide

## 🚀 Quick Start

```bash
# Prerequisites: Install notcurses
# Ubuntu/Debian: sudo apt install libnotcurses-dev libnotcurses3
# Fedora/RHEL: sudo dnf install notcurses-devel

# Build and run
./build.sh
./build/demos/poker_pixel_showcase      # Pixel demo (requires kitty/iTerm2/WezTerm)
./build/demos/poker_animation_final     # Animation showcase
./build/demos/poker_demo_27_lowball     # Classic character demo
```

## 📁 Project Structure

```
poker/
├── assets/sprites/cards/    # 52 PNG card images (75x113px)
├── demos/                   # Complete demo applications
├── mvc/view/               # Rendering engine with pixel support
├── sprite-experiments/     # Test programs and experiments
└── variants/              # Different poker game types
```

## 🎯 Key Features

- **Pixel-perfect card rendering** using notcurses NCBLIT_PIXEL
- **Professional animations** for dealing, betting, and pot collection
- **Multiple poker variants** including Texas Hold'em and 2-7 Lowball
- **Automatic terminal detection** with graceful fallback

## 💻 Terminal Requirements

| Terminal | Pixel Support | Notes |
|----------|--------------|-------|
| kitty | ✅ Excellent | Recommended for development |
| iTerm2 | ✅ Good | macOS users |
| WezTerm | ✅ Good | Cross-platform |
| xterm | ❌ None | Character rendering only |

## 🔧 Core Code Patterns

### Pixel Card Rendering
```c
// The proven pattern for perfect aspect ratios
struct ncvisual* ncv = ncvisual_from_file("assets/sprites/cards/spadeAce.png");
struct ncvisual_options vopts = {
    .blitter = NCBLIT_PIXEL,
    .scaling = NCSCALE_STRETCH,
};

struct ncvgeom geom;
ncvisual_geom(nc, ncv, &vopts, &geom);

// Let notcurses calculate optimal dimensions
struct ncplane_options nopts = {
    .rows = geom.rcelly > max_height ? max_height : geom.rcelly,
    .cols = geom.rcellx > max_width ? max_width : geom.rcellx,
    .y = y, .x = x, .name = "card"
};
```

### Animation System
```c
// Smooth animations at 50 FPS
Animation* anim = animation_create(ANIM_CARD_DEAL, 500); // 500ms duration
animation_add_stage(anim, card_plane, start_x, start_y, end_x, end_y);
animation_engine_add(engine, anim);
animation_engine_update(engine, 20); // Update every 20ms
```

## 📋 Available Demos

1. **poker_pixel_showcase** - Shows all 52 cards with pixel rendering
2. **poker_animation_final** - Complete game with smooth animations
3. **poker_pixel_10player_professional** - Multi-player Texas Hold'em
4. **poker_pixel_10player_lowball_v2** - 2-7 Triple Draw variant

## 🛠️ Development Tips

- Always run commands from the project root directory
- Use `notcurses_canpixel(nc)` to check pixel support
- Follow the orca demo pattern for aspect ratios (see code above)
- Test in both pixel (kitty) and character (xterm) terminals

## 📚 Next Steps

1. Study `demos/poker_pixel_showcase.c` for rendering patterns
2. Review `mvc/view/animation_engine.h` for animation API
3. Check `demos/poker_animation_spec.md` for detailed requirements
4. Run tests with `./demos/run_animation_tests.sh`

---
**Status**: Production ready with pixel rendering and animations
**License**: Apache-2.0

## 🔐 Error Handling System

The poker platform includes a comprehensive error handling system with thread-local error contexts and convenient macros.

### Error Codes
```c
typedef enum {
    POKER_SUCCESS = 0,
    POKER_ERROR_INVALID_PARAMETER = -1,
    POKER_ERROR_OUT_OF_MEMORY = -2,
    POKER_ERROR_INVALID_STATE = -3,
    POKER_ERROR_NOT_FOUND = -4,
    POKER_ERROR_FULL = -5,
    POKER_ERROR_EMPTY = -6,
    POKER_ERROR_INVALID_CARD = -7,
    POKER_ERROR_INVALID_ACTION = -8,
    POKER_ERROR_INSUFFICIENT_FUNDS = -9,
    POKER_ERROR_GAME_IN_PROGRESS = -10,
    POKER_ERROR_NO_GAME_IN_PROGRESS = -11,
    POKER_ERROR_INVALID_BET = -12,
    POKER_ERROR_NETWORK = -13,
    POKER_ERROR_FILE_IO = -14,
    POKER_ERROR_PARSE = -15,
    POKER_ERROR_TIMEOUT = -16,
    POKER_ERROR_UNKNOWN = -99
} PokerError;
```

### Error Handling Macros
```c
// Check for NULL and return error
POKER_CHECK_NULL(ptr);

// Set error and return
POKER_RETURN_ERROR(POKER_ERROR_INVALID_BET, "Bet amount cannot be negative");

// Check function result
POKER_CHECK(some_function());

// Set error without returning
POKER_SET_ERROR(POKER_ERROR_INVALID_STATE, "Game not started");
```

### Using the Result Type
```c
// For functions that return values
PokerResult result = calculate_pot_odds(pot, bet);
if (result.success) {
    double odds = result.value.double_val;
    // Use the value
} else {
    // Handle error
    LOG_ERROR("game", "Failed: %s", poker_error_to_string(result.error));
}
```

### Example Usage
```c
PokerError process_action(GameState* game, PlayerAction action, int amount) {
    POKER_CHECK_NULL(game);
    
    if (!game->hand_in_progress) {
        POKER_RETURN_ERROR(POKER_ERROR_NO_GAME_IN_PROGRESS, 
                          "No active hand");
    }
    
    if (amount < 0) {
        POKER_RETURN_ERROR(POKER_ERROR_INVALID_BET, 
                          "Negative bet amount");
    }
    
    // Process the action...
    return POKER_SUCCESS;
}
```

## 📊 Logging System

The platform includes a flexible logging system with multiple output targets, log levels, and module-specific macros.

### Log Levels
- `LOG_LEVEL_ERROR` - Critical errors
- `LOG_LEVEL_WARN` - Warnings  
- `LOG_LEVEL_INFO` - Informational messages
- `LOG_LEVEL_DEBUG` - Debug information
- `LOG_LEVEL_TRACE` - Detailed trace logs

### Basic Usage
```c
// Initialize logger
logger_init(LOG_LEVEL_INFO);

// Log messages
LOG_INFO("game", "Starting new hand #%d", hand_number);
LOG_ERROR("network", "Connection failed: %s", error_msg);
LOG_DEBUG("ai", "Calculating hand strength: %.2f", strength);
```

### Module-Specific Logging
```c
// Network module
LOG_NETWORK_INFO("Connected to server");
LOG_NETWORK_ERROR("Packet loss detected: %.1f%%", loss_rate);

// Game module  
LOG_GAME_INFO("Player %s joined table", player_name);
LOG_GAME_WARN("Low chip warning for player %d", seat);

// AI module
LOG_AI_INFO("AI player created: %s", ai_name);
LOG_AI_DEBUG("Decision confidence: %.2f", confidence);
```

### Configuration
```c
// Configure logger
LoggerConfig config = {
    .min_level = LOG_LEVEL_DEBUG,
    .targets = LOG_TARGET_STDOUT | LOG_TARGET_FILE,
    .log_file = NULL,  // Will be set by logger_set_file()
    .include_timestamp = true,
    .include_location = true,
    .use_colors = true
};
logger_configure(&config);

// Set output file
logger_set_file("poker_game.log");

// Add custom callback
void my_log_handler(LogLevel level, const char* module, 
                   const char* file, int line, const char* func,
                   const char* message, void* userdata) {
    // Custom handling
}
logger_set_callback(my_log_handler, userdata);
```

## 💾 Persistence System

The platform provides save/load functionality for game states, player statistics, and auto-save capabilities.

### Saving Game States
```c
// Save options
PersistenceOptions options = {
    .compress_data = false,
    .include_hand_history = true,
    .include_ai_state = true,
    .encrypt_data = false
};

// Save game
PokerError err = save_game_state(game, "savegame.pgs", &options);
if (err != POKER_SUCCESS) {
    LOG_ERROR("save", "Failed: %s", poker_error_to_string(err));
}

// Load game
GameState* loaded_game = NULL;
err = load_game_state(&loaded_game, "savegame.pgs");
```

### Player Statistics
```c
// Define player stats
PersistentPlayerStats stats = {
    .player_id = "player_001",
    .display_name = "Alice",
    .stats = {
        .hands_played = 150,
        .hands_won = 45,
        .total_winnings = 3500,
        .hands_vpip = 38,
        .hands_pfr = 25
    },
    .last_played = time(NULL),
    .total_sessions = 10,
    .peak_chips = 5000,
    .avg_session_length = 45.5
};

// Save/load player stats
save_player_stats(&stats, 1, "player_stats.pps");
load_player_stats(&loaded_stats, &count, "player_stats.pps");
```

### Auto-Save System
```c
// Configure auto-save
AutoSaveConfig config = {
    .enabled = true,
    .interval_seconds = 300,  // 5 minutes
    .max_saves = 5,           // Keep last 5 saves
    .save_directory = "./saves",
    .filename_prefix = "autosave"
};

// Create manager
AutoSaveManager* autosave = autosave_create(&config);

// Register game for auto-save
autosave_register_game(autosave, game, "game_001");

// Trigger immediate save
autosave_trigger(autosave, "game_001");

// Restore from latest save
GameState* restored = NULL;
autosave_restore_latest(autosave, &restored, "game_001");

// Clean up
autosave_unregister_game(autosave, "game_001");
autosave_destroy(autosave);
```

### File Management
```c
// Create save directory
create_save_directory("./saves");

// List save files
char** files;
uint32_t count;
list_save_files("./saves", ".pgs", &files, &count);

// Validate save file
if (validate_save_file("game.pgs")) {
    // File is valid
}

// Delete save
delete_save_file("old_save.pgs");
```

## 🧪 Testing

### Running Tests
```bash
# Build all tests
./build.sh

# Run individual tests
./build/tests/test_simple
./build/tests/test_deck
./build/tests/test_game_state
./build/tests/test_hand_eval
./build/tests/test_ai
./build/tests/test_p2p_network

# Run all tests with CTest
cd build && ctest --output-on-failure
```

### Integration Testing
```bash
# Compile and run integration tests
gcc -o test_integration tests/test_integration.c \
    -Icommon/include -Lbuild/common -lpoker_common \
    -lnotcurses -lpthread -lm
./test_integration
```

### Demo Programs
```bash
# Error handling and logging demo
gcc -o error_logging_demo demos/error_logging_demo.c \
    -Icommon/include -Lbuild/common -lpoker_common \
    -lnotcurses -lpthread -lm
./error_logging_demo

# Persistence demo
./build/demos/persistence_demo

# Simple 2-7 Triple Draw game
./build/src/main/simple_27_draw
```

## 🤖 AI System

The poker platform includes a sophisticated AI system with personality-based decision making, opponent modeling, and adaptive strategies.

### AI Player Types
- **Tight Passive (Rock)** - Plays few hands, rarely raises
- **Tight Aggressive (Shark)** - Professional player, aggressive but selective
- **Loose Passive (Fish)** - Calls too much, rarely raises
- **Loose Aggressive (Maniac)** - Extremely aggressive, plays many hands
- **Random (Chaos)** - Completely unpredictable
- **GTO** - Game Theory Optimal approximation
- **Exploitative** - Adapts to opponent tendencies

### AI Features
- **Hand strength evaluation** using Chen formula and board analysis
- **Pot odds and implied odds** calculations
- **Position-aware** decision making
- **Opponent modeling** with VPIP, PFR, and aggression tracking
- **Tilt and emotion** simulation
- **Bluffing logic** based on personality and situation
- **Tournament ICM** considerations
- **Skill levels** from 1-10 with appropriate play adjustments

### Using the AI System
```c
// Create AI player with specific type
AIPlayer* shark = ai_player_create("ProShark", AI_TYPE_TIGHT_AGGRESSIVE);
ai_player_set_skill_level(shark, 0.9f); // Expert level

// Get AI decision
int64_t amount;
PlayerAction action = ai_player_decide_action(shark, game_state, &amount);

// Observe opponent actions for modeling
ai_player_observe_action(shark, opponent_seat, ACTION_RAISE, 200);

// Create a full table of AI players
AIPlayer** table = ai_engine_create_table(6, 3, 8); // 6 players, skills 3-8
```

### AI Demo
```bash
./build/demos/ai_showcase_demo
```

## 📈 Recent Updates

### Version 1.2.0 (Latest)
- ✅ **Complete AI system** with personality-based decision making
- ✅ **Opponent modeling** and adaptive strategies
- ✅ **Skill levels** from beginner to expert
- ✅ **Multiple AI strategies** including GTO and exploitative
- ✅ Comprehensive error handling system with thread-local contexts
- ✅ Full-featured logging with file output and custom callbacks
- ✅ Game state persistence with checksums and validation
- ✅ Auto-save manager with background thread
- ✅ Player statistics tracking and persistence

### Known Issues
- Network simulation module needs completion
- Some variant modules have compilation issues
- Full CMake build doesn't complete (use fallback compilation)