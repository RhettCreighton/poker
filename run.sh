#!/bin/bash

echo "🎰 Terminal Poker Platform 🎰"
echo "============================="

# Check if games are built
if [ ! -f poker_game ] && [ ! -f tournament_27_draw ]; then
    echo "❌ No games found! Run ./build.sh first"
    exit 1
fi

# Default to standalone game
GAME="./poker_game"

# If tournament version exists, offer choice
if [ -f tournament_27_draw ]; then
    echo "Available games:"
    echo "  1) 2-7 Triple Draw (standalone)"
    echo "  2) Tournament with AI (ncurses)"
    echo ""
    read -p "Select game (1 or 2, default 1): " choice
    
    case $choice in
        2)
            GAME="./tournament_27_draw"
            ;;
        *)
            GAME="./poker_game"
            ;;
    esac
fi

echo ""
echo "🚀 Starting game..."
echo "====================="

# Run the selected game
exec $GAME