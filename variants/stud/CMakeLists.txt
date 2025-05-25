# Stud poker variants
cmake_minimum_required(VERSION 3.13)

# Check for source files
file(GLOB STUD_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Only create library if we have source files
if(STUD_SOURCES)
    add_library(poker_stud STATIC ${STUD_SOURCES})
else()
    # Create a dummy library with a placeholder file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stud_placeholder.c" 
        "// Placeholder for Stud poker implementation\n"
        "void poker_stud_placeholder(void) {}\n"
    )
    
    add_library(poker_stud STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/stud_placeholder.c"
    )
    
    message(STATUS "Stud variants: Using placeholder (no source files found)")
endif()

target_include_directories(poker_stud PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(poker_stud PUBLIC
    poker_common
)