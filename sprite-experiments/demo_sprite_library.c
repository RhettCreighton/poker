/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SPRITE LIBRARY DEMO
 * 
 * Shows how to use the sprite_renderer library for future experiments
 * This is the reference implementation for all sprite operations
 */

#include "../mvc/view/sprite_renderer.h"
#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .loglevel = NCLOGLEVEL_WARNING,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    struct ncplane* std = notcurses_stdplane(nc);
    unsigned dimy, dimx;
    ncplane_dim_yx(std, &dimy, &dimx);
    
    // Initialize sprite system
    sprite_renderer_init();
    
    // 1. Render poker background (CRITICAL: dedicated plane!)
    printf("Loading poker background...\n");
    struct ncplane* bg_plane = render_poker_background(nc);
    if (!bg_plane) {
        printf("Note: poker-background.jpg not found in assets/backgrounds/\n");
        // Fallback
        ncplane_set_bg_rgb8(std, 20, 20, 20);
        for(int y = 0; y < dimy; y++) {
            for(int x = 0; x < dimx; x++) {
                ncplane_putchar_yx(std, y, x, ' ');
            }
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_set_bg_rgb8(std, 20, 20, 20);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "SPRITE LIBRARY DEMO");
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Reference implementation for all sprite operations");
    
    notcurses_render(nc);
    sleep(2);
    
    // 2. Render some cards
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 4, 5, "Card rendering examples:");
    
    // Face cards
    render_card_sprite(nc, 6, 10, "Ace", "spade");
    render_card_sprite(nc, 6, 16, "King", "heart");
    render_card_sprite(nc, 6, 22, "Queen", "club");
    render_card_sprite(nc, 6, 28, "Jack", "diamond");
    
    // Card backs
    render_card_back(nc, 6, 40, CARD_BACK_BLUE);
    render_card_back(nc, 6, 46, CARD_BACK_RED);
    
    notcurses_render(nc);
    sleep(2);
    
    // 3. Card selection demo
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 12, 5, "Selection UI (highlighted + selected):");
    
    // Normal card
    render_card_from_file(nc, 14, 10, "assets/sprites/cards/heart2.png");
    
    // Highlighted card
    render_card_with_highlight(nc, 14, 20, "assets/sprites/cards/diamond7.png", 
                             true, false);
    
    // Selected card
    render_card_with_highlight(nc, 14, 30, "assets/sprites/cards/spade10.png", 
                             false, true);
    
    // Both highlighted and selected
    render_card_with_highlight(nc, 14, 40, "assets/sprites/cards/clubQueen.png", 
                             true, true);
    
    notcurses_render(nc);
    sleep(3);
    
    // 4. Chip rendering
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 21, 5, "Chip displays:");
    
    // Unicode circles
    render_chip_stack_circles(std, 23, 10, 2750);
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, 24, 10, "Circles");
    
    // Numeric display
    render_chip_amount(std, 23, 25, 2750);
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, 24, 25, "Numeric");
    
    // Combined
    render_chip_amount(std, 23, 40, 1337);
    ncplane_putstr_yx(std, 23, 47, " ");
    render_chip_stack_circles(std, 23, 48, 1337);
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, 24, 40, "Combined");
    
    notcurses_render(nc);
    sleep(3);
    
    // 5. Animation demo
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 4, 50, "Dealing animation:");
    
    // Show deck position
    struct ncplane* deck = render_card_back(nc, 8, 65, CARD_BACK_BLUE);
    notcurses_render(nc);
    sleep(1);
    
    // Animate dealing to 3 positions
    animate_card_deal(nc, 8, 65, 14, 55, CARD_BACK_RED);
    animate_card_deal(nc, 8, 65, 16, 50, CARD_BACK_RED);
    animate_card_deal(nc, 8, 65, 18, 55, CARD_BACK_RED);
    
    // Final message
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, dimy - 2, 5, "Sprite library ready! See sprite_renderer.h for API. Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    // Cleanup
    sprite_renderer_cleanup();
    notcurses_stop(nc);
    
    printf("\n=== SPRITE LIBRARY USAGE ===\n");
    printf("1. #include \"mvc/view/sprite_renderer.h\"\n");
    printf("2. Call sprite_renderer_init() at start\n");
    printf("3. Always render background with render_poker_background()\n");
    printf("4. Use render_card_sprite() or render_card_back() for cards\n");
    printf("5. Call sprite_renderer_cleanup() at end\n");
    printf("\nSee demo_sprite_library.c for examples!\n");
    
    return 0;
}