# MVC Architecture for Terminal Poker Platform

## Overview

This modular architecture separates concerns to ensure:
- Beautiful animations never interfere with game logic
- Network code doesn't affect rendering
- Game state remains pure and testable
- UI can be swapped out (terminal, web, etc.)

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                     VIEW                            │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │  Renderer   │  │  Animations  │  │   Input   │ │
│  │  - Table    │  │  - Cards     │  │  Handler  │ │
│  │  - Players  │  │  - Chips     │  │           │ │
│  │  - Cards    │  │  - Effects   │  │           │ │
│  └─────────────┘  └──────────────┘  └───────────┘ │
└─────────────────────────────────────────────────────┘
                           ▲
                           │ Events/Updates
                           ▼
┌─────────────────────────────────────────────────────┐
│                   CONTROLLER                        │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │ Game Flow   │  │   Action     │  │   Event   │ │
│  │ Manager     │  │  Processor   │  │   Queue   │ │
│  └─────────────┘  └──────────────┘  └───────────┘ │
└─────────────────────────────────────────────────────┘
                           ▲
                           │ Commands/Queries
                           ▼
┌─────────────────────────────────────────────────────┐
│                     MODEL                           │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │ Game State  │  │   Players    │  │    AI     │ │
│  │  - Pot      │  │  - Chips     │  │ Decisions │ │
│  │  - Cards    │  │  - Actions   │  │           │ │
│  │  - Rules    │  │  - History   │  │           │ │
│  └─────────────┘  └──────────────┘  └───────────┘ │
└─────────────────────────────────────────────────────┘
```

## Key Principles

1. **Model** is pure data - no UI, no I/O
2. **View** only renders - no game logic
3. **Controller** coordinates - no direct rendering
4. **Events** flow up, **Updates** flow down
5. **Animations** are view-only concerns

## Module Structure

```
mvc/
├── model/
│   ├── game_state.h      # Core game state
│   ├── player.h          # Player data
│   ├── cards.h           # Card representation
│   ├── rules.h           # Game rules interface
│   └── ai_decision.h     # AI logic
├── view/
│   ├── renderer.h        # Main rendering interface
│   ├── animations.h      # Animation engine
│   ├── ui_components.h   # Reusable UI elements
│   ├── input_handler.h   # User input processing
│   └── themes.h          # Visual themes
├── controller/
│   ├── game_controller.h # Main game flow
│   ├── action_handler.h  # Process player actions
│   ├── event_bus.h       # Event system
│   └── state_machine.h   # Game state management
└── interfaces/
    ├── irenderer.h       # Renderer interface
    ├── igame_rules.h     # Rules interface
    └── iai_player.h      # AI interface
```

## Event Flow Example

1. User presses 'R' to raise
2. View's InputHandler captures key
3. Creates RaiseAction event
4. Controller receives event
5. Controller validates with Model
6. Model updates game state
7. Controller sends StateChanged event
8. View receives update
9. View triggers raise animation
10. Animation completes, ready for next input

## Benefits

- **Testable**: Model can be tested without UI
- **Flexible**: Can swap renderers (ncurses, SDL, web)
- **Maintainable**: Clear separation of concerns
- **Performant**: Animations don't block game logic
- **Networkable**: Model state can be serialized