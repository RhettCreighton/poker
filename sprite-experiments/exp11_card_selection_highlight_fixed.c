/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 11 (FIXED): Card Selection and Highlighting
 * 
 * Test: Interactive card selection with proper border cleanup
 *       Using tiny (3x5) cards that user prefers
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

typedef struct {
    struct ncplane* plane;
    char filename[256];
    bool selected;
    int index;
    int base_y;
    int base_x;
} CardInfo;

// Clear card area including border space
void clear_card_area(struct ncplane* std, int y, int x) {
    ncplane_set_bg_rgb8(std, 15, 25, 35);
    
    // Clear a 7x7 area to include border and selection indicator
    for (int cy = -2; cy < 5; cy++) {
        for (int cx = -1; cx < 7; cx++) {
            ncplane_putchar_yx(std, y + cy, x + cx, ' ');
        }
    }
}

// Display card with optional highlighting
struct ncplane* display_highlighted_card(struct notcurses* nc, int y, int x, 
                                       const char* filename, bool highlighted, bool selected) {
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // If selected, card moves down
    int card_y = selected ? y + 1 : y;
    int card_x = x;
    
    // Always create standard 3x5 card
    struct ncplane_options nopts = {
        .rows = 3,
        .cols = 5,
        .y = card_y,
        .x = card_x,
        .name = "card",
    };
    
    struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    // Draw highlight border if needed (BEFORE creating card plane)
    if (highlighted) {
        struct ncplane* std = notcurses_stdplane(nc);
        
        // Yellow border for highlight - position it around the card's actual position
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_set_bg_rgb8(std, 50, 50, 0);
        
        // Top and bottom borders
        for (int i = 0; i < 7; i++) {
            ncplane_putstr_yx(std, card_y - 1, card_x - 1 + i, "═");
            ncplane_putstr_yx(std, card_y + 3, card_x - 1 + i, "═");
        }
        
        // Side borders
        for (int i = 0; i < 3; i++) {
            ncplane_putstr_yx(std, card_y + i, card_x - 1, "║");
            ncplane_putstr_yx(std, card_y + i, card_x + 5, "║");
        }
        
        // Corners
        ncplane_putstr_yx(std, card_y - 1, card_x - 1, "╔");
        ncplane_putstr_yx(std, card_y - 1, card_x + 5, "╗");
        ncplane_putstr_yx(std, card_y + 3, card_x - 1, "╚");
        ncplane_putstr_yx(std, card_y + 3, card_x + 5, "╝");
    }
    
    // Draw selection indicator
    if (selected) {
        struct ncplane* std = notcurses_stdplane(nc);
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_set_bg_rgb8(std, 100, 0, 0);
        ncplane_putstr_yx(std, card_y - 2, card_x + 1, "✗");
    }
    
    vopts.n = card_plane;
    
    if (!ncvisual_blit(nc, ncv, &vopts)) {
        ncplane_destroy(card_plane);
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    ncvisual_destroy(ncv);
    return card_plane;
}

// Pulse animation for highlighted card
void animate_pulse(struct notcurses* nc, CardInfo* cards, int count, int highlight_index) {
    static int pulse_frame = 0;
    pulse_frame++;
    
    float pulse = 0.5f + 0.5f * sin(pulse_frame * 0.15f);
    
    // Only redraw the highlighted card with pulse effect
    if (highlight_index >= 0 && highlight_index < count) {
        CardInfo* card = &cards[highlight_index];
        
        // Clear old position
        struct ncplane* std = notcurses_stdplane(nc);
        clear_card_area(std, card->base_y, card->base_x);
        
        // Destroy old plane
        if (card->plane) {
            ncplane_destroy(card->plane);
        }
        
        // Recreate with pulse offset
        int pulse_offset = (int)(pulse * 2);
        card->plane = display_highlighted_card(nc, 
            card->base_y - pulse_offset, 
            card->base_x,
            card->filename, 
            true, 
            card->selected);
    }
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
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 11 (FIXED): Card Selection & Highlighting");
    ncplane_set_fg_rgb8(std, 200, 200, 200);
    ncplane_putstr_aligned(std, 2, NCALIGN_CENTER, "Use arrow keys to navigate, SPACE to select, ENTER to confirm");
    
    // Instructions
    ncplane_set_fg_rgb8(std, 150, 150, 255);
    ncplane_putstr_yx(std, dimy - 3, 5, "Controls: ← → to move, SPACE to select/deselect, ENTER to discard selected, Q to quit");
    
    // Initialize 5 cards
    CardInfo cards[5] = {
        {NULL, "../SVGCards/Decks/Accessible/Horizontal/pngs/heart2.png", false, 0, 10, 10},
        {NULL, "../SVGCards/Decks/Accessible/Horizontal/pngs/diamond7.png", false, 1, 10, 18},
        {NULL, "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeKing.png", false, 2, 10, 26},
        {NULL, "../SVGCards/Decks/Accessible/Horizontal/pngs/club9.png", false, 3, 10, 34},
        {NULL, "../SVGCards/Decks/Accessible/Horizontal/pngs/heart4.png", false, 4, 10, 42},
    };
    
    // Draw initial cards
    for (int i = 0; i < 5; i++) {
        cards[i].plane = display_highlighted_card(nc, cards[i].base_y, cards[i].base_x, 
                                                 cards[i].filename, false, false);
    }
    
    notcurses_render(nc);
    
    int highlight_index = 0;
    bool running = true;
    
    // Selection status
    ncplane_set_fg_rgb8(std, 100, 255, 100);
    ncplane_putstr_yx(std, 16, 10, "Selected cards: None");
    
    while (running) {
        // Non-blocking input with animation
        ncinput ni;
        struct timespec ts = {0, 50000000}; // 50ms timeout for smooth animation
        
        if (notcurses_get(nc, &ts, &ni) > 0) {
            bool needs_redraw = false;
            
            if (ni.id == NCKEY_LEFT && highlight_index > 0) {
                highlight_index--;
                needs_redraw = true;
            } else if (ni.id == NCKEY_RIGHT && highlight_index < 4) {
                highlight_index++;
                needs_redraw = true;
            } else if (ni.id == ' ') {
                // Toggle selection
                cards[highlight_index].selected = !cards[highlight_index].selected;
                needs_redraw = true;
            } else if (ni.id == NCKEY_ENTER || ni.id == '\n') {
                // Discard selected cards (animation)
                ncplane_erase(std);
                ncplane_set_bg_rgb8(std, 15, 25, 35);
                for(int y = 0; y < dimy; y++) {
                    for(int x = 0; x < dimx; x++) {
                        ncplane_putchar_yx(std, y, x, ' ');
                    }
                }
                
                ncplane_set_fg_rgb8(std, 255, 100, 100);
                ncplane_putstr_aligned(std, dimy/2, NCALIGN_CENTER, "Discarding selected cards...");
                notcurses_render(nc);
                
                // Animate selected cards flying away
                for (int frame = 0; frame < 20; frame++) {
                    for (int i = 0; i < 5; i++) {
                        if (cards[i].selected && cards[i].plane) {
                            int offset_y = -(frame * 2);
                            ncplane_move_yx(cards[i].plane, cards[i].base_y + offset_y, cards[i].base_x);
                        }
                    }
                    notcurses_render(nc);
                    struct timespec anim_ts = {0, 20000000}; // 20ms
                    nanosleep(&anim_ts, NULL);
                }
                
                sleep(1);
                running = false;
            } else if (ni.id == 'q' || ni.id == 'Q') {
                running = false;
            }
            
            if (needs_redraw && running) {
                // Clear all card areas first
                for (int i = 0; i < 5; i++) {
                    clear_card_area(std, cards[i].base_y, cards[i].base_x);
                }
                
                // Redraw all cards with new highlight
                for (int i = 0; i < 5; i++) {
                    if (cards[i].plane) {
                        ncplane_destroy(cards[i].plane);
                    }
                    
                    cards[i].plane = display_highlighted_card(nc, 
                        cards[i].base_y, 
                        cards[i].base_x,
                        cards[i].filename, 
                        i == highlight_index, 
                        cards[i].selected);
                }
                
                // Update selection status
                ncplane_set_bg_rgb8(std, 15, 25, 35);
                for(int x = 0; x < dimx; x++) {
                    ncplane_putchar_yx(std, 16, x, ' ');
                }
                
                ncplane_set_fg_rgb8(std, 100, 255, 100);
                ncplane_printf_yx(std, 16, 10, "Selected cards: ");
                
                int selected_count = 0;
                for (int i = 0; i < 5; i++) {
                    if (cards[i].selected) {
                        if (selected_count > 0) ncplane_putstr(std, ", ");
                        ncplane_printf(std, "%d", i + 1);
                        selected_count++;
                    }
                }
                if (selected_count == 0) {
                    ncplane_putstr(std, "None");
                }
            }
        }
        
        // Animate pulse effect on highlighted card (disabled for now to avoid border issues)
        // animate_pulse(nc, cards, 5, highlight_index);
        notcurses_render(nc);
    }
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        if (cards[i].plane) {
            ncplane_destroy(cards[i].plane);
        }
    }
    
    notcurses_stop(nc);
    
    printf("\nCard selection experiment complete!\n");
    printf("Fixed: Border now properly cleans up and follows card position.\n");
    
    return 0;
}