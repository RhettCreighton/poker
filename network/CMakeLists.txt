# Network layer for poker platform
cmake_minimum_required(VERSION 3.13)

# Check for source files
file(GLOB NETWORK_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Only create library if we have source files
if(NETWORK_SOURCES)
    add_library(poker_network STATIC ${NETWORK_SOURCES})
else()
    # Create a dummy library with a placeholder file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/network_placeholder.c" 
        "// Placeholder for network implementation\n"
        "void poker_network_placeholder(void) {}\n"
    )
    
    add_library(poker_network STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/network_placeholder.c"
    )
    
    message(STATUS "Network: Using placeholder (no source files found)")
endif()

target_include_directories(poker_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(poker_network PUBLIC
    poker_common
    pthread
)