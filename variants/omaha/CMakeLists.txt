# Omaha variant
cmake_minimum_required(VERSION 3.13)

# Check for source files
file(GLOB OMAHA_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Only create library if we have source files
if(OMAHA_SOURCES)
    add_library(poker_omaha STATIC ${OMAHA_SOURCES})
else()
    # Create a dummy library with a placeholder file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/omaha_placeholder.c" 
        "// Placeholder for Omaha implementation\n"
        "void poker_omaha_placeholder(void) {}\n"
    )
    
    add_library(poker_omaha STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/omaha_placeholder.c"
    )
    
    message(STATUS "Omaha variant: Using placeholder (no source files found)")
endif()

target_include_directories(poker_omaha PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(poker_omaha PUBLIC
    poker_common
)