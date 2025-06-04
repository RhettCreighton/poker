#!/bin/bash
set -e  # Exit on any error

echo "🎰 Building Terminal Poker Platform..."
echo "========================================="

# Clean any existing builds
echo "🧹 Cleaning previous builds..."
rm -rf build/
rm -f tournament_27_draw

# Create build directory
echo "📁 Creating build directory..."
mkdir -p build

# Try CMake build first (full platform)
echo "🔨 Attempting CMake build..."
cd build
if cmake .. -DCMAKE_BUILD_TYPE=Release 2>/dev/null && make -j$(nproc) 2>/dev/null; then
    echo "✅ CMake build successful!"
    cd ..
    
    # Copy executables to root for easy access
    if [ -f build/poker_game_standalone ]; then
        cp build/poker_game_standalone ./poker_game
        echo "✅ Standalone game available as: ./poker_game"
    fi
    
    if [ -f build/src/main/tournament_27_draw ]; then
        cp build/src/main/tournament_27_draw ./tournament_27_draw
        echo "✅ Tournament game available as: ./tournament_27_draw"
    fi
else
    echo "⚠️  CMake build failed, falling back to simple compilation..."
    cd ..
    
    # Fallback: compile demos and games directly
    echo "🔨 Compiling poker demos and games..."
    
    # Compile demos from demos directory
    mkdir -p build/demos
    
    if cc -o build/demos/poker_demo_27_lowball demos/poker_demo_27_lowball.c -lnotcurses-core -lnotcurses -lm 2>/dev/null; then
        echo "✅ 2-7 Lowball demo compiled"
    fi
    
    if cc -o build/demos/poker_demo_9_player_beautiful demos/poker_demo_9_player_beautiful.c -lnotcurses-core -lnotcurses -lm 2>/dev/null; then
        echo "✅ 9-player demo compiled"
    fi
    
    # Compile the main playable poker game
    echo "🔨 Compiling playable poker game..."
    if cc -o build/demos/poker_game demos/poker_game.c \
       mvc/view/beautiful_view.c mvc/view/animated_view.c \
       -I. -lnotcurses-core -lnotcurses -lm 2>/dev/null; then
        echo "✅ 🎭 Beautiful animated poker game compiled!"
    else
        echo "⚠️  Failed to compile poker game"
    fi
    
    # Build sprite experiments
    echo "🔨 Building sprite experiments..."
    mkdir -p build/sprite-experiments
    if cc -o build/sprite-experiments/simple_card_demo sprite-experiments/simple_card_demo.c \
       -lnotcurses -lnotcurses-core -std=c99 -Wall -Wextra 2>/dev/null; then
        echo "✅ Simple card sprite demo compiled"
    else
        echo "⚠️  Failed to compile simple card sprite demo"
    fi
    
    # Build pixel blitting demos
    echo "🔨 Building pixel blitting demos..."
    if cc -o build/demos/poker_pixel_showcase demos/poker_pixel_showcase.c \
       mvc/view/sprite_renderer.c -I. -lnotcurses-core -lnotcurses -lm -D_GNU_SOURCE 2>/dev/null; then
        echo "✅ Pixel showcase demo compiled"
    else
        echo "⚠️  Failed to compile pixel showcase"
    fi
    
    if cc -o build/demos/poker_pixel_10player_professional demos/poker_pixel_10player_professional.c \
       mvc/view/sprite_renderer.c -I. -lnotcurses-core -lnotcurses -lm -D_GNU_SOURCE 2>/dev/null; then
        echo "✅ 10-player pixel professional demo compiled"
    else
        echo "⚠️  Failed to compile 10-player pixel professional demo"
    fi
    
    if cc -o build/demos/poker_pixel_10player_lowball_v2 demos/poker_pixel_10player_lowball_v2.c \
       -I. -lnotcurses-core -lnotcurses -lm -lpthread -D_GNU_SOURCE 2>/dev/null; then
        echo "✅ 10-player pixel lowball V2 (clean) compiled"
    else
        echo "⚠️  Failed to compile 10-player pixel lowball V2"
    fi
    
    # Build animation demos
    if cc -o build/demos/poker_animation_final_pixel demos/poker_animation_final_pixel.c \
       mvc/view/sprite_renderer.c mvc/view/animation_engine.c -I. -lnotcurses-core -lnotcurses -lm -D_GNU_SOURCE 2>/dev/null; then
        echo "✅ Animation final pixel demo compiled"
    else
        echo "⚠️  Failed to compile animation final pixel demo"
    fi
    
    if cc -o build/demos/poker_animation_test demos/poker_animation_test.c \
       mvc/view/sprite_renderer.c mvc/view/animation_engine.c -I. -lnotcurses-core -lnotcurses -lm -D_GNU_SOURCE 2>/dev/null; then
        echo "✅ Animation test suite compiled"
    else
        echo "⚠️  Failed to compile animation test suite"
    fi
    
    # Build interactive pixel demo
    if cc -o build/demos/poker_interactive_pixel demos/poker_interactive_pixel.c \
       mvc/view/sprite_renderer.c -I. -lnotcurses-core -lnotcurses -lm -D_GNU_SOURCE 2>/dev/null; then
        echo "✅ Interactive pixel poker compiled"
    else
        echo "⚠️  Failed to compile interactive pixel poker"
    fi
    
    # Build interactive hero demo (based on working animation demo)
    if cc -o build/demos/poker_interactive_hero demos/poker_interactive_hero.c \
       mvc/view/sprite_renderer.c mvc/view/animation_engine.c -I. -lnotcurses-core -lnotcurses -lm -D_GNU_SOURCE 2>/dev/null; then
        echo "✅ Interactive hero poker compiled"
    else
        echo "⚠️  Failed to compile interactive hero poker"
    fi
    
    # Build interactive complete demo
    if cc -o build/demos/poker_interactive_complete demos/poker_interactive_complete.c \
       -I. -lnotcurses-core -lnotcurses -lm -lpthread 2>/dev/null; then
        echo "✅ Interactive complete poker compiled"
    else
        echo "⚠️  Failed to compile interactive complete poker"
    fi
fi

echo ""
echo "🎯 BUILD COMPLETE!"
echo "========================================="
echo "Available programs:"
if [ -f build/demos/poker_demo_27_lowball ]; then
    echo "  ./build/demos/poker_demo_27_lowball - Beautiful 6-player demo"
fi
if [ -f build/demos/poker_demo_9_player_beautiful ]; then
    echo "  ./build/demos/poker_demo_9_player_beautiful - 9-player demo with chip animations"
fi
if [ -f build/demos/poker_game ]; then
    echo "  ./build/demos/poker_game - 🎮 PLAYABLE 2-7 Triple Draw (NEW!)"
fi
if [ -f tournament_27_draw ]; then
    echo "  ./tournament_27_draw - Full tournament with ncurses UI"
fi
if [ -f build/demos/poker_pixel_showcase ]; then
    echo "  ./build/demos/poker_pixel_showcase - 🎨 PIXEL BLITTING Showcase (Requires pixel terminal!)"
fi
if [ -f build/demos/poker_pixel_10player_professional ]; then
    echo "  ./build/demos/poker_pixel_10player_professional - 🎰 10-PLAYER PIXEL Professional (Requires pixel terminal!)"
fi
if [ -f build/demos/poker_pixel_10player_lowball_v2 ]; then
    echo "  ./build/demos/poker_pixel_10player_lowball_v2 - 🎯 CLEAN PRO 10-PLAYER (Test with --test flag)"
fi
if [ -f build/demos/poker_animation_final_pixel ]; then
    echo "  ./build/demos/poker_animation_final_pixel - 🎨 ANIMATED PIXEL POKER (Smooth animations!)"
fi
if [ -f build/demos/poker_animation_test ]; then
    echo "  ./build/demos/poker_animation_test - 🧪 Animation test suite (Use --test flag)"
fi
if [ -f build/demos/poker_interactive_pixel ]; then
    echo "  ./build/demos/poker_interactive_pixel - 🎮 INTERACTIVE POKER with hero controls!"
fi
if [ -f build/demos/poker_interactive_hero ]; then
    echo "  ./build/demos/poker_interactive_hero - 🎯 HERO POKER with pixel cards and controls!"
fi
if [ -f build/demos/poker_interactive_complete ]; then
    echo "  ./build/demos/poker_interactive_complete - 🎲 COMPLETE INTERACTIVE poker with all features!"
fi
if [ -f build/sprite-experiments/simple_card_demo ]; then
    echo "  ./build/sprite-experiments/simple_card_demo - 🃏 Simple card sprite demo (Clean example)"
fi
echo ""
echo "Run demos directly from project root - e.g., ./build/demos/poker_pixel_showcase"