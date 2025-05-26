# Poker Assets Directory

This directory contains all visual assets for the poker game.

## Directory Structure

```
assets/
├── backgrounds/
│   └── poker-background.jpg    # Main poker table background (REQUIRED)
├── sprites/
│   └── cards/                  # Symlink to card PNG files
└── README.md                   # This file
```

## Asset Locations

### Background Image
- **File**: `backgrounds/poker-background.jpg`
- **Usage**: Main poker table background
- **Required**: YES - Game uses this specific file

### Card Sprites
- **Location**: `sprites/cards/` (symlink to `../SVGCards/Decks/Accessible/Horizontal/pngs/`)
- **Format**: PNG files, 3x5 display size optimal
- **Naming**: camelCase (e.g., `spadeAce.png`, `heart10.png`, `blueBack.png`)

### Card File Naming Convention
- **Suits**: spade, heart, club, diamond
- **Ranks**: Ace, 2-10, Jack, Queen, King
- **Examples**: 
  - `spadeAce.png`
  - `heart10.png`
  - `clubQueen.png`
  - `diamondKing.png`
- **Card backs**: `blueBack.png`, `redBack.png`
- **Special**: `blackJoker.png`, `redJoker.png`

## Usage in Code

```c
// Background
const char* BG_PATH = "assets/backgrounds/poker-background.jpg";

// Cards
const char* CARD_PATH = "assets/sprites/cards/spadeAce.png";
const char* BLUE_BACK = "assets/sprites/cards/blueBack.png";
const char* RED_BACK = "assets/sprites/cards/redBack.png";
```

## Important Notes

1. **Always use relative paths** from the poker directory
2. **Card size**: Display at 3x5 cells (user preference confirmed)
3. **Background**: Must use dedicated ncplane to prevent blurring
4. **Caching**: Recommended for performance (6% improvement)

See `sprite-experiments/` for working examples and patterns.