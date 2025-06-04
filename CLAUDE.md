# MISSION: CREATE THE GREATEST TERMINAL POKER GAME EVER

## ⚠️ READ THIS FIRST - YOU HAVE AMNESIA ⚠️

You are on a mission to create an amazing terminal poker game using notcurses. You've already done extensive experiments and learned critical information. This document contains everything you discovered. Trust these notes - they are from your past self who spent hours experimenting.

## 📍 PROJECT STRUCTURE

Everything is organized in this poker/ directory:
- **demos/** - Main pixel blitting showcase
- **sprite-experiments/** - All working experiments and tests
- **mvc/view/sprite_renderer.{h,c}** - The sprite library with pixel patterns
- **assets/sprites/cards/** - 52 PNG card images (75x113 pixels from SVGCards)
- **build/demos/** - Built executables (poker_pixel_showcase)

## 🔨 SETUP FOR NEW DEVELOPERS

### Prerequisites
```bash
# Install notcurses development libraries
# Ubuntu/Debian:
sudo apt install libnotcurses-dev libnotcurses3

# Fedora/RHEL:
sudo dnf install notcurses-devel

# Or build from source: https://github.com/dankamongmen/notcurses
```

### Quick Start
```bash
# 1. Clone and build
git clone <repo>
cd poker
./build.sh

# 2. Run pixel showcase (requires pixel-capable terminal)
cd build/demos && ./poker_pixel_showcase

# 3. Try classic demos (work in any terminal)
./poker_demo_27_lowball
./poker_demo_9_player_beautiful
```

### Project Structure
```
poker/
├── assets/sprites/cards/          # 52 PNG card images (75x113 px)
├── demos/                         # Main pixel blitting showcase
├── sprite-experiments/            # All research & test code
├── mvc/view/sprite_renderer.{h,c} # Core pixel blitting library
├── build/demos/                   # Compiled pixel demos
└── CLAUDE.md                      # THIS FILE - read it!
```

## 🎯 THE MISSION OBJECTIVE

Create a beautiful Texas Hold'em poker game that:
- Shows pixel-perfect card images using NCBLIT_PIXEL
- Displays all 52 PNG card assets from SVGCards
- Works with pixel-capable terminals (kitty, iTerm2, WezTerm)
- Has proper card aspect ratios and sizing
- Looks professional with high-resolution graphics

## 🎨 PIXEL BLITTING BREAKTHROUGH - THE NEW APPROACH

**MAJOR UPDATE: We've moved from character-based rendering to pixel blitting!**

### What We Achieved:
- ✅ Integrated 52 high-quality PNG card images (75x113 pixels each)
- ✅ NCBLIT_PIXEL rendering following notcurses orca demo patterns
- ✅ Automatic optimal card sizing and grid layouts
- ✅ Perfect aspect ratios using notcurses geometry calculations
- ✅ Working pixel showcase demo: `./build/demos/poker_pixel_showcase`

### Terminal Requirements:
**PIXEL SUPPORT REQUIRED**: The new approach needs pixel-capable terminals:
- **kitty** (recommended)
- **iTerm2** (macOS)
- **WezTerm**
- **mlterm**
- Any Sixel-capable terminal

Check with: `notcurses_canpixel(nc)`

## 🚨 CRITICAL LESSONS LEARNED

### 1. ASPECT RATIO IS EVERYTHING
- **Card images are 75x113 pixels** (aspect ratio 0.664:1)
- **Don't manually calculate aspect ratios** - use `geom.rcellx` and `geom.rcelly`
- **Royal flush looked good at 13x8 cells** (aspect 1.625:1)
- **Too narrow constraints = tall skinny cards**
- **Too wide constraints = fat wide cards**

### 2. THE ORCA DEMO IS THE GOLD STANDARD
- **Copy notcurses/src/demo/intro.c orcashow() function exactly**
- **Use `ncvisual_geom()` to get optimal dimensions**
- **Set max constraints, let notcurses choose optimal size within them**
- **This automatically handles terminal differences**

### 3. PIXEL BLITTING REQUIREMENTS
- **Must check `notcurses_canpixel(nc)` first**
- **Gracefully fail to fallback or show message**
- **xterm-256color doesn't support pixels**
- **kitty, iTerm2, WezTerm do support pixels**

## ✅ PIXEL BLITTING PATTERNS - THE WORKING SOLUTION

### 1. THE ORCA DEMO PATTERN (FOLLOW THIS EXACTLY!)

```c
// Load card image
struct ncvisual* ncv = ncvisual_from_file("assets/sprites/cards/spadeAce.png");
if (!ncv) return NULL;

// Set up visual options like orca demo
struct ncvisual_options vopts = {
    .blitter = NCBLIT_PIXEL,
    .scaling = NCSCALE_STRETCH,
};

// Get geometry like orca demo - THIS IS THE KEY!
struct ncvgeom geom;
ncvisual_geom(nc, ncv, &vopts, &geom);

// Use orca's sizing logic - let notcurses calculate optimal cell count
int max_height = dimy / 10;  // Your constraint
int max_width = dimx / 15;   // Your constraint
struct ncplane_options nopts = {
    .rows = geom.rcelly > max_height ? max_height : geom.rcelly,  // CRITICAL!
    .cols = geom.rcellx > max_width ? max_width : geom.rcellx,    // CRITICAL!
    .y = y,
    .x = x,
    .name = "card",
};

struct ncplane* card_plane = ncplane_create(notcurses_stdplane(nc), &nopts);
vopts.n = card_plane;
ncvisual_blit(nc, ncv, &vopts);  // Perfect aspect ratio!
```

### 2. OPTIMAL CARD GRID SIZING

```c
// Algorithm to maximize card size while fitting all 52 cards
int best_height = 0, best_width = 0, best_cols = 0, best_rows = 0;

// Test different grid layouts (6x9, 7x8, 8x7, etc.)
for (int cols = 6; cols <= 13; cols++) {
    int rows = (52 + cols - 1) / cols;  // Ceiling division
    
    int available_height = dimy - 4;  // Leave room for UI
    int available_width = dimx - 4;
    
    int card_height = available_height / rows;
    int card_width = available_width / cols;
    
    // Maintain good aspect ratio (13:8 from testing)
    int constrained_width = (card_height * 13) / 8;
    if (constrained_width > card_width) {
        card_height = (card_width * 8) / 13;
    } else {
        card_width = constrained_width;
    }
    
    // Pick layout that gives biggest total card area
    if (card_height * card_width > best_height * best_width) {
        best_height = card_height;
        best_width = card_width;
        best_cols = cols;
        best_rows = rows;
    }
}
// Use best_height, best_width as max constraints in orca pattern
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

## 📊 TERMINAL PIXEL SUPPORT TABLE

| Terminal | Pixel Support | Method | Performance | Notes |
|----------|---------------|--------|-------------|-------|
| kitty | ✅ Excellent | kitty protocol | Fast | Best choice for pixel blitting |
| iTerm2 | ✅ Good | kitty protocol | Good | macOS users |
| WezTerm | ✅ Good | kitty protocol | Good | Cross-platform |
| mlterm | ✅ Limited | Sixel | Slow | Sixel fallback |
| xterm-256color | ❌ None | - | - | Character only |

## 🎮 PIXEL POKER ARCHITECTURE

### Card Asset System
```c
// Card images: 75x113 pixels each
// Naming: spadeAce.png, heartKing.png, club10.png, etc.
// Location: assets/sprites/cards/

typedef struct {
    const char* filename;  // "spadeAce.png"
    struct ncplane* plane; // Rendered card plane
    int y, x;             // Position
} PixelCard;

typedef struct {
    PixelCard cards[52];
    int num_visible;
    struct ncplane* background;
} PixelDeck;
```

### Rendering Strategy (Pixel Blitting)
1. **Check pixel support**: `notcurses_canpixel(nc)`
2. **Load PNG assets**: `ncvisual_from_file()`
3. **Use orca sizing**: `geom.rcelly/rcellx` with max constraints
4. **Create dedicated planes**: One plane per card
5. **NCSCALE_STRETCH**: Let notcurses handle aspect ratios

### Working Pixel Demo

The `poker_pixel_showcase` demonstrates all working patterns:
- ✅ Loads all 52 PNG card assets
- ✅ Optimal grid sizing algorithm
- ✅ Proper aspect ratios using orca pattern
- ✅ Pixel support detection
- ✅ Royal flush display with good proportions

## 🚀 QUICK START FOR NEXT DEVELOPER

```bash
# 1. Build everything
./build.sh

# 2. Run pixel showcase (requires pixel terminal)
cd build/demos && ./poker_pixel_showcase

# 3. Press '1' to see all cards, '2' for royal flush
```

## 💡 DEBUGGING TIPS

1. **Cards too tall/skinny?** → Increase max_width constraint
2. **Cards too fat/wide?** → Decrease max_width constraint  
3. **Cards too small?** → Use the grid optimization algorithm
4. **No cards showing?** → Check `notcurses_canpixel(nc)` 
5. **Wrong aspect ratio?** → Trust `geom.rcellx/rcelly`, don't calculate manually

---

## 🚀 FOR THE NEXT DEVELOPER

### What's Ready to Use
✅ **52 high-quality PNG card assets** in `assets/sprites/cards/`
✅ **Working pixel blitting system** following notcurses orca demo patterns  
✅ **Optimal card sizing algorithm** that maximizes card size for any terminal
✅ **Complete showcase demo** at `build/demos/poker_pixel_showcase`
✅ **Fallback character demos** for non-pixel terminals

### Next Steps
1. **Start with `demos/poker_pixel_showcase.c`** - it has all the working patterns
2. **Copy the `display_card_pixel()` function** - it handles everything correctly
3. **Use the grid optimization algorithm** for multi-card layouts
4. **Always check `notcurses_canpixel(nc)`** before using pixel features

### Key Code Pattern
```c
// This is the gold standard - copy this exactly:
struct ncvisual_options vopts = { .blitter = NCBLIT_PIXEL, .scaling = NCSCALE_STRETCH };
struct ncvgeom geom;
ncvisual_geom(nc, ncv, &vopts, &geom);
struct ncplane_options nopts = {
    .rows = geom.rcelly > max_height ? max_height : geom.rcelly,  // KEY!
    .cols = geom.rcellx > max_width ? max_width : geom.rcellx,    // KEY!
    .y = y, .x = x, .name = "card"
};
```

### Terminal Testing
- **Development**: Use `kitty` terminal for best pixel support
- **Testing**: Test in `xterm-256color` to ensure graceful fallback
- **Production**: Support both pixel and character rendering

---
**Status**: ✅ **PRODUCTION READY** - All pixel blitting patterns proven and documented
**Last updated**: June 2025 - Ready for next developer handoff  
**Start here**: `./build.sh && cd build/demos && ./poker_pixel_showcase`