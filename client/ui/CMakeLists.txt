# UI components for poker client
cmake_minimum_required(VERSION 3.13)

# Check for existing UI source files
set(UI_SOURCES)
foreach(src src/render_helpers.c src/layouts/heads_up.c)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND UI_SOURCES ${src})
    endif()
endforeach()

if(UI_SOURCES)
    add_library(poker_ui STATIC ${UI_SOURCES})
else()
    # Create placeholder if no sources exist
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ui_placeholder.c"
        "/* SPDX-FileCopyrightText: 2025 Rhett Creighton\n"
        " * SPDX-License-Identifier: Apache-2.0\n"
        " */\n\n"
        "void poker_ui_placeholder(void) {}\n"
    )
    add_library(poker_ui STATIC "${CMAKE_CURRENT_BINARY_DIR}/ui_placeholder.c")
endif()

target_include_directories(poker_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(poker_ui PUBLIC
    poker_common
    notcurses
)