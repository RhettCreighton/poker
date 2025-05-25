# Draw poker variants
cmake_minimum_required(VERSION 3.13)

# Check for source files
file(GLOB DRAW_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Only create library if we have source files
if(DRAW_SOURCES)
    add_library(poker_draw STATIC ${DRAW_SOURCES})
else()
    # Create a dummy library with a placeholder file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/draw_placeholder.c" 
        "// Placeholder for Draw poker implementation\n"
        "void poker_draw_placeholder(void) {}\n"
    )
    
    add_library(poker_draw STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/draw_placeholder.c"
    )
    
    message(STATUS "Draw variants: Using placeholder (no source files found)")
endif()

target_include_directories(poker_draw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(poker_draw PUBLIC
    poker_common
)