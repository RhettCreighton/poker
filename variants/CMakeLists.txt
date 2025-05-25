# Poker game variants
cmake_minimum_required(VERSION 3.13)

# Add all variant subdirectories
add_subdirectory(holdem)
add_subdirectory(omaha)
add_subdirectory(stud)
add_subdirectory(draw)
add_subdirectory(lowball)

# Create a unified variants library that includes all variants
add_library(poker_variants INTERFACE)

target_link_libraries(poker_variants INTERFACE
    poker_holdem
    poker_omaha
    poker_stud
    poker_draw
    poker_lowball
)