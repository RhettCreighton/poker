// Professional 10-Player Network-Style Poker Game
// Features: Clean UI, proper z-ordering, smooth animations, professional aesthetics

#define _XOPEN_SOURCE 700
#include <notcurses/notcurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <locale.h>
#include <pthread.h>
#include <wchar.h>

// Game constants
#define MAX_PLAYERS 10
#define CARDS_PER_PLAYER 5
#define HERO_SEAT 0
#define STARTING_CHIPS 10000
#define SMALL_BLIND 50
#define BIG_BLIND 100

// Card dimensions
#define HERO_CARD_HEIGHT 12
#define HERO_CARD_WIDTH 8
#define TABLE_CARD_HEIGHT 8
#define TABLE_CARD_WIDTH 5
#define FOLDED_CARD_HEIGHT 6
#define FOLDED_CARD_WIDTH 4

// Animation timing (ms)
#define DEAL_DELAY_MS 150
#define CARD_FLIP_MS 200
#define CHIP_MOVE_MS 500
#define FADE_STEPS 10

// UI Colors - Professional poker site theme
#define COLOR_FELT 0x0B5F0B       // Dark green felt
#define COLOR_TABLE_EDGE 0x8B4513  // Saddle brown
#define COLOR_GOLD 0xFFD700
#define COLOR_CHIP_RED 0xDC143C
#define COLOR_CHIP_GREEN 0x228B22
#define COLOR_CHIP_BLACK 0x2F4F4F
#define COLOR_ACTIVE_GLOW 0x00FF00
#define COLOR_HERO_GLOW 0x4169E1
#define COLOR_POT_GLOW 0xFFA500

typedef enum {
    PLAYER_ACTIVE,
    PLAYER_FOLDED,
    PLAYER_ALL_IN,
    PLAYER_SITTING_OUT,
    PLAYER_EMPTY
} PlayerStatus;

typedef enum {
    ACTION_NONE,
    ACTION_CHECK,
    ACTION_CALL,
    ACTION_BET,
    ACTION_RAISE,
    ACTION_FOLD,
    ACTION_ALL_IN
} PlayerAction;

typedef struct {
    int rank;  // 2-14 (Ace high in lowball)
    int suit;  // 0-3 (C,D,H,S)
    struct ncplane* plane;
    int display_x, display_y;  // Current display position
    int target_x, target_y;    // Animation target
    bool face_up;
    bool is_animating;
} Card;

typedef struct {
    char name[32];
    int seat_number;
    int chips;
    int current_bet;
    Card hand[CARDS_PER_PLAYER];
    int num_cards;
    PlayerStatus status;
    PlayerAction last_action;
    int position_x, position_y;  // Table position
    
    // UI elements
    struct ncplane* info_plane;
    struct ncplane* bet_plane;
    struct ncplane* action_plane;
    bool is_turn;
    int timer_seconds;
} Player;

typedef struct {
    struct ncplane* plane;
    int x, y;
    int amount;
    bool animating;
} ChipStack;

typedef struct {
    struct notcurses* nc;
    
    // Players
    Player players[MAX_PLAYERS];
    int num_active_players;
    int dealer_position;
    int current_player;
    
    // Game state
    int main_pot;
    int current_bet;
    int hand_number;
    bool in_hand;
    
    // UI Planes (ordered by z-index)
    struct ncplane* background_plane;
    struct ncplane* table_plane;
    struct ncplane* pot_plane;
    struct ncplane* dealer_button_plane;
    struct ncplane* community_plane;
    struct ncplane* message_plane;
    
    // Animation system
    pthread_mutex_t render_mutex;
    bool animations_active;
    
    // Table dimensions
    int table_center_x, table_center_y;
    int table_radius_x, table_radius_y;
} PokerGame;

// Forward declarations
void render_game(PokerGame* game);
struct ncplane* create_card_plane(PokerGame* game, Card* card, int y, int x, int height, int width);
void animate_card_deal(PokerGame* game, Card* card, int from_x, int from_y, int to_x, int to_y);

// Get card PNG filename
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

// Create properly sized card with correct z-order
struct ncplane* create_card_plane(PokerGame* game, Card* card, int y, int x, 
                                 int height, int width) {
    const char* image_path = get_card_image_path(card->rank, card->suit, card->face_up);
    
    struct ncvisual* ncv = ncvisual_from_file(image_path);
    if (!ncv) return NULL;
    
    // Use ORCA pattern for sizing
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    struct ncvgeom geom;
    ncvisual_geom(game->nc, ncv, &vopts, &geom);
    
    // Create plane with proper constraints
    struct ncplane_options nopts = {
        .rows = (geom.rcelly > height) ? height : geom.rcelly,
        .cols = (geom.rcellx > width) ? width : geom.rcellx,
        .y = y,
        .x = x,
        .name = "card",
    };
    
    struct ncplane* plane = ncplane_create(notcurses_stdplane(game->nc), &nopts);
    if (!plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    // Render the card
    vopts.n = plane;
    ncvisual_blit(game->nc, ncv, &vopts);
    ncvisual_destroy(ncv);
    
    // Store the plane reference
    card->plane = plane;
    card->display_x = x;
    card->display_y = y;
    
    return plane;
}

// Initialize table with professional appearance
void create_poker_table(PokerGame* game) {
    int dimy, dimx;
    notcurses_stddim_yx(game->nc, &dimy, &dimx);
    
    // Create dark background
    game->background_plane = notcurses_stdplane(game->nc);
    uint64_t bg_channel = 0;
    ncchannels_set_bg_rgb(&bg_channel, 0x0A0A0A);  // Very dark gray
    ncchannels_set_fg_rgb(&bg_channel, 0x1A1A1A);
    ncplane_set_base(game->background_plane, " ", 0, bg_channel);
    
    // Table parameters
    game->table_center_x = dimx / 2;
    game->table_center_y = dimy / 2 - 2;
    game->table_radius_x = (dimx - 20) / 2;
    game->table_radius_y = (dimy - 14) / 2;
    
    // Create table plane
    struct ncplane_options table_opts = {
        .y = 3,
        .x = 5,
        .rows = dimy - 8,
        .cols = dimx - 10,
        .name = "table",
    };
    
    game->table_plane = ncplane_create(game->background_plane, &table_opts);
    if (!game->table_plane) return;
    
    // Draw table with gradient effect
    for (int y = 0; y < ncplane_dim_y(game->table_plane); y++) {
        for (int x = 0; x < ncplane_dim_x(game->table_plane); x++) {
            // Calculate distance from center
            double dy = (y - ncplane_dim_y(game->table_plane)/2) / (double)(ncplane_dim_y(game->table_plane)/2);
            double dx = (x - ncplane_dim_x(game->table_plane)/2) / (double)(ncplane_dim_x(game->table_plane)/2);
            double dist = sqrt(dy * dy + dx * dx);
            
            if (dist <= 1.0) {
                // Inside table oval
                uint64_t felt_channel = 0;
                
                if (dist > 0.9) {
                    // Table edge
                    ncchannels_set_bg_rgb(&felt_channel, COLOR_TABLE_EDGE);
                    ncchannels_set_fg_rgb(&felt_channel, 0x654321);
                    ncplane_set_channels(game->table_plane, felt_channel);
                    ncplane_putchar_yx(game->table_plane, y, x, '█');
                } else {
                    // Felt with subtle gradient
                    int green_value = COLOR_FELT + (int)(20 * (1.0 - dist));
                    ncchannels_set_bg_rgb(&felt_channel, green_value);
                    ncchannels_set_fg_rgb(&felt_channel, green_value);
                    ncplane_set_channels(game->table_plane, felt_channel);
                    ncplane_putchar_yx(game->table_plane, y, x, ' ');
                }
            }
        }
    }
    
    // Add table logo/branding in center
    uint64_t logo_channel = 0;
    ncchannels_set_fg_rgb(&logo_channel, 0x0F7F0F);
    ncchannels_set_bg_alpha(&logo_channel, NCALPHA_TRANSPARENT);
    ncplane_set_channels(game->table_plane, logo_channel);
    
    int center_y = ncplane_dim_y(game->table_plane) / 2;
    int center_x = ncplane_dim_x(game->table_plane) / 2;
    ncplane_putstr_aligned(game->table_plane, center_y - 1, NCALIGN_CENTER, "♠ ♥ ♣ ♦");
    ncplane_putstr_aligned(game->table_plane, center_y, NCALIGN_CENTER, "PROFESSIONAL POKER");
    ncplane_putstr_aligned(game->table_plane, center_y + 1, NCALIGN_CENTER, "♦ ♣ ♥ ♠");
}

// Calculate player positions around oval table
void position_players(PokerGame* game) {
    // Professional seat arrangement for 10 players
    // Seats numbered clockwise from bottom center (hero position)
    double seat_angles[MAX_PLAYERS] = {
        M_PI / 2,           // Seat 0: Bottom center (Hero)
        M_PI / 2 + 0.5,     // Seat 1: Bottom left
        M_PI - 0.4,         // Seat 2: Left
        M_PI + 0.2,         // Seat 3: Top left
        3 * M_PI / 2 - 0.5, // Seat 4: Top left center
        3 * M_PI / 2,       // Seat 5: Top center
        3 * M_PI / 2 + 0.5, // Seat 6: Top right center
        2 * M_PI - 0.2,     // Seat 7: Top right
        0.4,                // Seat 8: Right
        M_PI / 2 - 0.5      // Seat 9: Bottom right
    };
    
    // Professional player names
    const char* bot_names[] = {
        "You", "Mike_M", "Phil_I", "Doyle_B", "Johnny_C",
        "Daniel_N", "Phil_H", "Tom_D", "Gus_H", "Antonio_E"
    };
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game->players[i];
        
        // Initialize player
        strcpy(p->name, bot_names[i]);
        p->seat_number = i;
        p->chips = STARTING_CHIPS + (rand() % 5000) - 2500;
        p->status = (rand() % 10 < 8) ? PLAYER_ACTIVE : PLAYER_SITTING_OUT;
        p->current_bet = 0;
        p->num_cards = 0;
        
        // Calculate position
        double angle = seat_angles[i];
        p->position_x = game->table_center_x + (int)(game->table_radius_x * 0.85 * cos(angle));
        p->position_y = game->table_center_y + (int)(game->table_radius_y * 0.85 * sin(angle));
        
        // Adjust hero position to bottom
        if (i == HERO_SEAT) {
            p->position_y = notcurses_stddim_y(game->nc) - 10;
            p->status = PLAYER_ACTIVE;
        }
    }
    
    game->num_active_players = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status == PLAYER_ACTIVE) {
            game->num_active_players++;
        }
    }
}

// Create player info display box
void create_player_info_box(PokerGame* game, int player_idx) {
    Player* p = &game->players[player_idx];
    
    if (p->status == PLAYER_EMPTY) return;
    
    // Destroy old plane if exists
    if (p->info_plane) {
        ncplane_destroy(p->info_plane);
    }
    
    // Box dimensions
    int box_width = (player_idx == HERO_SEAT) ? 24 : 20;
    int box_height = 4;
    
    // Position box near player
    int box_y = p->position_y - 2;
    int box_x = p->position_x - box_width / 2;
    
    // Keep within screen bounds
    int max_x = notcurses_stddim_x(game->nc);
    int max_y = notcurses_stddim_y(game->nc);
    
    if (box_x < 1) box_x = 1;
    if (box_x + box_width >= max_x - 1) box_x = max_x - box_width - 1;
    if (box_y < 1) box_y = 1;
    if (box_y + box_height >= max_y - 1) box_y = max_y - box_height - 1;
    
    struct ncplane_options opts = {
        .y = box_y,
        .x = box_x,
        .rows = box_height,
        .cols = box_width,
        .name = "player_info",
    };
    
    p->info_plane = ncplane_create(game->background_plane, &opts);
    if (!p->info_plane) return;
    
    // Styling based on status
    uint64_t bg_color = 0;
    uint64_t fg_color = 0;
    uint64_t border_color = 0;
    
    if (player_idx == HERO_SEAT) {
        ncchannels_set_bg_rgb(&bg_color, 0x1A1A50);     // Dark blue
        ncchannels_set_fg_rgb(&fg_color, COLOR_HERO_GLOW);
        ncchannels_set_fg_rgb(&border_color, COLOR_HERO_GLOW);
    } else if (p->status == PLAYER_FOLDED) {
        ncchannels_set_bg_rgb(&bg_color, 0x2A2A2A);     // Dark gray
        ncchannels_set_fg_rgb(&fg_color, 0x808080);
        ncchannels_set_fg_rgb(&border_color, 0x606060);
    } else if (p->is_turn) {
        ncchannels_set_bg_rgb(&bg_color, 0x1A3A1A);     // Dark green
        ncchannels_set_fg_rgb(&fg_color, COLOR_ACTIVE_GLOW);
        ncchannels_set_fg_rgb(&border_color, COLOR_ACTIVE_GLOW);
    } else {
        ncchannels_set_bg_rgb(&bg_color, 0x2A2A2A);     // Dark gray
        ncchannels_set_fg_rgb(&fg_color, 0xFFFFFF);
        ncchannels_set_fg_rgb(&border_color, 0x808080);
    }
    
    // Fill background
    ncplane_set_base(p->info_plane, " ", 0, bg_color);
    
    // Draw border
    ncplane_set_channels(p->info_plane, border_color);
    ncplane_double_box(p->info_plane, 0, border_color, box_height, box_width, 0);
    
    // Player name
    ncplane_set_channels(p->info_plane, fg_color);
    ncplane_printf_aligned(p->info_plane, 0, NCALIGN_CENTER, " %s ", p->name);
    
    // Chip count
    ncplane_printf_aligned(p->info_plane, 1, NCALIGN_CENTER, "$%d", p->chips);
    
    // Status indicators
    if (p->seat_number == game->dealer_position) {
        uint64_t dealer_color = 0;
        ncchannels_set_fg_rgb(&dealer_color, COLOR_GOLD);
        ncchannels_set_bg_alpha(&dealer_color, NCALPHA_TRANSPARENT);
        ncplane_set_channels(p->info_plane, dealer_color);
        ncplane_putstr_yx(p->info_plane, 2, 2, "[D]");
    }
    
    if (p->status == PLAYER_ALL_IN) {
        uint64_t allin_color = 0;
        ncchannels_set_fg_rgb(&allin_color, 0xFF4444);
        ncchannels_set_bg_alpha(&allin_color, NCALPHA_TRANSPARENT);
        ncplane_set_channels(p->info_plane, allin_color);
        ncplane_printf_aligned(p->info_plane, 2, NCALIGN_CENTER, "ALL IN");
    } else if (p->current_bet > 0) {
        ncplane_set_channels(p->info_plane, fg_color);
        ncplane_printf_aligned(p->info_plane, 2, NCALIGN_CENTER, "Bet: $%d", p->current_bet);
    }
    
    // Timer for active player
    if (p->is_turn && p->timer_seconds > 0) {
        uint64_t timer_color = 0;
        if (p->timer_seconds <= 5) {
            ncchannels_set_fg_rgb(&timer_color, 0xFF0000);  // Red when low
        } else {
            ncchannels_set_fg_rgb(&timer_color, 0x00FF00);  // Green
        }
        ncchannels_set_bg_alpha(&timer_color, NCALPHA_TRANSPARENT);
        ncplane_set_channels(p->info_plane, timer_color);
        ncplane_printf_yx(p->info_plane, 2, box_width - 5, "%02d", p->timer_seconds);
    }
}

// Create central pot display
void create_pot_display(PokerGame* game) {
    if (game->pot_plane) {
        ncplane_destroy(game->pot_plane);
    }
    
    struct ncplane_options opts = {
        .y = game->table_center_y - 3,
        .x = game->table_center_x - 12,
        .rows = 3,
        .cols = 24,
        .name = "pot",
    };
    
    game->pot_plane = ncplane_create(game->background_plane, &opts);
    if (!game->pot_plane) return;
    
    // Style the pot display
    uint64_t pot_bg = 0;
    ncchannels_set_bg_alpha(&pot_bg, NCALPHA_TRANSPARENT);
    ncplane_set_base(game->pot_plane, " ", 0, pot_bg);
    
    if (game->main_pot > 0) {
        uint64_t pot_color = 0;
        ncchannels_set_fg_rgb(&pot_color, COLOR_POT_GLOW);
        ncchannels_set_bg_alpha(&pot_color, NCALPHA_TRANSPARENT);
        ncplane_set_channels(game->pot_plane, pot_color);
        
        ncplane_printf_aligned(game->pot_plane, 0, NCALIGN_CENTER, "╔═══════════════╗");
        ncplane_printf_aligned(game->pot_plane, 1, NCALIGN_CENTER, "║ POT: $%-7d║", game->main_pot);
        ncplane_printf_aligned(game->pot_plane, 2, NCALIGN_CENTER, "╚═══════════════╝");
    }
}

// Shuffle and deal cards
void deal_cards(PokerGame* game) {
    // Create deck
    Card deck[52];
    int deck_idx = 0;
    
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 2; rank <= 14; rank++) {
            deck[deck_idx].rank = rank;
            deck[deck_idx].suit = suit;
            deck[deck_idx].face_up = false;
            deck[deck_idx].plane = NULL;
            deck_idx++;
        }
    }
    
    // Fisher-Yates shuffle
    for (int i = 51; i > 0; i--) {
        int j = rand() % (i + 1);
        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
    
    // Dealer position (center of table)
    int dealer_x = game->table_center_x;
    int dealer_y = game->table_center_y;
    
    // Deal cards to active players
    deck_idx = 0;
    for (int card_round = 0; card_round < CARDS_PER_PLAYER; card_round++) {
        for (int seat = 0; seat < MAX_PLAYERS; seat++) {
            // Deal from dealer's left (clockwise)
            int player_idx = (game->dealer_position + 1 + seat) % MAX_PLAYERS;
            Player* p = &game->players[player_idx];
            
            if (p->status != PLAYER_ACTIVE && p->status != PLAYER_ALL_IN) continue;
            
            // Copy card data
            p->hand[p->num_cards] = deck[deck_idx++];
            Card* card = &p->hand[p->num_cards];
            
            // Determine card size
            int card_height = (player_idx == HERO_SEAT) ? HERO_CARD_HEIGHT : TABLE_CARD_HEIGHT;
            int card_width = (player_idx == HERO_SEAT) ? HERO_CARD_WIDTH : TABLE_CARD_WIDTH;
            
            // Calculate card position
            int cards_x_offset = -(CARDS_PER_PLAYER * (card_width + 1)) / 2;
            int card_x = p->position_x + cards_x_offset + (p->num_cards * (card_width + 1));
            int card_y = (player_idx == HERO_SEAT) ? p->position_y - card_height - 5 : p->position_y + 5;
            
            // Create card at dealer position
            card->plane = create_card_plane(game, card, dealer_y, dealer_x, card_height, card_width);
            
            if (card->plane) {
                // Set target for animation
                card->target_x = card_x;
                card->target_y = card_y;
                card->is_animating = true;
                
                // Animate card to player
                animate_card_deal(game, card, dealer_x, dealer_y, card_x, card_y);
                
                p->num_cards++;
                
                // Render and brief pause
                render_game(game);
                usleep(DEAL_DELAY_MS * 1000);
            }
        }
    }
    
    // After dealing, flip hero's cards
    sleep(1);  // Dramatic pause
    
    Player* hero = &game->players[HERO_SEAT];
    for (int i = 0; i < hero->num_cards; i++) {
        Card* card = &hero->hand[i];
        if (card->plane) {
            ncplane_destroy(card->plane);
            card->face_up = true;
            card->plane = create_card_plane(game, card, card->display_y, card->display_x,
                                          HERO_CARD_HEIGHT, HERO_CARD_WIDTH);
        }
        render_game(game);
        usleep(CARD_FLIP_MS * 1000);
    }
}

// Smooth card animation
void animate_card_deal(PokerGame* game, Card* card, int from_x, int from_y, int to_x, int to_y) {
    int steps = 20;
    
    for (int step = 0; step <= steps; step++) {
        float progress = (float)step / steps;
        // Easing function for smooth motion
        progress = progress * progress * (3.0f - 2.0f * progress);
        
        int current_x = from_x + (int)((to_x - from_x) * progress);
        int current_y = from_y + (int)((to_y - from_y) * progress);
        
        if (card->plane) {
            ncplane_move_yx(card->plane, current_y, current_x);
            card->display_x = current_x;
            card->display_y = current_y;
        }
        
        render_game(game);
        usleep(20000);  // 20ms per frame
    }
    
    card->is_animating = false;
}

// Main render function
void render_game(PokerGame* game) {
    pthread_mutex_lock(&game->render_mutex);
    notcurses_render(game->nc);
    pthread_mutex_unlock(&game->render_mutex);
}

// Display game header
void display_header(PokerGame* game) {
    struct ncplane* plane = notcurses_stdplane(game->nc);
    
    uint64_t title_color = 0;
    ncchannels_set_fg_rgb(&title_color, COLOR_GOLD);
    ncchannels_set_bg_alpha(&title_color, NCALPHA_TRANSPARENT);
    ncplane_set_channels(plane, title_color);
    
    ncplane_printf_aligned(plane, 0, NCALIGN_CENTER, 
                          "═══ PROFESSIONAL NETWORK POKER ═══");
    
    ncchannels_set_fg_rgb(&title_color, 0xC0C0C0);
    ncplane_set_channels(plane, title_color);
    ncplane_printf_aligned(plane, 1, NCALIGN_CENTER,
                          "Texas Hold'em NL • 10 Players • $50/$100 Blinds");
}

// Display controls
void display_controls(PokerGame* game) {
    struct ncplane* plane = notcurses_stdplane(game->nc);
    int dimy = notcurses_stddim_y(game->nc);
    
    uint64_t control_color = 0;
    ncchannels_set_fg_rgb(&control_color, 0x808080);
    ncchannels_set_bg_alpha(&control_color, NCALPHA_TRANSPARENT);
    ncplane_set_channels(plane, control_color);
    
    ncplane_printf_yx(plane, dimy - 2, 2, 
                     "[F]old • [C]all • [R]aise • [A]ll-in • [Q]uit");
    ncplane_printf_yx(plane, dimy - 1, 2,
                     "[Space] Check • [N]ew Hand • [Tab] Stats");
}

// Initialize game
void init_game(PokerGame* game, struct notcurses* nc) {
    memset(game, 0, sizeof(PokerGame));
    
    game->nc = nc;
    game->hand_number = 1;
    game->dealer_position = rand() % MAX_PLAYERS;
    pthread_mutex_init(&game->render_mutex, NULL);
    
    // Create visual elements
    create_poker_table(game);
    position_players(game);
    
    // Create player info boxes
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status != PLAYER_EMPTY) {
            create_player_info_box(game, i);
        }
    }
    
    create_pot_display(game);
    display_header(game);
    display_controls(game);
}

// Start new hand
void start_new_hand(PokerGame* game) {
    // Clean up old cards
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game->players[i];
        for (int j = 0; j < p->num_cards; j++) {
            if (p->hand[j].plane) {
                ncplane_destroy(p->hand[j].plane);
                p->hand[j].plane = NULL;
            }
        }
        p->num_cards = 0;
        p->current_bet = 0;
        p->last_action = ACTION_NONE;
        
        // Reset folded players
        if (p->status == PLAYER_FOLDED && p->chips > 0) {
            p->status = PLAYER_ACTIVE;
        }
    }
    
    // Move dealer button
    game->dealer_position = (game->dealer_position + 1) % MAX_PLAYERS;
    while (game->players[game->dealer_position].status == PLAYER_EMPTY ||
           game->players[game->dealer_position].status == PLAYER_SITTING_OUT) {
        game->dealer_position = (game->dealer_position + 1) % MAX_PLAYERS;
    }
    
    // Post blinds
    int sb_pos = (game->dealer_position + 1) % MAX_PLAYERS;
    while (game->players[sb_pos].status != PLAYER_ACTIVE) {
        sb_pos = (sb_pos + 1) % MAX_PLAYERS;
    }
    
    int bb_pos = (sb_pos + 1) % MAX_PLAYERS;
    while (game->players[bb_pos].status != PLAYER_ACTIVE) {
        bb_pos = (bb_pos + 1) % MAX_PLAYERS;
    }
    
    // Deduct blinds
    game->players[sb_pos].current_bet = SMALL_BLIND;
    game->players[sb_pos].chips -= SMALL_BLIND;
    
    game->players[bb_pos].current_bet = BIG_BLIND;
    game->players[bb_pos].chips -= BIG_BLIND;
    
    game->main_pot = SMALL_BLIND + BIG_BLIND;
    game->current_bet = BIG_BLIND;
    
    // Update displays
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status != PLAYER_EMPTY) {
            create_player_info_box(game, i);
        }
    }
    
    create_pot_display(game);
    game->hand_number++;
    game->in_hand = true;
}

// Handle player action
void handle_player_action(PokerGame* game, int player_idx, PlayerAction action) {
    Player* p = &game->players[player_idx];
    
    switch (action) {
        case ACTION_FOLD:
            p->status = PLAYER_FOLDED;
            // Turn cards face down
            for (int i = 0; i < p->num_cards; i++) {
                Card* card = &p->hand[i];
                if (card->plane && card->face_up) {
                    ncplane_destroy(card->plane);
                    card->face_up = false;
                    card->plane = create_card_plane(game, card, card->display_y, card->display_x,
                                                  FOLDED_CARD_HEIGHT, FOLDED_CARD_WIDTH);
                }
            }
            break;
            
        case ACTION_CALL:
            {
                int call_amount = game->current_bet - p->current_bet;
                if (call_amount > p->chips) {
                    call_amount = p->chips;
                    p->status = PLAYER_ALL_IN;
                }
                p->chips -= call_amount;
                p->current_bet += call_amount;
                game->main_pot += call_amount;
            }
            break;
            
        case ACTION_RAISE:
            {
                int raise_to = game->current_bet * 2;  // Min raise
                int raise_amount = raise_to - p->current_bet;
                if (raise_amount >= p->chips) {
                    raise_amount = p->chips;
                    p->status = PLAYER_ALL_IN;
                }
                p->chips -= raise_amount;
                p->current_bet += raise_amount;
                game->main_pot += raise_amount;
                game->current_bet = p->current_bet;
            }
            break;
            
        case ACTION_ALL_IN:
            game->main_pot += p->chips;
            p->current_bet += p->chips;
            if (p->current_bet > game->current_bet) {
                game->current_bet = p->current_bet;
            }
            p->chips = 0;
            p->status = PLAYER_ALL_IN;
            break;
            
        default:
            break;
    }
    
    p->last_action = action;
    create_player_info_box(game, player_idx);
    create_pot_display(game);
}

// Main game
int main(void) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
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
        fprintf(stderr, "This demo requires pixel support. Please use a compatible terminal:\n");
        fprintf(stderr, "  - kitty (recommended)\n");
        fprintf(stderr, "  - iTerm2 (macOS)\n");
        fprintf(stderr, "  - WezTerm\n");
        return 1;
    }
    
    // Initialize game
    PokerGame game;
    init_game(&game, nc);
    
    // Initial render
    render_game(&game);
    sleep(1);
    
    // Start first hand
    start_new_hand(&game);
    render_game(&game);
    sleep(1);
    
    // Deal cards
    deal_cards(&game);
    render_game(&game);
    
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
                    start_new_hand(&game);
                    render_game(&game);
                    sleep(1);
                    deal_cards(&game);
                    break;
                    
                case 'f':
                case 'F':
                    if (game.players[HERO_SEAT].status == PLAYER_ACTIVE) {
                        handle_player_action(&game, HERO_SEAT, ACTION_FOLD);
                        render_game(&game);
                    }
                    break;
                    
                case 'c':
                case 'C':
                    if (game.players[HERO_SEAT].status == PLAYER_ACTIVE) {
                        handle_player_action(&game, HERO_SEAT, ACTION_CALL);
                        render_game(&game);
                    }
                    break;
                    
                case 'r':
                case 'R':
                    if (game.players[HERO_SEAT].status == PLAYER_ACTIVE) {
                        handle_player_action(&game, HERO_SEAT, ACTION_RAISE);
                        render_game(&game);
                    }
                    break;
                    
                case 'a':
                case 'A':
                    if (game.players[HERO_SEAT].status == PLAYER_ACTIVE) {
                        handle_player_action(&game, HERO_SEAT, ACTION_ALL_IN);
                        render_game(&game);
                    }
                    break;
            }
        }
        
        usleep(50000);  // 50ms
    }
    
    // Cleanup
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player* p = &game.players[i];
        for (int j = 0; j < p->num_cards; j++) {
            if (p->hand[j].plane) {
                ncplane_destroy(p->hand[j].plane);
            }
        }
        if (p->info_plane) ncplane_destroy(p->info_plane);
        if (p->bet_plane) ncplane_destroy(p->bet_plane);
        if (p->action_plane) ncplane_destroy(p->action_plane);
    }
    
    if (game.table_plane) ncplane_destroy(game.table_plane);
    if (game.pot_plane) ncplane_destroy(game.pot_plane);
    if (game.dealer_button_plane) ncplane_destroy(game.dealer_button_plane);
    if (game.community_plane) ncplane_destroy(game.community_plane);
    if (game.message_plane) ncplane_destroy(game.message_plane);
    
    pthread_mutex_destroy(&game.render_mutex);
    
    notcurses_stop(nc);
    return 0;
}