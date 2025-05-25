# Main executables
cmake_minimum_required(VERSION 3.13)

# Simple 27 draw game (working version)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simple_27_draw.c")
    add_executable(simple_27_draw simple_27_draw.c)
    message(STATUS "Created simple_27_draw executable")
endif()

# Tournament executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tournament_27_draw.c")
    add_executable(tournament_27_draw tournament_27_draw.c)
    
    target_include_directories(tournament_27_draw PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
        ${CMAKE_SOURCE_DIR}/variants
        ${CMAKE_SOURCE_DIR}/ai/include
    )
    
    target_link_libraries(tournament_27_draw
        poker_common
        poker_lowball
        poker_ai
        ncurses
        m
    )
    
    message(STATUS "Created tournament_27_draw executable")
endif()

# General game executable (if main.c exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
    add_executable(poker_game main.c)
    
    target_include_directories(poker_game PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
        ${CMAKE_SOURCE_DIR}/variants
        ${CMAKE_SOURCE_DIR}/ai/include
    )
    
    target_link_libraries(poker_game
        poker_common
        poker_holdem
        poker_ai
        ncurses
        m
    )
endif()