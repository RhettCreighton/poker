#!/bin/bash
# Copyright 2025 Rhett Creighton
# Licensed under the Apache License, Version 2.0

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
JOBS=$(nproc)
CLEAN=false
INSTALL=false
TEST=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --install)
            INSTALL=true
            shift
            ;;
        --test)
            TEST=true
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug     Build in debug mode"
            echo "  --clean     Clean build directory first"
            echo "  --install   Install after building"
            echo "  --test      Run tests after building"
            echo "  --jobs N    Number of parallel jobs (default: $(nproc))"
            echo "  --help      Show this help"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}=== PokerPlatform Build Script ===${NC}"
echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs: $JOBS"
echo

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "${YELLOW}Configuring...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_TESTS=ON \
    -DBUILD_SERVER=ON \
    -DBUILD_CLIENT=ON \
    -DBUILD_AI=ON

# Build
echo -e "${YELLOW}Building...${NC}"
make -j"$JOBS"

# Run tests if requested
if [ "$TEST" = true ]; then
    echo -e "${YELLOW}Running tests...${NC}"
    make test ARGS="-V"
fi

# Install if requested
if [ "$INSTALL" = true ]; then
    echo -e "${YELLOW}Installing...${NC}"
    sudo make install
fi

echo -e "${GREEN}Build complete!${NC}"
echo
echo "To run the server: $BUILD_DIR/server/poker_server"
echo "To run the client: $BUILD_DIR/client/poker_client"