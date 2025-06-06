#!/bin/bash
# SPDX-FileCopyrightText: 2025 Rhett Creighton
# SPDX-License-Identifier: Apache-2.0

# Build script for Terminal Poker Game
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}🎰 Building Terminal Poker Game...${NC}"

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: CMake is required but not installed.${NC}"
    echo "Install with: sudo apt install cmake (Ubuntu/Debian) or sudo dnf install cmake (Fedora)"
    exit 1
fi

# Check for notcurses
if ! pkg-config --exists notcurses 2>/dev/null; then
    echo -e "${RED}Error: notcurses is required but not installed.${NC}"
    echo "Install with:"
    echo "  Ubuntu/Debian: sudo apt install libnotcurses-dev libnotcurses3"
    echo "  Fedora/RHEL: sudo dnf install notcurses-devel"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo -e "${YELLOW}Building project...${NC}"
make -j$(nproc)

echo -e "${GREEN}✅ Build complete!${NC}"
echo ""

# Check which demos actually built
echo "Available programs:"
if [ -f build/src/main/simple_27_draw ]; then
    echo "  ./build/src/main/simple_27_draw        # Simple 2-7 Draw game"
fi
if [ -f build/src/main/tournament_27_draw ]; then
    echo "  ./build/src/main/tournament_27_draw    # Tournament 2-7 Draw"
fi
if [ -f build/demos/persistence_demo ]; then
    echo "  ./build/demos/persistence_demo         # Persistence system demo"
fi
if [ -f build/demos/error_logging_demo ]; then
    echo "  ./build/demos/error_logging_demo       # Error handling & logging demo"
fi
if [ -f build/demos/simple_poker_demo ]; then
    echo "  ./build/demos/simple_poker_demo        # Simple poker demo"
fi

echo ""
echo "Working tests:"
echo "  ./build/tests/test_simple              # Basic tests that pass"
echo ""
echo "Run all tests with:"
echo "  cd build && ctest --output-on-failure"
echo ""
echo "Note: Some demos require pixel-capable terminals (kitty, iTerm2, WezTerm)."
echo "      GUI demos are still being fixed."