# Changelog

All notable changes to this project will be documented in this file.

## [1.3.0] - 2025-06-05

### Added
- Complete tournament system with multi-table support
  - Blind level progression and antes
  - Automatic table balancing
  - Configurable payout structures
  - Late registration support
  - Tournament templates
- Comprehensive AI demo programs
  - AI personality showcase demonstrating 7 different playing styles
  - AI skill levels demo showing progression from beginner to expert
  - Head-to-head matches and tournaments
- Hand history replay functionality
  - PHH (Poker Hand History) format support
  - Action-by-action replay capabilities
  - Integration with hand history analyzer
- New demo programs:
  - `tournament_showcase` - Full tournament features demonstration
  - `multi_table_tournament_demo` - MTT simulation with 100 players
  - `ai_personality_showcase` - All AI personality types
  - `ai_skill_levels_demo` - Skill progression demonstration
  - `poker_variants_showcase` - All poker variants demo

### Fixed
- Test suite segmentation faults caused by `hand_eval_init()` issues
  - Made initialization idempotent (safe to call multiple times)
  - Added proper null checks throughout hand evaluation
  - Fixed memory allocation issues in lookup tables
- Persistence demo crash during save operation
  - Fixed null pointer dereference with deck
  - Corrected file open modes for checksum calculation
  - Updated game state allocation in load function
- Game state management
  - Added proper `hand_in_progress` flag management
  - Fixed dealer button and blinds initialization
- Build system warnings
  - Fixed strncpy buffer overflow warnings
  - Resolved type mismatches in server code
  - Added missing function implementations

### Changed
- Improved CMake build system organization
- Enhanced error handling with better error messages
- Updated AI decision-making algorithms for more realistic play
- Optimized hand evaluation performance

### Technical Details
- All 6 core tests now pass without segmentation faults
- Build system generates clean builds with minimal warnings
- Memory management improved with proper cleanup in all modules

## [1.2.0] - Previous Release

### Added
- AI system with personality-based decision making
- Opponent modeling and adaptive strategies
- Error handling system with thread-local contexts
- Logging system with file output and custom callbacks
- Game state persistence with checksums
- Auto-save manager with background thread
- Player statistics tracking

## [1.1.0] - Previous Release

### Added
- Core poker game engine
- Multiple poker variants (Texas Hold'em, Omaha, Stud, etc.)
- Basic AI players
- Network protocol for multiplayer
- Hand evaluation system