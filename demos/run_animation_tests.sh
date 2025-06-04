#!/bin/bash
# SPDX-FileCopyrightText: 2025 Rhett Creighton
# SPDX-License-Identifier: Apache-2.0

echo "Running poker animation demo and tests..."

# Run the animation demo first (it will export state)
echo "Starting animation demo (press any key to end demo)..."
timeout 5s ./poker_animation_complete || true

# Check if state file was created
if [ -f "animation_state.txt" ]; then
    echo "Animation state exported successfully"
    cat animation_state.txt
else
    echo "Warning: No animation state file created"
fi

# Run the tests
echo -e "\nRunning animation tests..."
./poker_animation_test --test

# Cleanup
rm -f animation_state.txt