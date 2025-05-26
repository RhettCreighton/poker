# Poker client application
cmake_minimum_required(VERSION 3.13)

add_subdirectory(ui)

add_library(poker_client STATIC
    src/input_handler.c
    # Add more source files here as they are created
    # client.c
    # game_client.c
)

target_include_directories(poker_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(poker_client PUBLIC
    poker_common
    poker_network
    poker_ui
    notcurses
)

# Client executable
add_executable(poker_client_exe
    # main.c
)

target_link_libraries(poker_client_exe PRIVATE
    poker_client
)