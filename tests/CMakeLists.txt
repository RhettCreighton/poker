# Copyright 2025 Rhett Creighton
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Test configuration
enable_testing()

# Common test includes
include_directories(
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/variants
    ${CMAKE_SOURCE_DIR}/server/include
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_SOURCE_DIR}/network/include
    ${CMAKE_SOURCE_DIR}/ai/include
)

# Unit tests
# Note: test_cards.c doesn't exist yet, commenting out for now
# add_executable(test_cards
#     unit/test_cards.c
# )
# target_link_libraries(test_cards poker_common ${M_LIBRARY})
# add_test(NAME test_cards COMMAND test_cards)

# Deck tests - use fixed version
add_executable(test_deck
    test_deck_fixed.c
)
target_link_libraries(test_deck poker_common ${M_LIBRARY})
add_test(NAME test_deck COMMAND test_deck)

# Hand evaluation tests - use fixed version
add_executable(test_hand_eval
    test_hand_eval_fixed.c
)
target_link_libraries(test_hand_eval poker_common ${M_LIBRARY} Threads::Threads)
add_test(NAME test_hand_eval COMMAND test_hand_eval)

# Game state tests - use fixed version
add_executable(test_game_state
    test_game_state_fixed.c
)
target_link_libraries(test_game_state poker_common ${M_LIBRARY})
add_test(NAME test_game_state COMMAND test_game_state)

# AI tests - use fixed version
add_executable(test_ai
    test_ai_fixed.c
)
target_link_libraries(test_ai poker_ai poker_common ${M_LIBRARY})
add_test(NAME test_ai COMMAND test_ai)

# Integration tests
# if(BUILD_INTEGRATION_TESTS)
#     add_subdirectory(integration)
# endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# P2P Network tests - use fixed version
add_executable(test_p2p_network test_p2p_network_fixed.c)
target_link_libraries(test_p2p_network
    poker_network
    poker_ai
    poker_common
    pthread
    ${M_LIBRARY}
)
add_test(NAME test_p2p_network COMMAND test_p2p_network)

# Simple test that actually works
add_executable(test_simple test_simple.c)
target_link_libraries(test_simple poker_common ${M_LIBRARY})
add_test(NAME test_simple COMMAND test_simple)

# Custom test target that shows output
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_simple test_deck test_hand_eval test_game_state test_ai test_p2p_network
    COMMENT "Running tests..."
)