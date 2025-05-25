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

# Benchmark configuration

# Simple timing utilities
add_library(bench_utils STATIC
    bench_utils.c
)

# Hand evaluation benchmark
add_executable(bench_hand_eval
    bench_hand_eval.c
)
target_link_libraries(bench_hand_eval poker_common bench_utils m)
target_include_directories(bench_hand_eval PRIVATE
    ${CMAKE_SOURCE_DIR}/common/include
)

# Rendering benchmark
add_executable(bench_rendering
    bench_rendering.c
    ${CMAKE_SOURCE_DIR}/src/ui/rendering.c
)
target_link_libraries(bench_rendering bench_utils ${NOTCURSES_LIBRARIES} m)

# Full game benchmark
add_executable(bench_full_game
    bench_full_game.c
    ${POKER_SOURCES}
)
target_link_libraries(bench_full_game bench_utils ${NOTCURSES_LIBRARIES} m pthread)

# Combined benchmark runner
add_custom_target(benchmark
    COMMAND bench_hand_eval
    COMMAND bench_rendering
    COMMAND bench_full_game
    DEPENDS bench_hand_eval bench_rendering bench_full_game
    COMMENT "Running all benchmarks..."
)