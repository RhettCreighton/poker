# Texas Hold'em variant
cmake_minimum_required(VERSION 3.13)

# Check for source files
file(GLOB HOLDEM_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Only create library if we have source files
if(HOLDEM_SOURCES)
    add_library(poker_holdem STATIC ${HOLDEM_SOURCES})
    
    target_include_directories(poker_holdem PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}  # Add root directory for variant_interface.h
    )
    
    target_link_libraries(poker_holdem PUBLIC
        poker_common
    )
else()
    # Create a dummy library with a placeholder file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/holdem_placeholder.c" 
        "// Placeholder for Texas Hold'em implementation\n"
        "void poker_holdem_placeholder(void) {}\n"
    )
    
    add_library(poker_holdem STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/holdem_placeholder.c"
    )
    
    target_include_directories(poker_holdem PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}  # Add root directory for variant_interface.h
    )
    
    target_link_libraries(poker_holdem PUBLIC
        poker_common
    )
    
    message(STATUS "Texas Hold'em variant: Using placeholder (no source files found)")
endif()