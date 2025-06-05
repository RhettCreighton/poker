# Copyright 2025 Rhett Creighton
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.16)
project(PokerPlatform VERSION 1.0.0 LANGUAGES C)

# C99 standard for maximum compatibility
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

# Export compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_SERVER "Build poker server" ON)
option(BUILD_CLIENT "Build poker client" ON)
option(BUILD_AI "Build AI opponents" ON)
option(ENABLE_SANITIZERS "Enable sanitizers in debug builds" ON)

# Sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_SANITIZERS)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED notcurses)
find_package(Threads REQUIRED)

# Math library
find_library(M_LIBRARY m)

# Global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/variants
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    ${CMAKE_CURRENT_SOURCE_DIR}/client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/client/ui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ai/include
    ${NOTCURSES_INCLUDE_DIRS}
)

# Add subdirectories
add_subdirectory(common)
add_subdirectory(variants)
add_subdirectory(network)

if(BUILD_AI)
    add_subdirectory(ai)
endif()

if(BUILD_SERVER)
    add_subdirectory(server)
endif()

if(BUILD_CLIENT)
    add_subdirectory(client)
endif()

# Main executables
add_subdirectory(src/main)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/common/include/poker
    DESTINATION include
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/
    DESTINATION include/poker/variants
    FILES_MATCHING PATTERN "*.h"
)

# Standalone game - commented out as file doesn't exist
# add_executable(poker_game_standalone standalone_27_draw.c)
# target_link_libraries(poker_game_standalone ${M_LIBRARY})

# Assets (if they exist)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
        DESTINATION share/poker/assets
    )
endif()

# Documentation
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/CLAUDE.md
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION share/doc/poker
)

# CPack configuration
set(CPACK_PACKAGE_NAME "PokerPlatform")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Professional Terminal Poker Platform")
set(CPACK_PACKAGE_VENDOR "Rhett Creighton")
set(CPACK_PACKAGE_CONTACT "poker@terminal-casino.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Debian package specifics
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libnotcurses3 (>= 3.0)")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")

# RPM package specifics
set(CPACK_RPM_PACKAGE_REQUIRES "notcurses >= 3.0")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")

include(CPack)

# Summary message
message(STATUS "")
message(STATUS "PokerPlatform ${PROJECT_VERSION} Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  Server: ${BUILD_SERVER}")
message(STATUS "  Client: ${BUILD_CLIENT}")
message(STATUS "  AI: ${BUILD_AI}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Sanitizers: ${ENABLE_SANITIZERS}")
message(STATUS "")