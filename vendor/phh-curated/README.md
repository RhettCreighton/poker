# Curated PHH Collection

This is a curated collection of approximately 25MB of the most interesting poker hands from the PHH dataset.

## Categories:

1. **Famous Individual Hands**
   - antonius-blom-2009.phh - Famous pot between Patrik Antonius and Viktor Blom
   - dwan-ivey-2009.phh - Tom Dwan vs Phil Ivey on Full Tilt
   - arieh-yockey-2019.phh - WSOP final table hand
   - phua-xuan-2019.phh - Triton high roller

2. **Pluribus AI Hands** 
   - 500 hands from the Pluribus AI experiments
   - 6-player games with advanced AI

3. **WSOP Tournament Hands**
   - Final table hands from major WSOP events

4. **High Stakes Cash Games**
   - $1000NL hands from PokerStars, Full Tilt, and IPN
   - Large pots and interesting action

## Format

All hands are in PHH (Poker Hand History) format, which is a TOML-based standard for storing poker hands.

## Usage

These hands can be parsed using the PHH parser in our codebase:
```c
PHHHand* hand = phh_parse_file("path/to/hand.phh");
```

Or converted to our internal format:
```c
HandHistory* hh = phh_to_hand_history(hand);
```
