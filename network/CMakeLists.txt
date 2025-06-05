# Network layer for poker platform
cmake_minimum_required(VERSION 3.13)

# Network source files
set(NETWORK_SOURCES
    src/protocol.c
    src/p2p_protocol.c
    src/chattr_gossip_stub.c
    src/poker_log_protocol.c
    src/poker_log_protocol_phh.c
    src/crypto_utils.c
    src/network_simulation.c
    src/phh_parser.c
    src/phh_export.c
    src/hand_history.c
    src/hand_history_analyzer.c
)

# Check which source files exist
set(EXISTING_SOURCES)
foreach(source ${NETWORK_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        list(APPEND EXISTING_SOURCES ${source})
    endif()
endforeach()

# Create library with existing sources or placeholder
if(EXISTING_SOURCES)
    add_library(poker_network STATIC ${EXISTING_SOURCES})
    message(STATUS "Network: Found ${CMAKE_LIST_LENGTH} source files")
else()
    # Create placeholder files for development
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/network_placeholder.c" 
        "/* SPDX-FileCopyrightText: 2025 Rhett Creighton\n"
        " * SPDX-License-Identifier: Apache-2.0\n"
        " */\n\n"
        "// Placeholder for network implementation\n"
        "void poker_network_placeholder(void) {}\n"
    )
    
    add_library(poker_network STATIC
        "${CMAKE_CURRENT_BINARY_DIR}/network_placeholder.c"
    )
    
    message(STATUS "Network: Using placeholder (implementing P2P network)")
endif()

target_include_directories(poker_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/ai/include
)

target_link_libraries(poker_network PUBLIC
    poker_common
    pthread
    m  # Math library for crypto operations
)

# Optional dependencies for full implementation
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(poker_network PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(poker_network PRIVATE HAVE_OPENSSL=1)
endif()

# Check for Tor library (for production)
find_library(TOR_LIBRARY tor QUIET)
if(TOR_LIBRARY)
    target_link_libraries(poker_network PRIVATE ${TOR_LIBRARY})
    target_compile_definitions(poker_network PRIVATE HAVE_TOR=1)
endif()