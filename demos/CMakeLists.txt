# SPDX-FileCopyrightText: 2025 Rhett Creighton
# SPDX-License-Identifier: Apache-2.0

# Demo programs CMakeLists.txt

# Common dependencies for demos
set(DEMO_LIBS
    poker_common
    poker_holdem
    poker_draw
    poker_lowball
    poker_ai
    ${NOTCURSES_LIBRARIES}
    ${M_LIBRARY}
    Threads::Threads
)

# Helper function to add demo executable
function(add_demo demo_name)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${demo_name}.c")
        add_executable(${demo_name} ${demo_name}.c)
        target_link_libraries(${demo_name} ${DEMO_LIBS})
        target_include_directories(${demo_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/mvc
            ${CMAKE_SOURCE_DIR}/mvc/view
            ${CMAKE_SOURCE_DIR}/variants
        )
    endif()
endfunction()

# Start with simpler demos that are more likely to build
add_demo(simple_poker_demo)
add_demo(poker_demo_27_lowball)
add_demo(poker_demo_9_player_beautiful)

# Try pixel demos (may need additional dependencies)
add_demo(poker_pixel_showcase)
add_demo(poker_animation_final_pixel)
add_demo(poker_pixel_10player_professional)
add_demo(poker_pixel_10player_lowball_v2)

# Interactive demos
add_demo(poker_interactive_pixel)
add_demo(poker_interactive_complete)
add_demo(poker_interactive_hero)

# Animation test
add_demo(poker_animation_test)

# Persistence demo
add_executable(persistence_demo persistence_demo.c)
target_link_libraries(persistence_demo poker_common ${M_LIBRARY} Threads::Threads)

# Error logging demo
add_executable(error_logging_demo error_logging_demo.c)
target_link_libraries(error_logging_demo poker_common ${M_LIBRARY} Threads::Threads)

# AI showcase demo
add_executable(ai_showcase_demo ai_showcase_demo.c)
target_link_libraries(ai_showcase_demo poker_ai poker_common ${M_LIBRARY} Threads::Threads)

# AI personality showcase
add_executable(ai_personality_showcase ai_personality_showcase.c)
target_link_libraries(ai_personality_showcase 
    poker_ai 
    poker_common 
    poker_variants
    ${M_LIBRARY} 
    Threads::Threads
)

# AI skill levels demo
add_executable(ai_skill_levels_demo ai_skill_levels_demo.c)
target_link_libraries(ai_skill_levels_demo 
    poker_ai 
    poker_common 
    poker_variants
    ${M_LIBRARY} 
    Threads::Threads
)

# Poker variants showcase demo
add_executable(poker_variants_showcase poker_variants_showcase.c)
target_link_libraries(poker_variants_showcase 
    poker_common 
    poker_variants
    poker_ai
    ${M_LIBRARY} 
    Threads::Threads
)

# Hand history replay demo (simple version)
add_executable(hand_history_replay_simple hand_history_replay_simple.c)
target_link_libraries(hand_history_replay_simple 
    poker_network
    poker_common 
    ${M_LIBRARY} 
    Threads::Threads
)

# Tournament demos
add_executable(tournament_simulation tournament_simulation.c)
target_link_libraries(tournament_simulation 
    poker_server
    poker_common 
    poker_variants
    poker_ai
    ${M_LIBRARY} 
    Threads::Threads
)

add_executable(tournament_full_demo tournament_full_demo.c)
target_link_libraries(tournament_full_demo 
    poker_network
    poker_common 
    ${M_LIBRARY} 
    Threads::Threads
)

add_executable(multi_table_tournament_demo multi_table_tournament_demo.c)
target_link_libraries(multi_table_tournament_demo 
    poker_server
    poker_network
    poker_common 
    poker_variants
    poker_ai
    ${M_LIBRARY} 
    Threads::Threads
)

add_executable(multi_table_tournament_demo_simple multi_table_tournament_demo_simple.c)
target_link_libraries(multi_table_tournament_demo_simple 
    poker_server
    poker_common 
    poker_ai
    ${M_LIBRARY} 
    Threads::Threads
)

add_executable(tournament_showcase tournament_showcase.c)
target_link_libraries(tournament_showcase 
    poker_server
    poker_common 
    ${M_LIBRARY} 
    Threads::Threads
)

# Create alias for documented demo name
add_custom_target(poker_animation_final ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
        poker_animation_final_pixel 
        ${CMAKE_CURRENT_BINARY_DIR}/poker_animation_final
    DEPENDS poker_animation_final_pixel
)