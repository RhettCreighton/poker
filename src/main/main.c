/*
 * Copyright 2025 Rhett Creighton
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <notcurses/notcurses.h>

#include "poker/game/game.h"
#include "poker/ui/ui.h"

int main(int argc, char* argv[]) {
    // Set up UTF-8 locale for card suits
    setlocale(LC_ALL, "");
    
    printf("Terminal Poker v1.0.0\n");
    printf("The ultimate terminal poker experience!\n\n");
    
    // TODO: Initialize game
    // TODO: Initialize UI
    // TODO: Main game loop
    
    return 0;
}