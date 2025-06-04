/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <time.h>
#include <math.h>

#define CARD_SPRITES_PATH "assets/sprites/cards/"
#define BACKGROUND_PATH "assets/backgrounds/poker-background.jpg"

typedef struct {
    const char* rank;
    const char* suit;
    const char* filename;
} CardInfo;

// All available cards
CardInfo all_cards[] = {
    // Spades
    {"A", "s", "spadeAce.png"}, {"2", "s", "spade2.png"}, {"3", "s", "spade3.png"},
    {"4", "s", "spade4.png"}, {"5", "s", "spade5.png"}, {"6", "s", "spade6.png"},
    {"7", "s", "spade7.png"}, {"8", "s", "spade8.png"}, {"9", "s", "spade9.png"},
    {"T", "s", "spade10.png"}, {"J", "s", "spadeJack.png"}, {"Q", "s", "spadeQueen.png"},
    {"K", "s", "spadeKing.png"},
    // Hearts
    {"A", "h", "heartAce.png"}, {"2", "h", "heart2.png"}, {"3", "h", "heart3.png"},
    {"4", "h", "heart4.png"}, {"5", "h", "heart5.png"}, {"6", "h", "heart6.png"},
    {"7", "h", "heart7.png"}, {"8", "h", "heart8.png"}, {"9", "h", "heart9.png"},
    {"T", "h", "heart10.png"}, {"J", "h", "heartJack.png"}, {"Q", "h", "heartQueen.png"},
    {"K", "h", "heartKing.png"},
    // Diamonds
    {"A", "d", "diamondAce.png"}, {"2", "d", "diamond2.png"}, {"3", "d", "diamond3.png"},
    {"4", "d", "diamond4.png"}, {"5", "d", "diamond5.png"}, {"6", "d", "diamond6.png"},
    {"7", "d", "diamond7.png"}, {"8", "d", "diamond8.png"}, {"9", "d", "diamond9.png"},
    {"T", "d", "diamond10.png"}, {"J", "d", "diamondJack.png"}, {"Q", "d", "diamondQueen.png"},
    {"K", "d", "diamondKing.png"},
    // Clubs
    {"A", "c", "clubAce.png"}, {"2", "c", "club2.png"}, {"3", "c", "club3.png"},
    {"4", "c", "club4.png"}, {"5", "c", "club5.png"}, {"6", "c", "club6.png"},
    {"7", "c", "club7.png"}, {"8", "c", "club8.png"}, {"9", "c", "club9.png"},
    {"T", "c", "club10.png"}, {"J", "c", "clubJack.png"}, {"Q", "c", "clubQueen.png"},
    {"K", "c", "clubKing.png"},
};

// Animation state
typedef struct {
    struct ncplane* card_planes[52];
    int num_cards;
    bool dealing;
    int deal_index;
    int frame;
} AnimState;

// Load and display background using pixel blitting
struct ncplane* create_pixel_background(struct notcurses* nc) {
    printf("Loading background: %s\n", BACKGROUND_PATH);
    
    struct ncvisual* ncv = ncvisual_from_file(BACKGROUND_PATH);
    if (!ncv) {
        printf("Failed to load background image\n");
        return NULL;
    }
    
    // Get terminal dimensions
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Create visual options for pixel blitting
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
        .flags = NCVISUAL_OPTION_NODEGRADE,
    };
    
    // Get geometry info
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    printf("Background geometry: %dx%d pixels, scale %d/%d\n", 
           geom.pixx, geom.pixy, geom.scalex, geom.scaley);
    
    // Create a plane for the background
    struct ncplane_options nopts = {
        .rows = dimy,
        .cols = dimx,
        .name = "background",
    };
    struct ncplane* bg_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    
    if (!bg_plane) {
        printf("Failed to create background plane\n");
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    // Render to the plane
    vopts.n = bg_plane;
    struct ncplane* result = ncvisual_blit(nc, ncv, &vopts);
    
    if (!result) {
        printf("Failed to blit background\n");
        ncplane_destroy(bg_plane);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    ncvisual_destroy(ncv);
    printf("Background loaded successfully\n");
    return bg_plane;
}

// Display a single card using pixel blitting (orca demo style)
struct ncplane* display_card_pixel(struct notcurses* nc, const char* filename, 
                                  int y, int x, int max_height, int max_width) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", CARD_SPRITES_PATH, filename);
    
    struct ncvisual* ncv = ncvisual_from_file(filepath);
    if (!ncv) {
        printf("Failed to load card: %s\n", filepath);
        return NULL;
    }
    
    // Set up visual options like orca demo
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Get geometry like orca demo
    struct ncvgeom geom;
    ncvisual_geom(nc, ncv, &vopts, &geom);
    
    // Use orca's sizing logic - let notcurses calculate optimal cell count
    struct ncplane_options nopts = {
        .rows = geom.rcelly > max_height ? max_height : geom.rcelly,
        .cols = geom.rcellx > max_width ? max_width : geom.rcellx,
        .y = y,
        .x = x,
        .name = "card",
    };
    
    struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    // Render like orca demo
    vopts.n = card_plane;
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    ncvisual_destroy(ncv);
    return card_plane;
}

// Show all cards in a grid
void showcase_all_cards(struct notcurses* nc, AnimState* state) {
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Calculate optimal grid to fit all 52 cards as large as possible
    // Try different grid layouts and pick the one that gives biggest cards
    
    int best_height = 0;
    int best_width = 0;
    int best_cols = 0;
    int best_rows = 0;
    
    // Test grid layouts from 6x9 to 13x4
    for (int cols = 6; cols <= 13; cols++) {
        int rows = (52 + cols - 1) / cols;  // Ceiling division
        
        // Calculate max card size for this layout
        int available_height = dimy - 4;  // Leave room for title/instructions
        int available_width = dimx - 4;   // Leave room for margins
        
        int card_height = available_height / rows;
        int card_width = available_width / cols;
        
        // Maintain royal flush aspect ratio (13:8)
        int constrained_width = (card_height * 13) / 8;
        if (constrained_width > card_width) {
            // Width constrained
            card_height = (card_width * 8) / 13;
        } else {
            // Height constrained
            card_width = constrained_width;
        }
        
        // Pick layout that gives biggest cards (by area)
        if (card_height * card_width > best_height * best_width) {
            best_height = card_height;
            best_width = card_width;
            best_cols = cols;
            best_rows = rows;
        }
    }
    
    int max_card_height = best_height;
    int max_card_width = best_width;
    int cards_per_row = best_cols;
    
    // Center the grid
    int total_grid_width = cards_per_row * max_card_width;
    int total_grid_height = best_rows * max_card_height;
    int start_y = (dimy - total_grid_height) / 2;
    int start_x = (dimx - total_grid_width) / 2;
    
    state->num_cards = 0;
    
    for (int i = 0; i < 52; i++) {
        int row = i / cards_per_row;
        int col = i % cards_per_row;
        
        int y = start_y + row * max_card_height;
        int x = start_x + col * max_card_width;
        
        state->card_planes[i] = display_card_pixel(nc, all_cards[i].filename,
                                                  y, x, max_card_height, max_card_width);
        if (state->card_planes[i]) {
            state->num_cards++;
        }
    }
}

// Animate cards flying to center
void animate_cards_to_center(struct notcurses* nc, AnimState* state) {
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    
    // Move all cards toward center
    for (int i = 0; i < 52; i++) {
        if (!state->card_planes[i]) continue;
        
        int cur_y, cur_x;
        ncplane_yx(state->card_planes[i], &cur_y, &cur_x);
        
        // Calculate movement
        int dy = (center_y - cur_y) / 10;
        int dx = (center_x - cur_x) / 10;
        
        if (abs(dy) > 0 || abs(dx) > 0) {
            ncplane_move_yx(state->card_planes[i], cur_y + dy, cur_x + dx);
        }
    }
}

// Display a poker hand showcase
void showcase_poker_hand(struct notcurses* nc) {
    unsigned dimy, dimx;
    notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Royal flush in hearts
    const char* royal_flush[] = {
        "heart10.png", "heartJack.png", "heartQueen.png", 
        "heartKing.png", "heartAce.png"
    };
    
    // Use orca-style sizing for royal flush
    int max_card_height = dimy / 3;  // Big cards for royal flush
    int max_card_width = dimx / 6;   // Room for 5 cards
    
    int spacing = 2;
    int total_width = 5 * max_card_width + 4 * spacing;
    int start_x = (dimx - total_width) / 2;
    int y = dimy / 2 - max_card_height / 2;
    
    // Display the royal flush - let notcurses handle proper proportions
    for (int i = 0; i < 5; i++) {
        int x = start_x + i * (max_card_width + spacing);
        display_card_pixel(nc, royal_flush[i], y, x, max_card_height, max_card_width);
    }
    
    // Add label
    struct ncplane_options label_opts = {
        .rows = 2,
        .cols = 30,
        .y = y - 3,
        .x = dimx/2 - 15,
    };
    struct ncplane* label = ncplane_create(notcurses_stdplane(nc), &label_opts);
    ncplane_set_bg_alpha(label, NCALPHA_TRANSPARENT);
    ncplane_set_fg_rgb8(label, 255, 215, 0);
    ncplane_putstr_aligned(label, 0, NCALIGN_CENTER, "♠ ROYAL FLUSH ♠");
    ncplane_set_fg_rgb8(label, 200, 200, 200);
    ncplane_putstr_aligned(label, 1, NCALIGN_CENTER, "The Best Hand in Poker!");
}

int main(void) {
    setlocale(LC_ALL, "");
    srand(time(NULL));
    
    printf("=== Poker Pixel Blitting Showcase ===\n");
    printf("Initializing notcurses...\n");
    
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_NO_ALTERNATE_SCREEN,
    };
    
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }
    
    // Check pixel support
    printf("Checking pixel support...\n");
    if (!notcurses_canpixel(nc)) {
        printf("\n*** PIXEL BLITTING NOT SUPPORTED ***\n");
        printf("Your terminal (%s) doesn't support pixel graphics.\n", getenv("TERM"));
        printf("\nFor pixel blitting, use one of these terminals:\n");
        printf("  - kitty (recommended)\n");
        printf("  - iTerm2 (macOS)\n");
        printf("  - WezTerm\n");
        printf("  - mlterm\n");
        printf("  - Any Sixel-capable terminal\n");
        printf("\nFalling back to demonstration mode...\n\n");
        
        // Show what would be displayed
        struct ncplane* std = notcurses_stdplane(nc);
        ncplane_erase(std);
        
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_putstr_yx(std, 2, 2, "PIXEL BLITTING DEMO (Simulation Mode)");
        
        ncplane_set_fg_rgb8(std, 200, 200, 200);
        ncplane_putstr_yx(std, 4, 2, "This demo would show:");
        ncplane_putstr_yx(std, 5, 4, "• Poker table background image");
        ncplane_putstr_yx(std, 6, 4, "• All 52 playing cards with pixel-perfect rendering");
        ncplane_putstr_yx(std, 7, 4, "• Smooth animations");
        ncplane_putstr_yx(std, 8, 4, "• Card dealing effects");
        
        ncplane_set_fg_rgb8(std, 255, 255, 100);
        ncplane_putstr_yx(std, 10, 2, "Card Assets Available:");
        
        // Show some card names
        for (int i = 0; i < 8 && i < sizeof(all_cards)/sizeof(all_cards[0]); i++) {
            ncplane_set_fg_rgb8(std, 150, 150, 255);
            ncplane_printf_yx(std, 12 + i, 4, "%s%s: %s", 
                            all_cards[i].rank, all_cards[i].suit, all_cards[i].filename);
        }
        
        ncplane_set_fg_rgb8(std, 100, 255, 100);
        ncplane_putstr_yx(std, 22, 2, "Press any key to exit...");
        
        notcurses_render(nc);
        
        struct ncinput ni;
        notcurses_get_blocking(nc, &ni);
        
        notcurses_stop(nc);
        return 0;
    }
    
    printf("Pixel support confirmed!\n");
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Clear screen
    ncplane_erase(std);
    
    // Create background
    printf("Creating background...\n");
    struct ncplane* bg = create_pixel_background(nc);
    if (!bg) {
        // Fallback to solid color
        ncplane_set_bg_rgb8(std, 0, 50, 0);
        for (int y = 0; y < dimy; y++) {
            for (int x = 0; x < dimx; x++) {
                ncplane_putchar_yx(std, y, x, ' ');
            }
        }
    }
    
    // Initialize animation state
    AnimState state = {0};
    
    // Title
    struct ncplane_options title_opts = {
        .rows = 3,
        .cols = dimx,
        .y = 0,
        .x = 0,
    };
    struct ncplane* title = ncplane_create(std, &title_opts);
    ncplane_set_bg_alpha(title, NCALPHA_TRANSPARENT);
    ncplane_set_fg_rgb8(title, 255, 215, 0);
    ncplane_putstr_aligned(title, 0, NCALIGN_CENTER, "═══ POKER PIXEL SHOWCASE ═══");
    ncplane_set_fg_rgb8(title, 200, 200, 255);
    ncplane_putstr_aligned(title, 1, NCALIGN_CENTER, "High-Resolution Card Rendering with NCBLIT_PIXEL");
    
    // Instructions
    struct ncplane_options inst_opts = {
        .rows = 2,
        .cols = dimx,
        .y = dimy - 2,
        .x = 0,
    };
    struct ncplane* inst = ncplane_create(std, &inst_opts);
    ncplane_set_bg_alpha(inst, NCALPHA_TRANSPARENT);
    ncplane_set_fg_rgb8(inst, 200, 200, 200);
    ncplane_putstr_aligned(inst, 0, NCALIGN_CENTER, 
                          "[1] All Cards  [2] Royal Flush  [3] Animate  [Q] Quit");
    
    notcurses_render(nc);
    
    // Main loop
    int mode = 0;
    struct ncinput ni;
    
    while (true) {
        // Non-blocking input
        if (notcurses_get_nblock(nc, &ni) > 0) {
            if (ni.id == 'q' || ni.id == 'Q') break;
            
            // Clean up previous cards
            for (int i = 0; i < state.num_cards; i++) {
                if (state.card_planes[i]) {
                    ncplane_destroy(state.card_planes[i]);
                    state.card_planes[i] = NULL;
                }
            }
            state.num_cards = 0;
            
            if (ni.id == '1') {
                mode = 1;
                showcase_all_cards(nc, &state);
            } else if (ni.id == '2') {
                mode = 2;
                showcase_poker_hand(nc);
            } else if (ni.id == '3') {
                mode = 3;
                showcase_all_cards(nc, &state);
                state.dealing = true;
                state.frame = 0;
            }
            
            notcurses_render(nc);
        }
        
        // Animation update
        if (mode == 3 && state.dealing) {
            animate_cards_to_center(nc, &state);
            notcurses_render(nc);
            
            state.frame++;
            if (state.frame > 100) {
                state.dealing = false;
            }
            
            usleep(20000); // 20ms per frame
        } else {
            usleep(50000); // 50ms when not animating
        }
    }
    
    // Cleanup
    for (int i = 0; i < state.num_cards; i++) {
        if (state.card_planes[i]) {
            ncplane_destroy(state.card_planes[i]);
        }
    }
    
    notcurses_stop(nc);
    printf("\nShowcase complete!\n");
    
    return 0;
}