/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * EXPERIMENT 11: Card Selection and Highlighting
 * 
 * Test: Interactive card selection for draw phase with visual feedback
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

// Display card with optional highlighting
struct ncplane* display_highlighted_card(struct notcurses* nc, int y, int x, 
                                       const char* filename, bool highlighted, bool selected) {
    
    struct ncvisual* ncv = ncvisual_from_file(filename);
    if (!ncv) return NULL;
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Create plane slightly larger if highlighted for border effect
    int rows = highlighted ? 4 : 3;
    int cols = highlighted ? 6 : 5;
    int offset_y = highlighted ? -1 : 0;
    int offset_x = highlighted ? -1 : 0;
    
    struct ncplane_options nopts = {
        .rows = rows,
        .cols = cols,
        .y = y + offset_y,
        .x = x + offset_x,
        .name = "highlight-card",
    };
    
    struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
    if (!card_plane) {
        ncvisual_destroy(ncv);
        return NULL;
    }
    
    // Draw highlight border if needed
    if (highlighted) {
        struct ncplane* std = notcurses_stdplane(nc);
        
        // Yellow border for highlight
        ncplane_set_fg_rgb8(std, 255, 255, 0);
        ncplane_set_bg_rgb8(std, 50, 50, 0);
        
        // Top and bottom borders
        for (int i = 0; i < cols; i++) {
            ncplane_putstr_yx(std, y - 1, x - 1 + i, "═");
            ncplane_putstr_yx(std, y + 3, x - 1 + i, "═");
        }
        
        // Side borders
        for (int i = 0; i < 3; i++) {
            ncplane_putstr_yx(std, y + i, x - 1, "║");
            ncplane_putstr_yx(std, y + i, x + 5, "║");
        }
        
        // Corners
        ncplane_putstr_yx(std, y - 1, x - 1, "╔");
        ncplane_putstr_yx(std, y - 1, x + 5, "╗");
        ncplane_putstr_yx(std, y + 3, x - 1, "╚");
        ncplane_putstr_yx(std, y + 3, x + 5, "╝");
    }
    
    // Draw selection indicator
    if (selected) {
        struct ncplane* std = notcurses_stdplane(nc);
        ncplane_set_fg_rgb8(std, 255, 100, 100);
        ncplane_set_bg_rgb8(std, 100, 0, 0);
        ncplane_putstr_yx(std, y - 2, x + 1, "✗");
        
        // Move card down slightly when selected
        y += 1;
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
    ncplane_putstr_aligned(std, 1, NCALIGN_CENTER, "EXPERIMENT 11: Card Selection & Highlighting");
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
        
        // Animate pulse effect on highlighted card
        animate_pulse(nc, cards, 5, highlight_index);
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
    printf("This UI pattern can be used for the draw phase in poker.\n");
    
    return 0;
}