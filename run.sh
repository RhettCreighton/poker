#!/bin/bash

echo "🎰 Terminal Poker Platform 🎰"
echo "============================="

# List available programs
echo "Available programs:"
echo ""

if [ -f poker_demo_27_lowball ]; then
    echo "  1) 2-7 Lowball Demo (6 players)"
fi
if [ -f poker_demo_9_player_beautiful ]; then
    echo "  2) 9-Player Demo (chip animations)"
fi
if [ -f poker_game ]; then
    echo "  3) 🎮 PLAYABLE 2-7 Triple Draw"
fi
if [ -f tournament_27_draw ]; then
    echo "  4) Tournament Mode"
fi

echo ""
read -p "Select program (1-4, default 3): " choice

case $choice in
    1)
        if [ -f poker_demo_27_lowball ]; then
            GAME="./poker_demo_27_lowball"
        else
            echo "Demo not found!"
            exit 1
        fi
        ;;
    2)
        if [ -f poker_demo_9_player_beautiful ]; then
            GAME="./poker_demo_9_player_beautiful"
        else
            echo "Demo not found!"
            exit 1
        fi
        ;;
    4)
        if [ -f tournament_27_draw ]; then
            GAME="./tournament_27_draw"
        else
            echo "Tournament not found!"
            exit 1
        fi
        ;;
    *)
        if [ -f poker_game ]; then
            GAME="./poker_game"
        else
            echo "Playable game not found! Run ./build.sh first"
            exit 1
        fi
        ;;
esac

echo ""
echo "🚀 Starting game..."
echo "====================="

# Run the selected game
exec $GAME