# Lowball poker variants
cmake_minimum_required(VERSION 3.13)

# Check for source files
file(GLOB LOWBALL_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Only create library if we have source files
if(LOWBALL_SOURCES)
    add_library(poker_lowball STATIC ${LOWBALL_SOURCES})
else()
    # Create a dummy library with a placeholder file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lowball_placeholder.c" 
        "// Placeholder for Lowball poker implementation\n"
        "void poker_lowball_placeholder(void) {}\n"
    )
    
    add_library(poker_lowball STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/lowball_placeholder.c"
    )
    
    message(STATUS "Lowball variants: Using placeholder (no source files found)")
endif()

target_include_directories(poker_lowball PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(poker_lowball PUBLIC
    poker_common
)