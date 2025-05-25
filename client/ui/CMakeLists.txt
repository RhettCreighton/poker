# UI components for poker client
cmake_minimum_required(VERSION 3.13)

add_library(poker_ui STATIC
    # Add source files here as they are created
    # ui_manager.c
    # table_renderer.c
    # card_renderer.c
    # animation.c
    # layouts/layout_manager.c
)

target_include_directories(poker_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(poker_ui PUBLIC
    poker_common
    notcurses
)