# Poker server components
cmake_minimum_required(VERSION 3.13)

# Server source files
set(SERVER_SOURCES
    src/server_main.c
    src/local_simulation_server.c
    src/lobby_system.c
    src/tournament_system.c
    src/table_manager.c
    src/connection_handler.c
)

# Check which source files exist
set(EXISTING_SERVER_SOURCES)
foreach(source ${SERVER_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        list(APPEND EXISTING_SERVER_SOURCES ${source})
    endif()
endforeach()

# Create server library
if(EXISTING_SERVER_SOURCES)
    add_library(poker_server STATIC ${EXISTING_SERVER_SOURCES})
else()
    # Create placeholder
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/server_placeholder.c"
        "/* SPDX-FileCopyrightText: 2025 Rhett Creighton\n"
        " * SPDX-License-Identifier: Apache-2.0\n"
        " */\n\n"
        "void poker_server_placeholder(void) {}\n"
    )
    add_library(poker_server STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/server_placeholder.c"
    )
endif()

# P2P simulation executable (first definition)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/p2p_simulation_main.c")
    add_executable(p2p_simulation src/p2p_simulation_main.c)
    target_link_libraries(p2p_simulation PRIVATE
        poker_network
        poker_ai
        poker_common
        pthread
    )
endif()

target_include_directories(poker_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/network/include
    ${CMAKE_SOURCE_DIR}/ai/include
)

target_link_libraries(poker_server PUBLIC
    poker_common
    poker_variants
    poker_network
    poker_ai
    pthread
)

# Local simulation server executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/server_main.c")
    add_executable(poker_server_exe src/server_main.c)
    target_link_libraries(poker_server_exe PRIVATE poker_server)
    set_target_properties(poker_server_exe PROPERTIES OUTPUT_NAME "poker_server")
endif()

# P2P simulation executable (duplicate removed - already defined above)

# Test utilities - commented out as test files don't exist
# if(BUILD_TESTS)
#     add_executable(test_consensus
#         tests/test_consensus.c
#     )
#     target_link_libraries(test_consensus PRIVATE poker_server poker_network)
#     
#     add_executable(test_gossip
#         tests/test_gossip.c
#     )
#     target_link_libraries(test_gossip PRIVATE poker_server poker_network)
# endif()