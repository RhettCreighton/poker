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
add_executable(test_cards
    unit/test_cards.c
)
target_link_libraries(test_cards poker_common ${M_LIBRARY})
add_test(NAME test_cards COMMAND test_cards)

# Add more tests as components are implemented
# add_executable(test_deck
#     unit/test_deck.c
# )
# target_link_libraries(test_deck poker_common ${M_LIBRARY})
# add_test(NAME test_deck COMMAND test_deck)

# add_executable(test_hand_eval
#     unit/test_hand_eval.c
# )
# target_link_libraries(test_hand_eval poker_common ${M_LIBRARY} Threads::Threads)
# add_test(NAME test_hand_eval COMMAND test_hand_eval)

# Integration tests
# if(BUILD_INTEGRATION_TESTS)
#     add_subdirectory(integration)
# endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# P2P Network tests
add_executable(test_p2p_network test_p2p_network.c)
target_link_libraries(test_p2p_network
    poker_network
    poker_ai
    poker_common
    pthread
    ${M_LIBRARY}
)
add_test(NAME test_p2p_network COMMAND test_p2p_network)

# Custom test target that shows output
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_cards test_p2p_network
    COMMENT "Running tests..."
)