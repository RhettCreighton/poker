#!/bin/bash
set -e  # Exit on any error

echo "🎰 Building Terminal Poker Platform..."
echo "========================================="

# Clean any existing builds
echo "🧹 Cleaning previous builds..."
rm -rf build/
rm -f poker_game tournament_27_draw

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
    
    # Fallback: compile standalone game directly
    echo "🔨 Compiling standalone 2-7 Triple Draw..."
    gcc -o poker_game standalone_27_draw.c -lm -O2
    echo "✅ Standalone game compiled successfully!"
fi

echo ""
echo "🎯 BUILD COMPLETE!"
echo "========================================="
echo "Ready to play:"
echo "  ./poker_game     - Play 2-7 Triple Draw immediately"
if [ -f tournament_27_draw ]; then
    echo "  ./tournament_27_draw - Full tournament with ncurses UI"
fi
echo ""
echo "Run: ./run.sh to start playing!"