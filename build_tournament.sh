#!/bin/bash
# SPDX-FileCopyrightText: 2025 Rhett Creighton
# SPDX-License-Identifier: Apache-2.0

set -e

echo "🎯 Building Tournament Simulation System..."

# Create build directory
mkdir -p build/tournament
mkdir -p build/logs

# Compile hand history implementation
echo "📝 Building hand history system..."
gcc -c -o build/tournament/hand_history.o \
    network/src/hand_history.c \
    -Inetwork/include \
    -Icommon/include \
    -std=c99 -Wall

# Compile tournament simulation
echo "🏆 Building tournament simulation..."
gcc -o build/tournament/tournament_simulation \
    demos/tournament_simulation.c \
    build/tournament/hand_history.o \
    -Inetwork/include \
    -Icommon/include \
    -std=c99 -Wall -lm

echo "✅ Tournament simulation built successfully!"

# Try to build the network components if possible
echo "🌐 Attempting to build network components..."
gcc -c -o build/tournament/protocol.o \
    network/src/protocol.c \
    -Inetwork/include \
    -Icommon/include \
    -std=c99 -Wall 2>/dev/null || echo "⚠️  Protocol compilation needs OpenSSL"

echo ""
echo "🎯 TOURNAMENT BUILD COMPLETE!"
echo "========================================="
echo "Available programs:"
echo "  ./build/tournament/tournament_simulation - 🏆 Full tournament simulation with hand histories"
echo ""
echo "The simulation includes:"
echo "  - 100-player NLHE tournaments"
echo "  - Heads-up tournament brackets"  
echo "  - 2-7 Triple Draw tournaments"
echo "  - Complete hand histories with 64-byte player keys"
echo "  - Tournament results and prize tracking"
echo ""
echo "Run: ./build/tournament/tournament_simulation"