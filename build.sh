#!/bin/bash
set -e  # Exit on any error

echo "🎰 Building Terminal Poker Platform..."
echo "========================================="

# Clean any existing builds
echo "🧹 Cleaning previous builds..."
rm -rf build/
rm -f poker_game tournament_27_draw poker_demo_27_lowball poker_demo_9_player_beautiful

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
    
    # Compile demos
    if cc -o poker_demo_27_lowball poker_demo_27_lowball.c -lnotcurses-core -lnotcurses -lm 2>/dev/null; then
        echo "✅ 2-7 Lowball demo compiled"
    fi
    
    if cc -o poker_demo_9_player_beautiful poker_demo_9_player_beautiful.c -lnotcurses-core -lnotcurses -lm 2>/dev/null; then
        echo "✅ 9-player demo compiled"
    fi
    
    # Compile the main playable poker game
    echo "🔨 Compiling playable poker game..."
    if cc -o poker_game poker_game.c \
       mvc/view/beautiful_view.c mvc/view/animated_view.c \
       -I. -lnotcurses-core -lnotcurses -lm 2>/dev/null; then
        echo "✅ 🎭 Beautiful animated poker game compiled!"
    else
        echo "⚠️  Failed to compile poker game"
    fi
fi

echo ""
echo "🎯 BUILD COMPLETE!"
echo "========================================="
echo "Available programs:"
if [ -f poker_demo_27_lowball ]; then
    echo "  ./poker_demo_27_lowball - Beautiful 6-player demo"
fi
if [ -f poker_demo_9_player_beautiful ]; then
    echo "  ./poker_demo_9_player_beautiful - 9-player demo with chip animations"
fi
if [ -f poker_game ]; then
    echo "  ./poker_game - 🎮 PLAYABLE 2-7 Triple Draw (NEW!)"
fi
if [ -f tournament_27_draw ]; then
    echo "  ./tournament_27_draw - Full tournament with ncurses UI"
fi
echo ""
echo "Run: ./run.sh to see the demos!"