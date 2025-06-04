# Terminal Poker Game - Developer Guide

## 🚀 Quick Start

```bash
# Prerequisites: Install notcurses
# Ubuntu/Debian: sudo apt install libnotcurses-dev libnotcurses3
# Fedora/RHEL: sudo dnf install notcurses-devel

# Build and run
./build.sh
./build/demos/poker_pixel_showcase      # Pixel demo (requires kitty/iTerm2/WezTerm)
./build/demos/poker_animation_final     # Animation showcase
./build/demos/poker_demo_27_lowball     # Classic character demo
```

## 📁 Project Structure

```
poker/
├── assets/sprites/cards/    # 52 PNG card images (75x113px)
├── demos/                   # Complete demo applications
├── mvc/view/               # Rendering engine with pixel support
├── sprite-experiments/     # Test programs and experiments
└── variants/              # Different poker game types
```

## 🎯 Key Features

- **Pixel-perfect card rendering** using notcurses NCBLIT_PIXEL
- **Professional animations** for dealing, betting, and pot collection
- **Multiple poker variants** including Texas Hold'em and 2-7 Lowball
- **Automatic terminal detection** with graceful fallback

## 💻 Terminal Requirements

| Terminal | Pixel Support | Notes |
|----------|--------------|-------|
| kitty | ✅ Excellent | Recommended for development |
| iTerm2 | ✅ Good | macOS users |
| WezTerm | ✅ Good | Cross-platform |
| xterm | ❌ None | Character rendering only |

## 🔧 Core Code Patterns

### Pixel Card Rendering
```c
// The proven pattern for perfect aspect ratios
struct ncvisual* ncv = ncvisual_from_file("assets/sprites/cards/spadeAce.png");
struct ncvisual_options vopts = {
    .blitter = NCBLIT_PIXEL,
    .scaling = NCSCALE_STRETCH,
};

struct ncvgeom geom;
ncvisual_geom(nc, ncv, &vopts, &geom);

// Let notcurses calculate optimal dimensions
struct ncplane_options nopts = {
    .rows = geom.rcelly > max_height ? max_height : geom.rcelly,
    .cols = geom.rcellx > max_width ? max_width : geom.rcellx,
    .y = y, .x = x, .name = "card"
};
```

### Animation System
```c
// Smooth animations at 50 FPS
Animation* anim = animation_create(ANIM_CARD_DEAL, 500); // 500ms duration
animation_add_stage(anim, card_plane, start_x, start_y, end_x, end_y);
animation_engine_add(engine, anim);
animation_engine_update(engine, 20); // Update every 20ms
```

## 📋 Available Demos

1. **poker_pixel_showcase** - Shows all 52 cards with pixel rendering
2. **poker_animation_final** - Complete game with smooth animations
3. **poker_pixel_10player_professional** - Multi-player Texas Hold'em
4. **poker_pixel_10player_lowball_v2** - 2-7 Triple Draw variant

## 🛠️ Development Tips

- Always run commands from the project root directory
- Use `notcurses_canpixel(nc)` to check pixel support
- Follow the orca demo pattern for aspect ratios (see code above)
- Test in both pixel (kitty) and character (xterm) terminals

## 📚 Next Steps

1. Study `demos/poker_pixel_showcase.c` for rendering patterns
2. Review `mvc/view/animation_engine.h` for animation API
3. Check `demos/poker_animation_spec.md` for detailed requirements
4. Run tests with `./demos/run_animation_tests.sh`

---
**Status**: Production ready with pixel rendering and animations
**License**: Apache-2.0