# MISSION: CREATE THE GREATEST TERMINAL POKER GAME EVER

## ⚠️ READ THIS FIRST - YOU HAVE AMNESIA ⚠️

You are on a mission to create an amazing terminal poker game using notcurses. You've already done extensive experiments and learned critical information. This document contains everything you discovered. Trust these notes - they are from your past self who spent hours experimenting.

## 📍 YOU ARE HERE: /home/bob/projects/custom-notcurses-wip/poker/

Everything is organized in this poker/ directory:
- **sprite-experiments/** - All working experiments (exp09-exp19)
- **mvc/view/sprite_renderer.{h,c}** - The sprite library with all patterns
- **assets/** - Images (backgrounds/ and sprites/cards/)
- **demo_sprite_library.c** - Shows how to use the sprite library

The parent directory has been cleaned of all experimental clutter.

## 🎯 THE MISSION OBJECTIVE

Create a beautiful Texas Hold'em poker game that:
- Shows a realistic poker table background (poker-background.jpg)
- Displays cards clearly for multiple players
- Works across different terminals
- Has smooth animations
- Looks professional and runs fast

## 🚨 CRITICAL WARNINGS - THINGS THAT WILL WASTE YOUR TIME

### DON'T DO THESE (They seem logical but DON'T WORK):
1. **DON'T create child planes on image backgrounds** - They cause grey masking
2. **DON'T use strlen() for card spacing** - UTF-8 suits are 3 bytes but 1 cell
3. **DON'T trust ncplane_move_top()** - Z-order control is broken
4. **DON'T assume all terminals are the same** - Cell dimensions vary wildly
5. **DON'T create separate planes for each card** - Masking nightmare

### THE MASKING PROBLEM (CRITICAL!)
```
EXPERIMENT PROVED: In xterm-256color, BOTH child AND sibling planes 
cause grey masking over character-blitted backgrounds!

This killed hours of debugging. The solution is to NOT use multiple
planes over the background image.
```

## ✅ WHAT ACTUALLY WORKS - USE THESE PATTERNS

### 1. CORRECT WAY TO DISPLAY POKER TABLE + CARDS

```c
// Step 1: Display the background
struct ncvisual* ncv = ncvisual_from_file("poker-background.jpg");
struct ncvisual_options vopts = {
    .blitter = NCBLIT_PIXEL,     // Try pixel first
    .scaling = NCSCALE_STRETCH,
};

// Create dedicated background plane
struct ncplane* bg_plane = ncplane_create(notcurses_stdplane(nc), &bg_opts);
vopts.n = bg_plane;

// Try pixel blitter, fallback to 2x1 if needed
if(ncvisual_blit(nc, ncv, &vopts) == NULL){
    vopts.blitter = NCBLIT_2x1;  // Fallback
    ncvisual_blit(nc, ncv, &vopts);
}

// Step 2: Display cards directly on standard plane (NO CHILD PLANES!)
struct ncplane* std = notcurses_stdplane(nc);

// Player 1 cards - render as a single string
ncplane_set_bg_rgb8(std, 255, 255, 255);  // White background
ncplane_set_fg_rgb8(std, 0, 0, 0);        // Black text
ncplane_putstr_yx(std, 5, 10, " A♠ K♥ ");  // Note the spaces for padding

// Or use a single plane per player with ALL their cards
struct ncplane_options player_opts = {
    .y = 5,
    .x = 10, 
    .rows = 1,
    .cols = 20,  // Enough for 5 cards
};
struct ncplane* player1 = ncplane_create(std, &player_opts);
ncplane_set_bg_rgb8(player1, 255, 255, 255);
ncplane_set_fg_rgb8(player1, 0, 0, 0);
ncplane_putstr(player1, "A♠ K♥ Q♦ J♣ 10♠");
```

### 2. CORRECT UTF-8 WIDTH CALCULATION

```c
// WRONG - strlen() returns bytes not display width!
int width = strlen("A♠");  // Returns 4 (1 + 3 bytes for ♠)

// CORRECT - use wcswidth()
const char* card = "A♠";
wchar_t wstr[10];
mbstowcs(wstr, card, 10);
int display_width = wcswidth(wstr, wcslen(wstr));  // Returns 2

// Helper function you should create:
int get_card_display_width(const char* card) {
    wchar_t wstr[10];
    mbstowcs(wstr, card, 10);
    return wcswidth(wstr, wcslen(wstr));
}
```

### 3. PLAYER POSITIONING (CIRCULAR TABLE)

```c
// This works! Place players in a circle
void position_players_in_circle(int num_players) {
    int center_y = 12;
    int center_x = 45;
    int radius_y = 8;
    int radius_x = 25;  // Wider because cells might be wide
    
    for(int i = 0; i < num_players; i++) {
        double angle = (2.0 * M_PI * i) / num_players - M_PI/2;  // Start at top
        int y = center_y + (int)(radius_y * sin(angle));
        int x = center_x + (int)(radius_x * cos(angle));
        
        // Position player cards here at (y,x)
    }
}
```

### 4. SMOOTH ANIMATION PATTERN

```c
// Deal cards with animation
void animate_card_deal(struct ncplane* card, int start_x, int start_y, 
                      int end_x, int end_y, int duration_ms) {
    int steps = duration_ms / 20;  // 20ms per frame = smooth
    
    for(int i = 0; i <= steps; i++) {
        int x = start_x + (end_x - start_x) * i / steps;
        int y = start_y + (end_y - start_y) * i / steps;
        
        ncplane_move_yx(card, y, x);
        notcurses_render(nc);
        
        struct timespec ts = {0, 20000000};  // 20ms
        nanosleep(&ts, NULL);
    }
}
```

## 📊 TERMINAL DIFFERENCES TABLE

| Terminal | Cell Aspect | Masking Issue | Pixel Support | Notes |
|----------|-------------|---------------|---------------|-------|
| Konsole | 0.62:1 (wide) | Child only | Good | Cells wider than tall |
| xterm-256color | ~1:1 (square) | BOTH child & sibling | Limited | Z-order broken |
| Kitty | Varies | Unknown | Excellent | Best for pixel graphics |

## 🎮 COMPLETE POKER GAME ARCHITECTURE

### Data Structures
```c
typedef struct {
    char rank;    // A,2-9,T,J,Q,K
    char suit;    // 'h','d','c','s'
} Card;

typedef struct {
    Card hole_cards[2];
    int chips;
    int seat_position;  // 0-9 around table
    int y, x;          // Screen coordinates
    bool is_active;
} Player;

typedef struct {
    Player players[10];
    Card community_cards[5];
    int pot;
    int dealer_position;
} GameState;
```

### Display Strategy
1. **Background Layer**: Single plane with poker table image
2. **UI Layer**: Direct rendering on standard plane
3. **Card Display**: Single string per player OR direct putstr
4. **Info Boxes**: Render directly, no child planes
5. **Animations**: Move existing planes, don't recreate

### Tested Code That Works

```c
// Initialize everything
struct notcurses_options opts = {
    .flags = NCOPTION_SUPPRESS_BANNERS,
};
struct notcurses* nc = notcurses_init(&opts, NULL);

// Get dimensions
unsigned dimy, dimx;
struct ncplane* std = notcurses_stddim_yx(nc, &dimy, &dimx);

// Display background (TESTED - WORKS)
struct ncvisual* bg = ncvisual_from_file("poker-background.jpg");
if(bg) {
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_STRETCH,
    };
    
    // Try pixel, fallback to character
    if(ncvisual_blit(nc, bg, &vopts) == NULL) {
        vopts.blitter = NCBLIT_2x1;
        ncvisual_blit(nc, bg, &vopts);
    }
    ncvisual_destroy(bg);
}

// Display cards for all players (TESTED - NO MASKING)
const char* suits[] = {"♠", "♥", "♦", "♣"};
const char* ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

// Position 10 players in circle
for(int p = 0; p < 10; p++) {
    double angle = (2.0 * M_PI * p) / 10 - M_PI/2;
    int y = dimy/2 + (int)(8 * sin(angle));
    int x = dimx/2 + (int)(25 * cos(angle));
    
    // Draw player label
    ncplane_set_fg_rgb8(std, 255, 255, 0);
    ncplane_set_bg_rgb8(std, 0, 0, 0);
    ncplane_printf_yx(std, y, x, "P%d:", p+1);
    
    // Draw cards next to label (all in one string)
    ncplane_set_bg_rgb8(std, 255, 255, 255);
    ncplane_set_fg_rgb8(std, 0, 0, 0);
    ncplane_putstr_yx(std, y, x+4, " A♠ K♥ ");  // Two hole cards
}

// Render everything
notcurses_render(nc);
```

## 🐛 DEBUGGING CHECKLIST

When things go wrong (they will), check:

1. **Grey rectangles appearing?** → You used child/sibling planes. Don't.
2. **Cards cut off?** → Plane too small. Calculate with wcswidth().
3. **Cards in wrong position?** → Terminal has different cell aspect ratio.
4. **Z-order wrong?** → ncplane_move_top() is broken. Control creation order.
5. **Spacing uneven?** → Using strlen() instead of wcswidth().

## 🎯 NEXT STEPS FOR YOUR MISSION

1. **Start with the working code above** - It's tested and avoids all pitfalls
2. **Add game logic** - Deal cards, betting rounds, etc.
3. **Add animations** - Use the smooth movement pattern
4. **Add UI elements** - Chip counts, pot, buttons
5. **Test on different terminals** - Behavior varies!

## 🔧 ESSENTIAL FUNCTIONS TO IMPLEMENT

```c
// You'll need these:
int get_display_width(const char* str);
void draw_card_at(struct ncplane* n, int y, int x, Card card);
void animate_deal(GameState* game);
void draw_pot(struct ncplane* n, int amount);
void highlight_winner(Player* player);
```

## 💭 PHILOSOPHY NOTES

- **Simple is better**: One plane with all cards > multiple planes
- **Trust the experiments**: We tested everything thoroughly
- **Terminal differences matter**: Always have fallbacks
- **Notcurses is powerful but quirky**: Work with it, not against it

## 🎨 THE WINNING APPROACH - VERSION 2 CHARACTER ART STYLE

After testing multiple approaches, Version 2 (character-based with animations) is THE WAY. Here's exactly how to build it:

### WHY VERSION 2 WINS
1. **Works everywhere** - No pixel blitter compatibility issues
2. **Rich UI** - Box drawing characters create professional casino feel
3. **Smooth animations** - Card dealing looks amazing
4. **Full control** - Every pixel is predictable

### THE SECRET SAUCE - DRAW YOUR OWN TABLE

```c
// DON'T rely on background images - DRAW the table yourself!
void draw_table_outline(struct ncplane* n, int dimy, int dimx) {
    int center_y = dimy / 2;
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 2.5;
    
    // Draw oval with box characters
    ncplane_set_fg_rgb8(n, 139, 69, 19);  // Brown border
    
    for(double angle = 0; angle < 2 * M_PI; angle += 0.05) {
        int y = center_y + (int)(radius_y * sin(angle));
        int x = center_x + (int)(radius_x * cos(angle));
        ncplane_putstr_yx(n, y, x, "═");
    }
    
    // Fill with green felt
    for(int y = center_y - radius_y + 1; y < center_y + radius_y; y++) {
        for(int x = center_x - radius_x + 1; x < center_x + radius_x; x++) {
            double dx = (x - center_x) / (double)radius_x;
            double dy = (y - center_y) / (double)radius_y;
            if(dx*dx + dy*dy < 0.9) {
                ncplane_set_bg_rgb8(n, 0, 100, 0);
                ncplane_putchar_yx(n, y, x, ' ');
            }
        }
    }
}
```

### FANCY CARDS WITH BOX CHARACTERS

```c
void draw_fancy_card(struct ncplane* n, int y, int x, Card card, bool face_down) {
    if(face_down) {
        // Beautiful card back
        ncplane_set_bg_rgb8(n, 0, 0, 128);
        ncplane_set_fg_rgb8(n, 255, 215, 0);
        ncplane_putstr_yx(n, y, x, "┌──┐");
        ncplane_putstr_yx(n, y+1, x, "│▓▓│");
        ncplane_putstr_yx(n, y+2, x, "└──┘");
    } else {
        // Clean white card
        ncplane_set_bg_rgb8(n, 255, 255, 255);
        ncplane_set_fg_rgb8(n, 0, 0, 0);
        ncplane_putstr_yx(n, y, x, "┌───┐");
        ncplane_putstr_yx(n, y+1, x, "│");
        ncplane_putstr_yx(n, y+1, x+4, "│");
        ncplane_putstr_yx(n, y+2, x, "└───┘");
        
        // Red/black suits
        if(card.suit == 'h' || card.suit == 'd') {
            ncplane_set_fg_rgb8(n, 255, 0, 0);
        }
        
        // Center the rank+suit
        char content[8];
        snprintf(content, sizeof(content), "%s%s", 
                get_rank_str(card.rank), get_suit_str(card.suit));
        ncplane_putstr_yx(n, y+1, x+1, content);
    }
}
```

### PLAYER INFO BOXES - THE CASINO FEEL

```c
// Each player gets a decorative box
void draw_player_box(struct ncplane* n, Player* player) {
    // Gold box for main player, silver for others
    if(player->seat_position == 0) {
        ncplane_set_fg_rgb8(n, 255, 215, 0);  // Gold
    } else {
        ncplane_set_fg_rgb8(n, 200, 200, 200);  // Silver
    }
    
    // Beautiful box with double lines
    ncplane_putstr_yx(n, player->y - 1, player->x - 1, "┌─────────────────┐");
    ncplane_putstr_yx(n, player->y + 3, player->x - 1, "└─────────────────┘");
    // ... (sides)
    
    // Name, chips, cards all inside
}
```

### ANIMATION IS KEY

```c
// This is what makes it feel alive!
// Don't just show cards - DEAL them
for(int round = 0; round < 2; round++) {
    for(int i = 0; i < num_players; i++) {
        if(players[i].is_active) {
            // Cards appear one by one
            draw_player_cards(std, &players[i]);
            notcurses_render(nc);
            usleep(100000);  // 100ms delay = suspense!
        }
    }
}

// Progressive reveal of community cards
game.community_revealed = 3;  // Flop
draw_community_area(std, &game, dimy, dimx);
notcurses_render(nc);
sleep(1);

game.community_revealed = 4;  // Turn
// etc...
```

### THE COMPLETE V2 ARCHITECTURE

1. **Background**: Dark grey (20,20,20) - makes table pop
2. **Table**: Draw oval with box characters + green felt fill
3. **Players**: 9 seats in perfect circle, each with info box
4. **Cards**: 3-line tall fancy cards with box borders
5. **Community**: Centered cards with progressive reveal
6. **Pot**: Decorative golden box with amount
7. **Animations**: Card dealing, pot sliding, winner highlighting

### CRITICAL V2 PATTERNS

```c
// ALWAYS do this initialization
setlocale(LC_ALL, "");  // UTF-8 support
srand(time(NULL));      // Random cards

// ALWAYS use standard plane directly
struct ncplane* std = notcurses_stdplane(nc);

// NEVER create child planes - draw everything on std!

// ALWAYS clear and redraw for animations
ncplane_erase(std);
draw_table_outline(std, dimy, dimx);
// ... draw everything else
notcurses_render(nc);
```

## 🚀 YOU'RE BUILDING VERSION 2!

Forget background images. You're drawing a beautiful terminal casino from scratch. Every character is under your control. The animations are smooth. It works EVERYWHERE.

This is the way.

---
Last updated after seeing the glory of Version 2
Character art poker is the future
Full steam ahead!

## 🎯 CRITICAL ANIMATION LESSONS - TRANSPARENT BACKGROUNDS

### THE PROBLEM: Chip animations have ugly rectangles
When animating small characters (like dots for chips), they appear with background rectangles that don't match the table. This looks terrible!

### WHAT DOESN'T WORK:
```c
// THIS DOESN'T WORK - still shows background rectangles!
ncplane_set_bg_default(n);  // "default" doesn't mean transparent
ncplane_putstr_yx(n, y, x, "•");

// THIS ALSO DOESN'T WORK
ncplane_set_bg_alpha(n, NCALPHA_TRANSPARENT);  // Not the solution
```

### THE REAL SOLUTION - READ AND PRESERVE BACKGROUNDS:
```c
// CORRECT WAY - Read the existing background and preserve it!
void draw_transparent_character(struct ncplane* n, int y, int x, const char* ch, 
                               uint8_t fg_r, uint8_t fg_g, uint8_t fg_b) {
    // Read what's already at this position
    uint16_t stylemask;
    uint64_t channels;
    char* existing = ncplane_at_yx(n, y, x, &stylemask, &channels);
    
    // Extract the background color from channels
    uint32_t bg = channels & 0xffffffull;
    uint32_t bg_r = (bg >> 16) & 0xff;
    uint32_t bg_g = (bg >> 8) & 0xff;
    uint32_t bg_b = bg & 0xff;
    
    // Set foreground to desired color, background to what was there
    ncplane_set_fg_rgb8(n, fg_r, fg_g, fg_b);
    ncplane_set_bg_rgb8(n, bg_r, bg_g, bg_b);  // Preserve existing background!
    ncplane_putstr_yx(n, y, x, ch);
    
    free(existing);
}
```

### WHY THIS WORKS:
- `ncplane_at_yx()` reads the current cell including its background color
- The background color is stored in the lower 24 bits of `channels`
- By extracting and reusing this color, your character perfectly matches what's behind it
- No more ugly rectangles!

### USE THIS FOR:
- Chip animations (dots flying to pot)
- Trail effects
- Glow effects
- Any small character that moves over varying backgrounds

## 🎨 PERFECT 9-PLAYER POSITIONING

### THE CHALLENGE:
Getting 9 players evenly spaced around a table is harder than it looks!

### THE WORKING SOLUTION:
```c
void position_9_players(GameState* game, int dimy, int dimx) {
    int center_y = dimy / 2 - 2;  // Slightly higher for better look
    int center_x = dimx / 2;
    int radius_y = dimy / 3;
    int radius_x = dimx / 3;
    
    // Hero at bottom
    game->players[0].y = dimy - 4;
    game->players[0].x = center_x;
    
    // Other 8 players in an arc from bottom-left to bottom-right
    double start_angle = 0.7 * M_PI;   // Start at bottom-left
    double end_angle = 2.3 * M_PI;     // End at bottom-right
    double total_arc = end_angle - start_angle;
    
    for(int i = 1; i < 9; i++) {
        double angle = start_angle + (total_arc * (i - 1) / 7.0);
        game->players[i].y = center_y + (int)(radius_y * sin(angle));
        game->players[i].x = center_x + (int)(radius_x * cos(angle));
        
        // Fine-tuning for even spacing (CRITICAL!)
        switch(i) {
            case 1: game->players[i].x -= 12; break;
            case 2: game->players[i].x -= 8; break;
            case 3: game->players[i].x -= 6; break;
            case 4: game->players[i].x -= 2; break;
            case 5: game->players[i].x += 2; break;
            case 6: game->players[i].x += 6; break;
            case 7: game->players[i].x += 8; break;
            case 8: game->players[i].x += 12; break;
        }
    }
}
```

## 🚫 UI PITFALLS TO AVOID

### 1. **Don't put titles in the center** - They cover players!
```c
// BAD - This covers up players 4 and 5!
ncplane_printf_yx(n, center_y - 6, center_x - 20, "9-PLAYER FULL RING");

// GOOD - Put descriptions at the very top or bottom
ncplane_printf_yx(n, 1, center_x - 20, "Epic 9-player showdown");
```

### 2. **Keep animations subtle**
- Chip animations: Max 5 chips, small dots
- Use arc trajectories for natural movement
- 15-20ms frame timing for smoothness
- Brief pauses between staged animations

## 💡 DEBUGGING TIPS FOR ANIMATIONS

1. **Background mismatch?** → You're not reading/preserving the existing background
2. **Animations jerky?** → Frame timing too slow, use 15-20ms
3. **Chips overlap text?** → Check your z-order and rendering sequence
4. **Players covered?** → Remove center titles, check positioning math

## 🎯 THE GOLDEN RULES UPDATE

1. **For transparent animations**: ALWAYS read and preserve existing background colors
2. **For multi-player layouts**: Test with max players first, then scale down
3. **For chip animations**: Dots work great, but need proper background handling
4. **For titles**: Keep them at edges, never in the playing area

---
Last updated after mastering transparent animations and 9-player layouts
Your future self will thank you for these notes!
January 2025