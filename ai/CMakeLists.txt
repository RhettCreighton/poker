# AI system for poker
cmake_minimum_required(VERSION 3.13)

# Check for source files
file(GLOB AI_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Only create library if we have source files
if(AI_SOURCES)
    add_library(poker_ai STATIC ${AI_SOURCES})
else()
    # Create a dummy library with a placeholder file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ai_placeholder.c" 
        "// Placeholder for AI implementation\n"
        "void poker_ai_placeholder(void) {}\n"
    )
    
    add_library(poker_ai STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/ai_placeholder.c"
    )
    
    message(STATUS "AI system: Using placeholder (no source files found)")
endif()

target_include_directories(poker_ai PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/variants
)

target_link_libraries(poker_ai PUBLIC
    poker_common
    m  # math library
)