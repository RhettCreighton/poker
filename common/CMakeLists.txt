# Common poker components library
cmake_minimum_required(VERSION 3.13)

# Common source files
set(COMMON_SOURCES
    src/cards.c
    src/deck.c
    src/player.c
    src/game_state.c
    # src/game_manager.c  # Commented out - needs major refactoring to match current API
    src/hand_eval.c
    src/hand_eval_tables.c
    src/error.c
    src/logger.c
    src/persistence.c
    src/autosave.c
)

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