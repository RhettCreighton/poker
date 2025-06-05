# PHH Curated Collection Index

This collection contains 339 carefully selected poker hands from various sources.

## Collection Statistics

- **Total Size**: ~3.2 MB
- **Total Hands**: 339
- **Famous Individual Hands**: 5
- **Pluribus AI Hands**: 184
- **WSOP Tournament Hands**: 83
- **High Stakes Cash Game Hands**: 67

## Hand Categories

### 1. Famous Individual Hands
These are historically significant hands featuring professional players:
- `antonius-blom-2009.phh` - The famous Patrik Antonius vs Viktor Blom pot
- `dwan-ivey-2009.phh` - Tom Dwan vs Phil Ivey on Full Tilt 
- `arieh-yockey-2019.phh` - WSOP $50k PPC final table
- `phua-xuan-2019.phh` - Triton London high roller
- `alice-carol-wikipedia.phh` - Example hand from Wikipedia

### 2. Pluribus AI Collection (pluribus_ai/)
184 hands from the groundbreaking Pluribus AI experiment:
- 6-player No Limit Hold'em games
- AI vs professional players
- Advanced strategy demonstrations
- Various stack depths from 30BB to 300BB

### 3. WSOP Tournament Hands (wsop/)
83 hands from the 2023 WSOP Event #43 final table:
- High-pressure tournament situations
- ICM considerations
- Short stack dynamics
- Big blind ante format

### 4. High Stakes Cash Games (high_stakes/)
67 hands from $1000NL online games:
- Deep stack play (100BB+)
- Complex multi-street decisions
- Large pots relative to blinds

## Usage Examples

### Parse a single hand:
```c
PHHHand* hand = phh_parse_file("phh-curated/antonius-blom-2009.phh");
```

### Analyze all Pluribus hands:
```c
DIR* dir = opendir("phh-curated/pluribus_ai");
struct dirent* entry;
while ((entry = readdir(dir)) != NULL) {
    if (strstr(entry->d_name, ".phh")) {
        char path[512];
        snprintf(path, sizeof(path), "phh-curated/pluribus_ai/%s", entry->d_name);
        PHHHand* hand = phh_parse_file(path);
        // Analyze hand...
        phh_destroy(hand);
    }
}
```

### Convert to internal format:
```c
PHHHand* phh = phh_parse_file("phh-curated/wsop/123.phh");
HandHistory* hh = phh_to_hand_history(phh);
// Use our internal hand history functions
hand_history_print(hh);
hand_history_destroy(hh);
phh_destroy(phh);
```

## Notable Hands

### Largest Pots:
1. `dwan-ivey-2009.phh` - $1,454,700 pot
2. `arieh-yockey-2019.phh` - $1,480,000 pot
3. `antonius-blom-2009.phh` - $1,425,000 pot

### Most Complex:
- Pluribus hands demonstrate advanced GTO concepts
- WSOP hands show tournament-specific strategies
- High stakes hands feature deep stack complexities

## Integration with P2P Network

All hands played on our P2P poker network are automatically exported in PHH format using the integrated export functionality. This ensures compatibility with industry-standard tools and long-term preservation of hand histories.