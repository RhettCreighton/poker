# Poker server components
cmake_minimum_required(VERSION 3.13)

add_library(poker_server STATIC
    # Add source files here as they are created
    # server.c
    # game_manager.c
    # player_manager.c
    # tournament_manager.c
)

target_include_directories(poker_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(poker_server PUBLIC
    poker_common
    poker_variants
    poker_network
)

# Server executable
add_executable(poker_server_exe
    # main.c
)

target_link_libraries(poker_server_exe PRIVATE
    poker_server
)