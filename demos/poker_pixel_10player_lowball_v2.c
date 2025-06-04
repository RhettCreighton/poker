// Professional 10-Player Network Poker - Clean Architecture
// Proper z-ordering, no overlaps, semantic test functions

#define _XOPEN_SOURCE 700
#include <notcurses/notcurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <locale.h>
#include <pthread.h>
#include <assert.h>

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================

#define MAX_PLAYERS 10
#define CARDS_PER_PLAYER 5
#define HERO_SEAT 0

// Card dimensions (cells)
#define CARD_HEIGHT_HERO 10
#define CARD_WIDTH_HERO 7
#define CARD_HEIGHT_TABLE 7
#define CARD_WIDTH_TABLE 5
#define CARD_SPACING 1

// Table layout
#define TABLE_MARGIN_TOP 4
#define TABLE_MARGIN_BOTTOM 12
#define TABLE_MARGIN_SIDES 6

// Animation
#define DEAL_ANIMATION_MS 300
#define CARD_MOVE_STEPS 15
#define FRAME_DELAY_MS 33  // ~30 FPS

// Z-layers (higher = on top)
#define Z_BACKGROUND 0
#define Z_TABLE 10
#define Z_POT 20
#define Z_PLAYER_INFO 30
#define Z_CARDS_BASE 100  // Cards get Z_CARDS_BASE + card_index
#define Z_DEALER_BUTTON 200
#define Z_UI_OVERLAY 300

// Colors
#define COLOR_FELT_DARK 0x0A4A0A
#define COLOR_FELT_LIGHT 0x0E5E0E
#define COLOR_TABLE_RAIL 0x8B4513
#define COLOR_PLAYER_BOX 0x1A1A1A
#define COLOR_PLAYER_ACTIVE 0x2A2A4A
#define COLOR_HERO_BOX 0x1A1A4A
#define COLOR_TEXT_PRIMARY 0xFFFFFF
#define COLOR_TEXT_SECONDARY 0xCCCCCC
#define COLOR_GOLD 0xFFD700

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    PLAYER_EMPTY = 0,
    PLAYER_ACTIVE,
    PLAYER_FOLDED,
    PLAYER_ALL_IN,
    PLAYER_SITTING_OUT
} PlayerStatus;

typedef struct {
    int rank;  // 2-14
    int suit;  // 0-3
    struct ncplane* plane;
    int x, y;  // Current position
    int width, height;  // Card dimensions
    bool face_up;
    int z_index;
} Card;

typedef struct {
    char name[32];
    int seat_number;
    int chips;
    int bet;
    Card hand[CARDS_PER_PLAYER];
    int num_cards;
    PlayerStatus status;
    
    // Position on table
    int table_x, table_y;
    
    // UI elements
    struct ncplane* info_box;
    struct ncplane* bet_chips;
    struct ncplane* action_text;
} Player;

typedef struct {
    double start_x, start_y;
    double end_x, end_y;
    double current_x, current_y;
    int steps_remaining;
    Card* card;
} CardAnimation;

typedef struct {
    struct notcurses* nc;
    
    // Game state
    Player players[MAX_PLAYERS];
    int dealer_position;
    int pot;
    int current_bet;
    
    // Table geometry
    int term_width, term_height;
    int table_center_x, table_center_y;
    int table_radius_x, table_radius_y;
    
    // UI planes
    struct ncplane* background;
    struct ncplane* table;
    struct ncplane* pot_display;
    struct ncplane* dealer_button;
    struct ncplane* controls;
    
    // Animation state
    CardAnimation animations[52];
    int num_animations;
    pthread_mutex_t anim_mutex;
    bool animating;
    
    // Debug/test mode
    bool test_mode;
    int frame_counter;
} PokerGame;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// Core functions
void init_game(PokerGame* game, struct notcurses* nc);
void cleanup_game(PokerGame* game);
void render_frame(PokerGame* game);

// Table and UI
void create_table(PokerGame* game);
void position_players(PokerGame* game);
void create_player_info_box(PokerGame* game, int player_idx);
void update_pot_display(PokerGame* game);

// Cards
Card* create_card(PokerGame* game, int rank, int suit, int x, int y, int width, int height, bool face_up);
void destroy_card(Card* card);
void move_card_to_layer(Card* card, int z_index);

// Animation
void add_card_animation(PokerGame* game, Card* card, int to_x, int to_y);
void update_animations(PokerGame* game);
bool animations_complete(PokerGame* game);

// Game flow
void deal_cards(PokerGame* game);
void collect_cards(PokerGame* game);

// Test functions
void test_render_single_frame(PokerGame* game);
void test_card_positioning(PokerGame* game);
void test_animation_frame(PokerGame* game, int frame);
void test_z_ordering(PokerGame* game);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const char* get_card_image_path(int rank, int suit, bool face_up) {
    static char path[256];
    
    if (!face_up) {
        strcpy(path, "assets/sprites/cards/blueBack.png");
        return path;
    }
    
    const char* suits[] = {"club", "diamond", "heart", "spade"};
    const char* ranks[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", 
                          "Jack", "Queen", "King", "Ace"};
    
    snprintf(path, sizeof(path), "assets/sprites/cards/%s%s.png", 
             suits[suit], ranks[rank - 2]);
    return path;
}

// ============================================================================
// CARD MANAGEMENT
// ============================================================================

Card* create_card(PokerGame* game, int rank, int suit, int x, int y, 
                 int width, int height, bool face_up) {
    Card* card = calloc(1, sizeof(Card));
    if (!card) return NULL;
    
    card->rank = rank;
    card->suit = suit;
    card->x = x;
    card->y = y;
    card->width = width;
    card->height = height;
    card->face_up = face_up;
    
    // Load card image
    const char* image_path = get_card_image_path(rank, suit, face_up);
    struct ncvisual* ncv = ncvisual_from_file(image_path);
    if (!ncv) {
        free(card);
        return NULL;
    }
    
    // Create plane with exact size
    struct ncplane_options opts = {
        .y = y,
        .x = x,
        .rows = height,
        .cols = width,
        .name = "card",
    };
    
    card->plane = ncplane_create(notcurses_stdplane(game->nc), &opts);
    if (!card->plane) {
        ncvisual_destroy(ncv);
        free(card);
        return NULL;
    }
    
    // Render card image using ORCA pattern
    struct ncvisual_options vopts = {
        .n = card->plane,
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    ncvisual_blit(game->nc, ncv, &vopts);
    ncvisual_destroy(ncv);
    
    return card;
}

void destroy_card(Card* card) {
    if (!card) return;
    if (card->plane) {
        ncplane_destroy(card->plane);
    }
    // Note: Don't free card here as it's part of player's hand array
}

void move_card_to_layer(Card* card, int z_index) {
    if (!card || !card->plane) return;
    card->z_index = z_index;
    // Move card above all planes with lower z_index
    // This ensures proper layering without overlap
    ncplane_move_top(card->plane);
}

// ============================================================================
// TABLE CREATION
// ============================================================================

void create_table(PokerGame* game) {
    // Dark background
    game->background = notcurses_stdplane(game->nc);
    uint64_t bg_channel = 0;
    ncchannels_set_bg_rgb(&bg_channel, 0x0A0A0A);
    ncplane_set_base(game->background, " ", 0, bg_channel);
    
    // Calculate table dimensions
    game->table_center_x = game->term_width / 2;
    game->table_center_y = game->term_height / 2 - 2;
    game->table_radius_x = (game->term_width - TABLE_MARGIN_SIDES * 2) / 2;
    game->table_radius_y = (game->term_height - TABLE_MARGIN_TOP - TABLE_MARGIN_BOTTOM) / 2;
    
    // Create table plane
    struct ncplane_options opts = {
        .y = TABLE_MARGIN_TOP,
        .x = TABLE_MARGIN_SIDES,
        .rows = game->term_height - TABLE_MARGIN_TOP - TABLE_MARGIN_BOTTOM,
        .cols = game->term_width - TABLE_MARGIN_SIDES * 2,
        .name = "table",
    };
    
    game->table = ncplane_create(game->background, &opts);
    if (!game->table) return;
    
    // Draw table with clean gradient
    int table_h = ncplane_dim_y(game->table);
    int table_w = ncplane_dim_x(game->table);
    int center_y = table_h / 2;
    int center_x = table_w / 2;
    
    for (int y = 0; y < table_h; y++) {
        for (int x = 0; x < table_w; x++) {
            // Calculate normalized distance from center
            double dy = (y - center_y) / (double)(table_h / 2);
            double dx = (x - center_x) / (double)(table_w / 2);
            double dist = sqrt(dy * dy + dx * dx);
            
            if (dist <= 1.0) {
                uint64_t color = 0;
                
                if (dist > 0.95) {
                    // Rail
                    ncchannels_set_bg_rgb(&color, COLOR_TABLE_RAIL);
                    ncchannels_set_fg_rgb(&color, 0x654321);
                    ncplane_set_channels(game->table, color);
                    ncplane_putchar_yx(game->table, y, x, '=');
                } else if (dist > 0.9) {
                    // Rail shadow
                    int shade = COLOR_FELT_DARK - (int)(20 * (dist - 0.9) * 10);
                    ncchannels_set_bg_rgb(&color, shade);
                    ncplane_set_channels(game->table, color);
                    ncplane_putchar_yx(game->table, y, x, ' ');
                } else {
                    // Felt with subtle gradient from center
                    int green = COLOR_FELT_DARK + (int)(0x040404 * (1.0 - dist));
                    ncchannels_set_bg_rgb(&color, green);
                    ncplane_set_channels(game->table, color);
                    ncplane_putchar_yx(game->table, y, x, ' ');
                }
            }
        }
    }
    
    // Table center decoration
    uint64_t deco_color = 0;
    ncchannels_set_fg_rgb(&deco_color, COLOR_FELT_LIGHT);
    ncchannels_set_bg_alpha(&deco_color, NCALPHA_TRANSPARENT);
    ncplane_set_channels(game->table, deco_color);
    
    ncplane_putstr_aligned(game->table, center_y - 1, NCALIGN_CENTER, "♠ ♥ ♣ ♦");
    ncplane_putstr_aligned(game->table, center_y, NCALIGN_CENTER, "LOWBALL 2-7");
    ncplane_putstr_aligned(game->table, center_y + 1, NCALIGN_CENTER, "♦ ♣ ♥ ♠");
}

// ============================================================================
// PLAYER POSITIONING
// ============================================================================

void position_players(PokerGame* game) {
    // Professional 10-player oval arrangement
    // Players positioned for optimal visibility and no overlap
    
    double angles[MAX_PLAYERS] = {
        M_PI / 2,         // 0: Bottom center (Hero)
        M_PI * 0.7,       // 1: Bottom left
        M_PI * 0.9,       // 2: Left
        M_PI * 1.1,       // 3: Top left
        M_PI * 1.3,       // 4: Top left-center
        M_PI * 1.5,       // 5: Top center
        M_PI * 1.7,       // 6: Top right-center
        M_PI * 1.9,       // 7: Top right
        M_PI * 2.1,       // 8: Right
        M_PI * 2.3        // 9: Bottom right
    };
    
    // Player names
    const char* names[MAX_PLAYERS] = {
        "Hero", "Mike_M", "Phil_I", "Doyle", "Johnny",
        "Daniel", "Phil_H", "Tom_D", "Gus_H", "Tony_G"
    };
    
    // Calculate positions ensuring no overlap
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game->players[i];
        
        strcpy(p->name, names[i]);
        p->seat_number = i;
        p->chips = 10000 + (rand() % 5000);
        p->status = PLAYER_ACTIVE;
        p->num_cards = 0;
        
        // Position on oval
        double angle = angles[i];
        p->table_x = game->table_center_x + (int)(game->table_radius_x * 0.75 * cos(angle));
        p->table_y = game->table_center_y + (int)(game->table_radius_y * 0.75 * sin(angle));
        
        // Special positioning for hero (bottom center)
        if (i == HERO_SEAT) {
            p->table_y = game->term_height - TABLE_MARGIN_BOTTOM - 2;
        }
    }
    
    // Randomly sit out 1-2 players
    int sit_outs = 1 + (rand() % 2);
    for (int i = 0; i < sit_outs; i++) {
        int seat = 1 + (rand() % 9);  // Not hero
        game->players[seat].status = PLAYER_SITTING_OUT;
    }
}

// ============================================================================
// PLAYER INFO BOXES
// ============================================================================

void create_player_info_box(PokerGame* game, int player_idx) {
    Player* p = &game->players[player_idx];
    
    if (p->info_box) {
        ncplane_destroy(p->info_box);
        p->info_box = NULL;
    }
    
    if (p->status == PLAYER_EMPTY) return;
    
    // Box dimensions
    int box_w = 16;
    int box_h = 3;
    
    // Position box without overlap
    int box_x = p->table_x - box_w / 2;
    int box_y = p->table_y - 2;
    
    // Adjust for screen edges
    if (box_x < 1) box_x = 1;
    if (box_x + box_w >= game->term_width - 1) box_x = game->term_width - box_w - 1;
    if (box_y < TABLE_MARGIN_TOP + 2) box_y = p->table_y + 5;  // Put below if at top
    
    struct ncplane_options opts = {
        .y = box_y,
        .x = box_x,
        .rows = box_h,
        .cols = box_w,
        .name = "player_info",
    };
    
    p->info_box = ncplane_create(game->background, &opts);
    if (!p->info_box) return;
    
    // Style based on player state
    uint64_t box_bg = 0, text_fg = 0;
    
    if (player_idx == HERO_SEAT) {
        ncchannels_set_bg_rgb(&box_bg, COLOR_HERO_BOX);
        ncchannels_set_fg_rgb(&text_fg, COLOR_GOLD);
    } else if (p->status == PLAYER_FOLDED) {
        ncchannels_set_bg_rgb(&box_bg, 0x2A2A2A);
        ncchannels_set_fg_rgb(&text_fg, 0x808080);
    } else if (p->status == PLAYER_SITTING_OUT) {
        ncchannels_set_bg_rgb(&box_bg, 0x1A1A1A);
        ncchannels_set_fg_rgb(&text_fg, 0x606060);
    } else {
        ncchannels_set_bg_rgb(&box_bg, COLOR_PLAYER_BOX);
        ncchannels_set_fg_rgb(&text_fg, COLOR_TEXT_PRIMARY);
    }
    
    ncplane_set_base(p->info_box, " ", 0, box_bg);
    
    // Draw clean border
    ncplane_set_channels(p->info_box, text_fg);
    ncplane_rounded_box(p->info_box, 0, text_fg, box_h, box_w, 0);
    
    // Player name
    ncplane_printf_aligned(p->info_box, 0, NCALIGN_CENTER, " %s ", p->name);
    
    // Chip count
    if (p->status == PLAYER_SITTING_OUT) {
        ncplane_printf_aligned(p->info_box, 1, NCALIGN_CENTER, "Sitting Out");
    } else {
        ncplane_printf_aligned(p->info_box, 1, NCALIGN_CENTER, "$%d", p->chips);
    }
    
    // Dealer button
    if (player_idx == game->dealer_position && p->status == PLAYER_ACTIVE) {
        uint64_t dealer_fg = 0;
        ncchannels_set_fg_rgb(&dealer_fg, COLOR_GOLD);
        ncchannels_set_bg_alpha(&dealer_fg, NCALPHA_TRANSPARENT);
        ncplane_set_channels(p->info_box, dealer_fg);
        ncplane_putstr_yx(p->info_box, 0, box_w - 4, "[D]");
    }
    
    // Move to proper z-layer
    ncplane_move_above(p->info_box, game->table);
}

// ============================================================================
// POT DISPLAY
// ============================================================================

void update_pot_display(PokerGame* game) {
    if (game->pot_display) {
        ncplane_destroy(game->pot_display);
    }
    
    if (game->pot == 0) return;
    
    struct ncplane_options opts = {
        .y = game->table_center_y - 2,
        .x = game->table_center_x - 10,
        .rows = 3,
        .cols = 20,
        .name = "pot",
    };
    
    game->pot_display = ncplane_create(game->background, &opts);
    if (!game->pot_display) return;
    
    // Transparent background
    ncplane_set_bg_alpha(game->pot_display, NCALPHA_TRANSPARENT);
    
    // Gold text for pot
    uint64_t pot_color = 0;
    ncchannels_set_fg_rgb(&pot_color, COLOR_GOLD);
    ncchannels_set_bg_alpha(&pot_color, NCALPHA_TRANSPARENT);
    ncplane_set_channels(game->pot_display, pot_color);
    
    ncplane_printf_aligned(game->pot_display, 0, NCALIGN_CENTER, "╔══════════════╗");
    ncplane_printf_aligned(game->pot_display, 1, NCALIGN_CENTER, "║ POT: $%-6d ║", game->pot);
    ncplane_printf_aligned(game->pot_display, 2, NCALIGN_CENTER, "╚══════════════╝");
    
    // Ensure pot is above table but below cards
    ncplane_move_above(game->pot_display, game->table);
}

// ============================================================================
// CARD DEALING
// ============================================================================

void deal_cards(PokerGame* game) {
    // Create and shuffle deck
    struct {
        int rank;
        int suit;
    } deck[52];
    
    int deck_size = 0;
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 2; rank <= 14; rank++) {
            deck[deck_size].rank = rank;
            deck[deck_size].suit = suit;
            deck_size++;
        }
    }
    
    // Fisher-Yates shuffle
    for (int i = deck_size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp_rank = deck[i].rank;
        int temp_suit = deck[i].suit;
        deck[i].rank = deck[j].rank;
        deck[i].suit = deck[j].suit;
        deck[j].rank = temp_rank;
        deck[j].suit = temp_suit;
    }
    
    // Dealer position (center)
    int dealer_x = game->table_center_x;
    int dealer_y = game->table_center_y;
    
    // Deal cards
    int deck_idx = 0;
    int card_z_counter = Z_CARDS_BASE;
    
    for (int round = 0; round < CARDS_PER_PLAYER; round++) {
        for (int seat = 0; seat < MAX_PLAYERS; seat++) {
            // Deal from dealer's left
            int player_idx = (game->dealer_position + 1 + seat) % MAX_PLAYERS;
            Player* p = &game->players[player_idx];
            
            if (p->status != PLAYER_ACTIVE) continue;
            
            // Get card from deck
            int rank = deck[deck_idx].rank;
            int suit = deck[deck_idx].suit;
            deck_idx++;
            
            // Determine card size
            int card_h = (player_idx == HERO_SEAT) ? CARD_HEIGHT_HERO : CARD_HEIGHT_TABLE;
            int card_w = (player_idx == HERO_SEAT) ? CARD_WIDTH_HERO : CARD_WIDTH_TABLE;
            
            // Calculate final card position (no overlap!)
            int total_width = CARDS_PER_PLAYER * card_w + (CARDS_PER_PLAYER - 1) * CARD_SPACING;
            int start_x = p->table_x - total_width / 2;
            int card_x = start_x + round * (card_w + CARD_SPACING);
            int card_y = (player_idx == HERO_SEAT) ? 
                        p->table_y - card_h - 4 : 
                        p->table_y + 4;
            
            // Create card at dealer position
            bool face_up = (player_idx == HERO_SEAT);  // Only hero sees cards
            Card* card = create_card(game, rank, suit, dealer_x, dealer_y, 
                                   card_w, card_h, false);  // Start face down
            
            if (card) {
                // Store in player's hand
                p->hand[p->num_cards] = *card;
                p->hand[p->num_cards].plane = card->plane;
                p->num_cards++;
                
                // Set z-order (later cards on top)
                move_card_to_layer(&p->hand[p->num_cards - 1], card_z_counter++);
                
                // Animate to position
                add_card_animation(game, &p->hand[p->num_cards - 1], card_x, card_y);
                
                // Free temporary card struct
                free(card);
            }
        }
        
        // Wait for animations to complete before next round
        while (!animations_complete(game)) {
            update_animations(game);
            render_frame(game);
            usleep(FRAME_DELAY_MS * 1000);
        }
    }
    
    // Flip hero's cards after dealing
    usleep(500000);  // Dramatic pause
    
    Player* hero = &game->players[HERO_SEAT];
    for (int i = 0; i < hero->num_cards; i++) {
        Card* card = &hero->hand[i];
        
        // Destroy old plane
        if (card->plane) {
            ncplane_destroy(card->plane);
        }
        
        // Create face-up card
        card->face_up = true;
        const char* image_path = get_card_image_path(card->rank, card->suit, true);
        struct ncvisual* ncv = ncvisual_from_file(image_path);
        
        if (ncv) {
            struct ncplane_options opts = {
                .y = card->y,
                .x = card->x,
                .rows = card->height,
                .cols = card->width,
                .name = "card",
            };
            
            card->plane = ncplane_create(notcurses_stdplane(game->nc), &opts);
            if (card->plane) {
                struct ncvisual_options vopts = {
                    .n = card->plane,
                    .blitter = NCBLIT_PIXEL,
                    .scaling = NCSCALE_STRETCH,
                };
                
                ncvisual_blit(game->nc, ncv, &vopts);
                move_card_to_layer(card, card->z_index);
            }
            
            ncvisual_destroy(ncv);
        }
        
        render_frame(game);
        usleep(100000);  // 100ms between flips
    }
}

// ============================================================================
// ANIMATION SYSTEM
// ============================================================================

void add_card_animation(PokerGame* game, Card* card, int to_x, int to_y) {
    pthread_mutex_lock(&game->anim_mutex);
    
    if (game->num_animations < 52) {
        CardAnimation* anim = &game->animations[game->num_animations++];
        anim->card = card;
        anim->start_x = card->x;
        anim->start_y = card->y;
        anim->end_x = to_x;
        anim->end_y = to_y;
        anim->current_x = card->x;
        anim->current_y = card->y;
        anim->steps_remaining = CARD_MOVE_STEPS;
    }
    
    pthread_mutex_unlock(&game->anim_mutex);
}

void update_animations(PokerGame* game) {
    pthread_mutex_lock(&game->anim_mutex);
    
    for (int i = 0; i < game->num_animations; i++) {
        CardAnimation* anim = &game->animations[i];
        
        if (anim->steps_remaining > 0) {
            // Smooth easing
            float t = 1.0f - ((float)anim->steps_remaining / CARD_MOVE_STEPS);
            t = t * t * (3.0f - 2.0f * t);  // Smoothstep
            
            anim->current_x = anim->start_x + (anim->end_x - anim->start_x) * t;
            anim->current_y = anim->start_y + (anim->end_y - anim->start_y) * t;
            
            // Update card position
            if (anim->card && anim->card->plane) {
                ncplane_move_yx(anim->card->plane, 
                               (int)anim->current_y, 
                               (int)anim->current_x);
                anim->card->x = (int)anim->current_x;
                anim->card->y = (int)anim->current_y;
            }
            
            anim->steps_remaining--;
        }
    }
    
    // Clean up completed animations
    int write_idx = 0;
    for (int read_idx = 0; read_idx < game->num_animations; read_idx++) {
        if (game->animations[read_idx].steps_remaining > 0) {
            if (write_idx != read_idx) {
                game->animations[write_idx] = game->animations[read_idx];
            }
            write_idx++;
        }
    }
    game->num_animations = write_idx;
    
    pthread_mutex_unlock(&game->anim_mutex);
}

bool animations_complete(PokerGame* game) {
    pthread_mutex_lock(&game->anim_mutex);
    bool complete = (game->num_animations == 0);
    pthread_mutex_unlock(&game->anim_mutex);
    return complete;
}

// ============================================================================
// RENDERING
// ============================================================================

void render_frame(PokerGame* game) {
    if (game->test_mode) {
        game->frame_counter++;
    }
    notcurses_render(game->nc);
}

// ============================================================================
// GAME INITIALIZATION
// ============================================================================

void init_game(PokerGame* game, struct notcurses* nc) {
    memset(game, 0, sizeof(PokerGame));
    
    game->nc = nc;
    notcurses_stddim_yx(nc, &game->term_height, &game->term_width);
    
    pthread_mutex_init(&game->anim_mutex, NULL);
    
    // Initialize game state
    game->dealer_position = rand() % MAX_PLAYERS;
    game->pot = 0;
    game->current_bet = 100;  // Big blind
    
    // Create visual elements
    create_table(game);
    position_players(game);
    
    // Create player info boxes
    for (int i = 0; i < MAX_PLAYERS; i++) {
        create_player_info_box(game, i);
    }
    
    // Header
    struct ncplane* std = notcurses_stdplane(nc);
    uint64_t header_color = 0;
    ncchannels_set_fg_rgb(&header_color, COLOR_GOLD);
    ncchannels_set_bg_alpha(&header_color, NCALPHA_TRANSPARENT);
    ncplane_set_channels(std, header_color);
    ncplane_printf_aligned(std, 0, NCALIGN_CENTER, 
                          "═══ PROFESSIONAL 10-PLAYER 2-7 LOWBALL ═══");
    
    // Controls
    uint64_t ctrl_color = 0;
    ncchannels_set_fg_rgb(&ctrl_color, 0x808080);
    ncchannels_set_bg_alpha(&ctrl_color, NCALPHA_TRANSPARENT);
    ncplane_set_channels(std, ctrl_color);
    ncplane_printf_yx(std, game->term_height - 1, 2,
                     "[Q]uit • [N]ew Hand • [F]old • [C]all • [R]aise");
}

void cleanup_game(PokerGame* game) {
    // Clean up all cards
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game->players[i];
        for (int j = 0; j < p->num_cards; j++) {
            destroy_card(&p->hand[j]);
        }
        if (p->info_box) ncplane_destroy(p->info_box);
        if (p->bet_chips) ncplane_destroy(p->bet_chips);
        if (p->action_text) ncplane_destroy(p->action_text);
    }
    
    // Clean up UI planes
    if (game->table) ncplane_destroy(game->table);
    if (game->pot_display) ncplane_destroy(game->pot_display);
    if (game->dealer_button) ncplane_destroy(game->dealer_button);
    if (game->controls) ncplane_destroy(game->controls);
    
    pthread_mutex_destroy(&game->anim_mutex);
}

// ============================================================================
// TEST FUNCTIONS
// ============================================================================

void test_render_single_frame(PokerGame* game) {
    printf("[TEST] Rendering single frame...\n");
    render_frame(game);
    printf("[TEST] Frame %d rendered\n", game->frame_counter);
}

void test_card_positioning(PokerGame* game) {
    printf("[TEST] Card positioning test\n");
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game->players[i];
        if (p->status != PLAYER_ACTIVE) continue;
        
        printf("  Player %d (%s): pos(%d,%d) cards=%d\n", 
               i, p->name, p->table_x, p->table_y, p->num_cards);
        
        for (int j = 0; j < p->num_cards; j++) {
            Card* c = &p->hand[j];
            printf("    Card %d: pos(%d,%d) size(%dx%d) z=%d\n",
                   j, c->x, c->y, c->width, c->height, c->z_index);
        }
    }
}

void test_animation_frame(PokerGame* game, int frame) {
    printf("[TEST] Animation frame %d\n", frame);
    
    pthread_mutex_lock(&game->anim_mutex);
    printf("  Active animations: %d\n", game->num_animations);
    for (int i = 0; i < game->num_animations; i++) {
        CardAnimation* a = &game->animations[i];
        printf("    Anim %d: pos(%.1f,%.1f) steps=%d\n",
               i, a->current_x, a->current_y, a->steps_remaining);
    }
    pthread_mutex_unlock(&game->anim_mutex);
}

void test_z_ordering(PokerGame* game) {
    printf("[TEST] Z-ordering verification\n");
    
    // Check all cards have unique z-indices
    int z_indices[200] = {0};
    int max_z = 0;
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game->players[i];
        for (int j = 0; j < p->num_cards; j++) {
            int z = p->hand[j].z_index;
            if (z > 0) {
                z_indices[z]++;
                if (z > max_z) max_z = z;
            }
        }
    }
    
    printf("  Max z-index: %d\n", max_z);
    for (int i = Z_CARDS_BASE; i <= max_z; i++) {
        if (z_indices[i] > 1) {
            printf("  WARNING: Z-index %d has %d cards!\n", i, z_indices[i]);
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
    // Check for test mode
    bool test_mode = (argc > 1 && strcmp(argv[1], "--test") == 0);
    
    // Initialize notcurses
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_NO_CLEAR_BITMAPS,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    // Check pixel support
    if (!notcurses_canpixel(nc)) {
        notcurses_stop(nc);
        fprintf(stderr, "This demo requires pixel support (kitty, iTerm2, WezTerm)\n");
        return 1;
    }
    
    // Initialize game
    PokerGame game;
    init_game(&game, nc);
    game.test_mode = test_mode;
    
    // Initial render
    render_frame(&game);
    
    if (test_mode) {
        // Run tests
        test_card_positioning(&game);
        test_z_ordering(&game);
        sleep(1);
        
        // Deal and test
        deal_cards(&game);
        test_animation_frame(&game, 1);
        
        while (!animations_complete(&game)) {
            update_animations(&game);
            render_frame(&game);
            test_animation_frame(&game, game.frame_counter);
            usleep(FRAME_DELAY_MS * 1000);
        }
        
        test_card_positioning(&game);
        test_z_ordering(&game);
        
        sleep(3);
    } else {
        // Normal game mode
        sleep(1);
        
        // Post blinds
        game.pot = 150;  // SB + BB
        update_pot_display(&game);
        render_frame(&game);
        
        // Deal cards
        deal_cards(&game);
        
        // Game loop
        struct ncinput ni;
        bool running = true;
        
        while (running) {
            if (notcurses_get_nblock(nc, &ni) > 0) {
                switch (ni.id) {
                    case 'q':
                    case 'Q':
                        running = false;
                        break;
                        
                    case 'n':
                    case 'N':
                        // New hand
                        collect_cards(&game);
                        game.dealer_position = (game.dealer_position + 1) % MAX_PLAYERS;
                        for (int i = 0; i < MAX_PLAYERS; i++) {
                            create_player_info_box(&game, i);
                        }
                        game.pot = 150;
                        update_pot_display(&game);
                        render_frame(&game);
                        sleep(1);
                        deal_cards(&game);
                        break;
                }
            }
            
            update_animations(&game);
            render_frame(&game);
            usleep(FRAME_DELAY_MS * 1000);
        }
    }
    
    // Cleanup
    cleanup_game(&game);
    notcurses_stop(nc);
    
    return 0;
}

// ============================================================================
// UTILITY: COLLECT CARDS
// ============================================================================

void collect_cards(PokerGame* game) {
    // Clean up all cards
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game->players[i];
        for (int j = 0; j < p->num_cards; j++) {
            destroy_card(&p->hand[j]);
        }
        p->num_cards = 0;
        p->bet = 0;
        
        // Reset folded players
        if (p->status == PLAYER_FOLDED) {
            p->status = PLAYER_ACTIVE;
        }
    }
    
    // Clear animations
    pthread_mutex_lock(&game->anim_mutex);
    game->num_animations = 0;
    pthread_mutex_unlock(&game->anim_mutex);
}