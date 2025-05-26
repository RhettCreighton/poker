/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 12: Card Back Design Testing
 * 
 * Test: Compare different card back designs for visibility and aesthetics
 *       Using tiny (3x5) cards
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>

// Display card back sprite
struct ncplane* display_card_back(struct notcurses* nc, int y, int x, 
                                 const char* filename, const char* label) {
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Create tiny plane (3x5)
    struct ncplane_options nopts = {
        .rows = 3,
        .cols = 5,
        .y = y,
        .x = x,
        .name = "card-back",
    };
    
    struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    vopts.n = card_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    // Add label
    struct ncplane* std = notcurses_stdplane(nc);
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, y + 4, x - strlen(label)/2 + 2, label);
    
    ncvisual_destroy(ncv);
    return card_plane;
}

// Display character-based card back design
void display_char_card_back(struct ncplane* std, int y, int x, 
                           int design_type, const char* label) {
    
    // White background for card
    ncplane_set_bg_rgb8(std, 255, 255, 255);
    
    switch(design_type) {
        case 0: // Classic pattern
            ncplane_set_fg_rgb8(std, 0, 0, 128);
            ncplane_putstr_yx(std, y, x, "┌───┐");
            ncplane_putstr_yx(std, y+1, x, "│▓▓▓│");
            ncplane_putstr_yx(std, y+2, x, "└───┘");
            break;
            
        case 1: // Diamond pattern
            ncplane_set_fg_rgb8(std, 128, 0, 0);
            ncplane_putstr_yx(std, y, x, "┌───┐");
            ncplane_putstr_yx(std, y+1, x, "│◊◊◊│");
            ncplane_putstr_yx(std, y+2, x, "└───┘");
            break;
            
        case 2: // Cross pattern
            ncplane_set_fg_rgb8(std, 0, 100, 0);
            ncplane_putstr_yx(std, y, x, "┌───┐");
            ncplane_putstr_yx(std, y+1, x, "│╳╳╳│");
            ncplane_putstr_yx(std, y+2, x, "└───┘");
            break;
            
        case 3: // Dots pattern
            ncplane_set_fg_rgb8(std, 100, 0, 100);
            ncplane_putstr_yx(std, y, x, "┌───┐");
            ncplane_putstr_yx(std, y+1, x, "│•••│");
            ncplane_putstr_yx(std, y+2, x, "└───┘");
            break;
            
        case 4: // Solid color
            ncplane_set_fg_rgb8(std, 200, 200, 200);
            ncplane_set_bg_rgb8(std, 50, 50, 150);
            ncplane_putstr_yx(std, y, x, "     ");
            ncplane_putstr_yx(std, y+1, x, "     ");
            ncplane_putstr_yx(std, y+2, x, "     ");
            break;
            
        case 5: // Gradient effect (simulated)
            ncplane_set_fg_rgb8(std, 255, 255, 255);
            ncplane_set_bg_rgb8(std, 100, 0, 0);
            ncplane_putstr_yx(std, y, x, "     ");
            ncplane_set_bg_rgb8(std, 150, 0, 0);
            ncplane_putstr_yx(std, y+1, x, "     ");
            ncplane_set_bg_rgb8(std, 200, 0, 0);
            ncplane_putstr_yx(std, y+2, x, "     ");
            break;
    }
    
    // Reset and add label
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_yx(std, y + 4, x - strlen(label)/2 + 2, label);
}

int main(void) {
    setlocale(LC_ALL, "");
    
    struct notcurses_options opts = {
        .loglevel = NCLOGLEVEL_WARNING,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) return 1;
    
    unsigned dimy, dimx;
    struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);
    
    // Dark background
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    for(int y = 0; y < dimy; y++) {
        for(int x = 0; x < dimx; x++) {
            ncplane_putchar_yx(std, y, x, ' ');
        }
    }
    
    // Title
    ncplane_set_fg_rgb8(std, 255, 255, 100);
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 12: Card Back Designs");
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Comparing sprite vs character-based card backs");
    
    // Section 1: Sprite card backs
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 5, 5, "SPRITE BACKS (from SVG cards):");
    
    struct ncplane* planes[2];
    planes[0] = display_card_back(nc, 7, 10, 
        "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png", "Blue Back");
    planes[1] = display_card_back(nc, 7, 20, 
        "../SVGCards/Decks/Accessible/Horizontal/pngs/redBack.png", "Red Back");
    
    // Section 2: Character-based backs
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 13, 5, "CHARACTER BACKS (custom designs):");
    
    display_char_card_back(std, 15, 10, 0, "Classic");
    display_char_card_back(std, 15, 20, 1, "Diamond");
    display_char_card_back(std, 15, 30, 2, "Cross");
    display_char_card_back(std, 15, 40, 3, "Dots");
    display_char_card_back(std, 15, 50, 4, "Solid");
    display_char_card_back(std, 15, 60, 5, "Gradient");
    
    // Section 3: Animation test
    ncplane_set_fg_rgb8(std, 150, 200, 255);
    ncplane_putstr_yx(std, 21, 5, "ANIMATION TEST (flipping cards):");
    
    notcurses_render(nc);
    sleep(2);
    
    // Animate card flips
    const char* face_card = "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png";
    const char* back_card = "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png";
    
    for (int flip = 0; flip < 3; flip++) {
        // Show back
        struct ncplane* test_plane = display_card_back(nc, 23, 20, back_card, "");
        notcurses_render(nc);
        sleep(1);
        
        // Quick flip animation
        for (int i = 0; i < 5; i++) {
            ncplane_destroy(test_plane);
            test_plane = display_card_back(nc, 23, 20 - i, back_card, "");
            notcurses_render(nc);
            struct timespec ts = {0, 50000000}; // 50ms
            nanosleep(&ts, NULL);
        }
        
        ncplane_destroy(test_plane);
        
        // Show face
        test_plane = display_card_back(nc, 23, 15, face_card, "");
        for (int i = 0; i < 5; i++) {
            ncplane_move_yx(test_plane, 23, 15 + i);
            notcurses_render(nc);
            struct timespec ts = {0, 50000000}; // 50ms
            nanosleep(&ts, NULL);
        }
        notcurses_render(nc);
        sleep(1);
        
        ncplane_destroy(test_plane);
    }
    
    // Final message
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, dimy - 3, 5, "Which design is most visible and appealing? Press any key to exit...");
    notcurses_render(nc);
    
    ncinput ni;
    notcurses_get_blocking(nc, &ni);
    
    // Clean up
    for (int i = 0; i < 2; i++) {
        if (planes[i]) ncplane_destroy(planes[i]);
    }
    
    notcurses_stop(nc);
    
    printf("\nCard back design test complete!\n");
    printf("Consider: visibility, aesthetics, and performance.\n");
    
    return 0;
}