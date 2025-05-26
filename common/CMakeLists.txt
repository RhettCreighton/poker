# Common poker components library
cmake_minimum_required(VERSION 3.13)

# Find available source files
set(COMMON_SOURCES "")

# Check each expected source file
foreach(SOURCE_FILE cards.c deck.c hand_eval.c player.c game_state.c game_manager.c)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${SOURCE_FILE}")
        list(APPEND COMMON_SOURCES "src/${SOURCE_FILE}")
    else()
        message(STATUS "Common library: ${SOURCE_FILE} not found, skipping")
    endif()
endforeach()

# Ensure we have at least some source files
if(NOT COMMON_SOURCES)
    message(FATAL_ERROR "No source files found for poker_common library")
endif()

# Common library for shared poker components
add_library(poker_common STATIC ${COMMON_SOURCES})

target_include_directories(poker_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(poker_common PUBLIC
    ${NOTCURSES_LIBRARIES}
)

# Export for other modules
set(POKER_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# Optional: Build lookup table generator if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/hand_eval_tables.c")
    add_executable(hand_eval_table_gen src/hand_eval_tables.c)
    target_include_directories(hand_eval_table_gen PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # Add custom target to generate tables
    add_custom_target(generate_hand_tables
        COMMAND hand_eval_table_gen ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS hand_eval_table_gen
        COMMENT "Generating hand evaluation lookup tables"
    )
endif()