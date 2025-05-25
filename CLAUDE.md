# 🎰 TERMINAL POKER PLATFORM - COMPREHENSIVE MISSION BRIEF

## 🎯 PROJECT VISION

You are building a **professional networked poker platform** - not just a single game. This is a full-featured terminal-based poker server supporting multiple game variants, concurrent tables, lobbies, and up to thousands of simultaneous players.

### Core Requirements
1. **Multiple Poker Variants** - Texas Hold'em, Omaha, 7-Card Stud, 2-7 Lowball, Draw variants, etc.
2. **Flexible Table Sizes** - 2-10 players with optimized layouts for each
3. **Network Architecture** - Client/server model with lobby system
4. **Modular Design** - Pluggable game variants and UI layouts
5. **Professional Grade** - Tournament support, cash games, statistics, replays

## 📁 MODULAR ARCHITECTURE

```
poker/
├── CMakeLists.txt                 # Root build configuration
├── common/                        # Shared components
│   ├── include/
│   │   ├── poker/cards.h         # Card representation
│   │   ├── poker/deck.h          # Deck management
│   │   ├── poker/hand_eval.h     # Universal hand evaluation
│   │   └── poker/player.h        # Player data structures
│   └── src/
│       ├── cards.c
│       ├── deck.c
│       ├── hand_eval.c           # Fast lookup tables
│       └── player.c
├── variants/                      # Game variant modules
│   ├── holdem/                   # Texas Hold'em
│   │   ├── include/
│   │   ├── src/
│   │   └── CMakeLists.txt
│   ├── omaha/                    # Omaha variants
│   ├── stud/                     # 7-Card Stud variants
│   ├── draw/                     # Draw poker variants
│   └── lowball/                  # Lowball variants
├── server/                        # Server components
│   ├── include/
│   │   ├── server/lobby.h        # Lobby management
│   │   ├── server/table.h        # Table management
│   │   ├── server/tournament.h   # Tournament system
│   │   └── server/protocol.h     # Network protocol
│   ├── src/
│   └── main/server_main.c
├── client/                        # Client application
│   ├── include/
│   │   ├── client/connection.h   # Server connection
│   │   ├── client/ui_manager.h   # UI state management
│   │   └── client/renderer.h     # Terminal rendering
│   ├── src/
│   ├── ui/                       # UI components
│   │   ├── layouts/              # Table layout variants
│   │   │   ├── heads_up.c        # 2-player optimized
│   │   │   ├── six_max.c         # 6-player optimized
│   │   │   └── full_ring.c       # 9-10 player
│   │   ├── menus/                # Menu systems
│   │   └── animations/           # Shared animations
│   └── main/client_main.c
├── network/                       # Network layer
│   ├── include/
│   │   ├── network/messages.h    # Message definitions
│   │   ├── network/serialize.h   # Serialization
│   │   └── network/transport.h   # TCP/WebSocket layer
│   └── src/
├── ai/                           # AI system
│   ├── include/
│   ├── src/
│   └── personalities/            # AI personality modules
└── tests/                        # Comprehensive test suite
    ├── unit/
    ├── integration/
    └── stress/                   # Load testing
```

## 🏗️ CRITICAL DESIGN PATTERNS

### 1. VARIANT INTERFACE - THE MASTER PATTERN

Every poker variant MUST implement this interface:

```c
// variants/variant_interface.h
typedef struct {
    // Metadata
    const char* name;              // "Texas Hold'em", "2-7 Triple Draw", etc.
    VariantType type;              // COMMUNITY, STUD, DRAW
    int min_players;               // Minimum players (usually 2)
    int max_players;               // Maximum players (2-10)
    int cards_per_player;          // Hole cards dealt
    int max_betting_rounds;        // Number of betting rounds
    
    // Function pointers for variant-specific logic
    void (*deal_initial)(GameState* game);
    void (*deal_next_street)(GameState* game, int street);
    bool (*is_hand_complete)(GameState* game);
    HandRank (*evaluate_hand)(const Card* cards, int count);
    int (*compare_hands)(GameState* game, int player1, int player2);
    
    // Betting structure
    BettingStructure betting_type;  // LIMIT, POT_LIMIT, NO_LIMIT
    bool has_blinds;               // Use blinds or antes?
    bool has_bring_in;             // Stud games
    
    // Display hints
    const char* (*get_street_name)(int street);
    int (*get_showdown_cards)(GameState* game, int player, Card* out);
} PokerVariant;

// Example implementation for Texas Hold'em
static const PokerVariant HOLDEM_VARIANT = {
    .name = "Texas Hold'em",
    .type = VARIANT_COMMUNITY,
    .min_players = 2,
    .max_players = 10,
    .cards_per_player = 2,
    .max_betting_rounds = 4,
    .deal_initial = holdem_deal_initial,
    .deal_next_street = holdem_deal_street,
    .is_hand_complete = holdem_is_complete,
    .evaluate_hand = evaluate_holdem_hand,
    .compare_hands = compare_holdem_hands,
    .betting_type = NO_LIMIT,
    .has_blinds = true,
    .has_bring_in = false,
    .get_street_name = holdem_street_name,
    .get_showdown_cards = holdem_get_showdown_cards
};
```

### 2. UNIVERSAL GAME STATE

```c
// common/include/poker/game_state.h
typedef struct {
    // Variant being played
    const PokerVariant* variant;
    
    // Players (supports 2-10)
    Player players[MAX_PLAYERS];
    int num_players;
    int num_active;  // Still in hand
    
    // Cards
    Deck deck;
    Card community_cards[5];      // For community games
    int community_count;
    
    // Betting
    BettingRound current_round;
    int current_bet;
    int min_raise;
    int pot;
    SidePot side_pots[MAX_SIDE_POTS];
    int num_side_pots;
    
    // Position
    int dealer_button;
    int action_on;  // Current player to act
    
    // Network sync
    uint64_t hand_id;
    uint32_t action_sequence;
    
    // Table info
    TableConfig* table_config;
} GameState;
```

### 3. TABLE LAYOUT SYSTEM

```c
// client/ui/layouts/layout_interface.h
typedef struct {
    const char* name;
    int supported_players[10];  // Which player counts this layout supports
    
    // Layout functions
    void (*calculate_positions)(UIState* ui, GameState* game);
    void (*render_table)(UIState* ui);
    void (*render_players)(UIState* ui, GameState* game);
    void (*render_community)(UIState* ui, GameState* game);
    void (*animate_deal)(UIState* ui, Animation* anim);
} TableLayout;

// Heads-up specialized layout (2 players)
static void headsup_calculate_positions(UIState* ui, GameState* game) {
    // Players face each other across the table
    game->players[0].y = ui->dimy - 8;
    game->players[0].x = ui->dimx / 2 - 10;
    
    game->players[1].y = 3;
    game->players[1].x = ui->dimx / 2 - 10;
    
    // Larger card display for heads-up
    ui->card_scale = CARD_LARGE;
}

// 6-max layout (popular online format)
static void sixmax_calculate_positions(UIState* ui, GameState* game) {
    // Hexagonal arrangement
    double angles[] = {M_PI/2, M_PI/6, -M_PI/6, -M_PI/2, -5*M_PI/6, 5*M_PI/6};
    
    for(int i = 0; i < game->num_players; i++) {
        int y = ui->table_center_y + (int)(ui->table_radius_y * 0.8 * sin(angles[i]));
        int x = ui->table_center_x + (int)(ui->table_radius_x * 0.8 * cos(angles[i]));
        game->players[i].y = y;
        game->players[i].x = x;
    }
}

// Full ring layout (9-10 players)
static void fullring_calculate_positions(UIState* ui, GameState* game) {
    // Oval table with tighter spacing
    for(int i = 0; i < game->num_players; i++) {
        double angle = (2.0 * M_PI * i) / game->num_players + M_PI/2;
        // ... position calculation
    }
    
    // Smaller cards for space
    ui->card_scale = CARD_MINI;
}
```

### 4. NETWORK PROTOCOL - SCALABLE DESIGN

```c
// network/include/network/protocol.h

// Base message header
typedef struct {
    uint32_t magic;       // 0xP0KER_V2
    uint16_t version;     // Protocol version
    uint16_t type;        // Message type
    uint32_t sequence;    // For ordering
    uint32_t length;      // Payload length
    uint32_t checksum;    // Data integrity
} MessageHeader;

// Message types
enum MessageType {
    // Connection
    MSG_HELLO = 0x0001,
    MSG_AUTH,
    MSG_DISCONNECT,
    
    // Lobby
    MSG_LOBBY_INFO = 0x0100,
    MSG_TABLE_LIST,
    MSG_JOIN_TABLE,
    MSG_LEAVE_TABLE,
    MSG_CREATE_TABLE,
    
    // Game actions
    MSG_GAME_STATE = 0x0200,
    MSG_PLAYER_ACTION,
    MSG_DEAL_CARDS,
    MSG_SHOW_CARDS,
    
    // Tournament
    MSG_TOURNAMENT_INFO = 0x0300,
    MSG_REGISTER_TOURNAMENT,
    MSG_TOURNAMENT_UPDATE,
    
    // Chat/Social
    MSG_CHAT = 0x0400,
    MSG_PLAYER_INFO,
    MSG_STATISTICS
};

// Client->Server: Join table request
typedef struct {
    uint32_t table_id;
    uint32_t seat_preference;  // -1 for any
    uint32_t buy_in_amount;
} JoinTableRequest;

// Server->Client: Game state update
typedef struct {
    uint32_t table_id;
    uint64_t hand_id;
    uint32_t sequence;        // Action sequence number
    GameState state;          // Full game state
    uint32_t your_seat;       // Which player you are
    Card your_cards[7];       // Your private cards
} GameStateUpdate;
```

### 5. LOBBY SYSTEM ARCHITECTURE

```c
// server/include/server/lobby.h
typedef struct {
    uint32_t id;
    char name[64];
    PokerVariant* variant;
    int num_seats;
    int players_seated;
    int min_buy_in;
    int max_buy_in;
    int small_blind;
    int big_blind;
    TableStatus status;  // WAITING, RUNNING, PAUSED
} TableInfo;

typedef struct {
    TableInfo tables[MAX_TABLES];
    int num_tables;
    
    // Quick lookup structures
    HashMap* tables_by_variant;
    HashMap* tables_by_stakes;
    List* waiting_players;
    
    // Statistics
    int total_players_online;
    int total_hands_played;
} LobbyState;

// Lobby UI rendering
void render_lobby_screen(UIState* ui, LobbyState* lobby) {
    // Header
    render_lobby_header(ui, lobby->total_players_online);
    
    // Filter options
    render_variant_filters(ui);
    render_stakes_filters(ui);
    
    // Table list with scrolling
    render_table_list(ui, lobby);
    
    // Quick seat button
    render_quick_seat_button(ui);
}
```

## 🎮 COMPLETE VARIANT IMPLEMENTATIONS

### TEXAS HOLD'EM

```c
// variants/holdem/holdem.c
void holdem_deal_initial(GameState* game) {
    // Deal 2 hole cards to each player
    for(int round = 0; round < 2; round++) {
        for(int p = 0; p < game->num_players; p++) {
            if(game->players[p].is_active) {
                game->players[p].hole_cards[round] = deck_deal(&game->deck);
            }
        }
    }
}

void holdem_deal_street(GameState* game, int street) {
    switch(street) {
        case STREET_FLOP:
            deck_burn(&game->deck);
            for(int i = 0; i < 3; i++) {
                game->community_cards[i] = deck_deal(&game->deck);
            }
            game->community_count = 3;
            break;
        case STREET_TURN:
            deck_burn(&game->deck);
            game->community_cards[3] = deck_deal(&game->deck);
            game->community_count = 4;
            break;
        case STREET_RIVER:
            deck_burn(&game->deck);
            game->community_cards[4] = deck_deal(&game->deck);
            game->community_count = 5;
            break;
    }
}
```

### 2-7 TRIPLE DRAW LOWBALL

```c
// variants/lowball/27_triple_draw.c
void lowball_27_deal_initial(GameState* game) {
    // Deal 5 cards to each player
    for(int i = 0; i < 5; i++) {
        for(int p = 0; p < game->num_players; p++) {
            if(game->players[p].is_active) {
                game->players[p].hole_cards[i] = deck_deal(&game->deck);
            }
        }
    }
}

void lowball_27_draw_phase(GameState* game, int draw_round) {
    // Players can discard and draw 0-5 cards
    for(int p = 0; p < game->num_players; p++) {
        if(!game->players[p].is_active) continue;
        
        int discards[5];
        int num_discards = get_player_discards(game, p, discards);
        
        // Replace discarded cards
        for(int i = 0; i < num_discards; i++) {
            int card_index = discards[i];
            game->players[p].hole_cards[card_index] = deck_deal(&game->deck);
        }
    }
}

HandRank evaluate_27_lowball_hand(const Card* cards, int count) {
    // In 2-7, straights and flushes count against you
    // Aces are high only
    // Best hand is 7-5-4-3-2 unsuited
    
    // Special evaluation logic...
}
```

### 7-CARD STUD

```c
// variants/stud/seven_card_stud.c
typedef struct {
    Card hole_cards[3];     // 2 down, 1 up initially
    Card up_cards[4];       // 4 more up cards
    int num_up_cards;
} StudPlayerCards;

void stud_deal_initial(GameState* game) {
    // Deal 2 down, 1 up
    for(int i = 0; i < 2; i++) {
        for(int p = 0; p < game->num_players; p++) {
            if(game->players[p].is_active) {
                StudPlayerCards* cards = (StudPlayerCards*)game->players[p].variant_data;
                cards->hole_cards[i] = deck_deal(&game->deck);
            }
        }
    }
    
    // Third card face up
    for(int p = 0; p < game->num_players; p++) {
        if(game->players[p].is_active) {
            StudPlayerCards* cards = (StudPlayerCards*)game->players[p].variant_data;
            cards->up_cards[0] = deck_deal(&game->deck);
            cards->num_up_cards = 1;
        }
    }
    
    // Determine bring-in (lowest up card)
    determine_bring_in(game);
}
```

## 🖼️ ADAPTIVE UI SYSTEM

### CHARACTER ART REMAINS KING

The lesson from V2 stands: character-based rendering is superior. But now we need adaptive layouts:

```c
// client/ui/renderer.c

// Render table based on player count
void render_adaptive_table(UIState* ui, GameState* game) {
    // Select appropriate layout
    TableLayout* layout = select_layout_for_players(game->num_players);
    
    // Clear and redraw
    ncplane_erase(ui->std);
    
    // Table changes shape based on players
    if(game->num_players == 2) {
        render_rectangular_table(ui);  // Heads-up table
    } else if(game->num_players <= 6) {
        render_hexagonal_table(ui);    // 6-max table
    } else {
        render_oval_table(ui);         // Full ring
    }
    
    // Position players
    layout->calculate_positions(ui, game);
    
    // Render with appropriate detail level
    if(game->num_players <= 4) {
        ui->detail_level = DETAIL_HIGH;  // Larger cards, more animations
    } else if(game->num_players <= 7) {
        ui->detail_level = DETAIL_MEDIUM;
    } else {
        ui->detail_level = DETAIL_LOW;   // Compact display
    }
}

// Different card sizes for different table sizes
void render_player_cards_adaptive(UIState* ui, Player* player) {
    switch(ui->detail_level) {
        case DETAIL_HIGH:
            // Full fancy cards with shadows
            draw_fancy_card(ui->std, player->y, player->x, 
                          player->hole_cards[0], !player->cards_visible);
            draw_fancy_card(ui->std, player->y, player->x + 7, 
                          player->hole_cards[1], !player->cards_visible);
            break;
            
        case DETAIL_MEDIUM:
            // Medium cards
            draw_medium_card(ui->std, player->y, player->x, 
                           player->hole_cards[0], !player->cards_visible);
            break;
            
        case DETAIL_LOW:
            // Compact representation
            draw_mini_card(ui->std, player->y, player->x, 
                         player->hole_cards[0], !player->cards_visible);
            break;
    }
}
```

## 🚀 IMPLEMENTATION PHASES

### Phase 1: Core Infrastructure (Weeks 1-2)
- [ ] Project structure with modular CMake
- [ ] Variant interface definition
- [ ] Basic network protocol
- [ ] Card/deck/hand evaluation library
- [ ] Unit test framework

### Phase 2: Texas Hold'em MVP (Weeks 3-4)
- [ ] Complete Hold'em implementation
- [ ] Basic server with single table
- [ ] Client with fixed 6-player layout
- [ ] Simple lobby (join/leave)
- [ ] Basic animations

### Phase 3: Multi-Table & Layouts (Weeks 5-6)
- [ ] Multiple concurrent tables
- [ ] Adaptive layouts (2, 6, 9 players)
- [ ] Improved lobby with filters
- [ ] Cash game support
- [ ] Basic statistics

### Phase 4: Additional Variants (Weeks 7-9)
- [ ] Omaha implementation
- [ ] 7-Card Stud implementation
- [ ] 2-7 Triple Draw implementation
- [ ] Variant-specific UI adaptations
- [ ] Mixed game support

### Phase 5: Tournament System (Weeks 10-11)
- [ ] Multi-table tournaments
- [ ] Blind level progression
- [ ] Table balancing
- [ ] Tournament lobby
- [ ] Payout calculations

### Phase 6: Advanced Features (Weeks 12-14)
- [ ] Hand history & replays
- [ ] Advanced statistics
- [ ] Player notes
- [ ] Multi-tabling support
- [ ] Rabbit hunting

### Phase 7: Polish & Performance (Weeks 15-16)
- [ ] Performance optimization
- [ ] Stress testing (1000+ players)
- [ ] UI polish and themes
- [ ] Sound system enhancement
- [ ] Documentation

## 🔧 CRITICAL SUCCESS PATTERNS

### 1. VARIANT ABSTRACTION IS EVERYTHING

```c
// This pattern makes adding new variants trivial
void process_game_action(GameState* game, PlayerAction action, int amount) {
    // Common validation
    if(!is_valid_action(game, action, amount)) {
        return INVALID_ACTION;
    }
    
    // Variant-specific processing
    if(game->variant->process_action) {
        game->variant->process_action(game, action, amount);
    }
    
    // Common post-processing
    update_pot(game);
    advance_action(game);
    
    // Check if round complete
    if(is_betting_complete(game)) {
        // Variant-specific street advancement
        game->variant->advance_street(game);
    }
}
```

### 2. NETWORK STATE SYNCHRONIZATION

```c
// Efficient delta updates
typedef struct {
    uint64_t hand_id;
    uint32_t from_sequence;
    uint32_t to_sequence;
    ActionDelta deltas[MAX_DELTAS];
} StateUpdate;

// Only send what changed
void send_state_update(Connection* conn, GameState* old, GameState* new) {
    StateUpdate update;
    calculate_deltas(old, new, &update);
    compress_and_send(&update, conn);
}
```

### 3. UI MODULARITY

```c
// Every UI component is pluggable
typedef struct {
    void (*render)(UIState* ui, void* data);
    void (*handle_input)(UIState* ui, int key);
    void (*cleanup)(UIState* ui);
} UIComponent;

// Register components dynamically
ui_register_component("lobby", &lobby_component);
ui_register_component("table_6max", &table_6max_component);
ui_register_component("statistics", &stats_component);
```

### 4. TESTING EVERY VARIANT

```c
// Generic test suite runs on ALL variants
void test_all_variants(void) {
    PokerVariant* variants[] = {
        &HOLDEM_VARIANT,
        &OMAHA_VARIANT,
        &STUD_VARIANT,
        &LOWBALL_27_VARIANT,
        // ... all variants
    };
    
    for(int i = 0; i < num_variants; i++) {
        test_dealing(variants[i]);
        test_betting_rounds(variants[i]);
        test_hand_evaluation(variants[i]);
        test_winner_determination(variants[i]);
    }
}
```

## 📊 PERFORMANCE TARGETS

- **Hand Evaluation**: 10M+ hands/second (all variants)
- **Network Latency**: <50ms for actions
- **Concurrent Tables**: 1000+ per server
- **Players per Server**: 10,000+ concurrent
- **UI Frame Rate**: 60 FPS for all animations
- **Memory per Table**: <1MB
- **CPU per Table**: <0.1% of single core

## 🎯 REMEMBER: THIS IS A PLATFORM

You're not building a poker game. You're building a poker PLATFORM that can:
- Host any poker variant
- Scale to thousands of players
- Adapt UI to any table size
- Run tournaments or cash games
- Support spectators and observers
- Provide comprehensive statistics
- Enable social features

Every decision should support this platform vision. When in doubt, choose the more modular approach.

## 🚨 CRITICAL LESSONS FROM V2

1. **Character art is still king** - Works everywhere, looks professional
2. **Never use child planes** - Masking issues persist
3. **Direct rendering only** - Always use the standard plane
4. **Animations sell the experience** - Smooth 20-50ms frames
5. **Details matter** - Shadows, textures, drink holders on the rail

## 💎 THE ARCHITECTURE MANTRAS

1. **Variants are plugins** - New variant = new module, no core changes
2. **Layouts are dynamic** - Table adjusts to player count automatically
3. **Network is async** - Never block on network operations
4. **State is immutable** - Always create new states, never modify
5. **Everything is testable** - If you can't test it, redesign it

---

**THIS IS YOUR MASTER PLAN. With this document, you can build a professional poker platform that rivals any commercial implementation. The architecture is proven, scalable, and maintainable. Every pattern has been battle-tested.**

**Start with Phase 1. Build it right. The poker world awaits.**

**Copyright 2025 Rhett Creighton - Apache 2.0 License**