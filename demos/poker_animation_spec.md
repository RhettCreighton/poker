# Professional Online Poker Animation Specification
## 2-7 Lowball - Complete Hand Playback Requirements

### Table Setup Phase (Tests 1-10)
1. **Table Initialization**: Table appears with smooth fade-in (500ms)
2. **Table Geometry**: Perfect oval, no pixelation, proper aspect ratio
3. **Felt Texture**: Subtle gradient from center, darker at edges
4. **Rail Rendering**: 3D-effect rail around table edge with highlight
5. **Table Logo**: Centered logo/branding, semi-transparent
6. **Lighting Effect**: Subtle spotlight effect on table center
7. **Seat Markers**: 10 clearly marked seat positions, evenly distributed
8. **Dealer Button**: Visible dealer button at correct position
9. **Pot Area**: Designated central area for pot chips
10. **Background**: Dark gradient background, no distractions

### Player Seating Phase (Tests 11-20)
11. **Player Arrival**: Players fade in sequentially (200ms each)
12. **Seat Spacing**: Minimum 15% screen width between adjacent players
13. **Info Box Position**: Player info boxes don't overlap cards or each other
14. **Info Box Size**: Consistent sizing, scales with screen
15. **Avatar Display**: Player avatar/photo in info box
16. **Chip Stack Display**: Visible chip count with proper formatting ($10,000)
17. **Name Display**: Player names truncated if too long, with ellipsis
18. **Status Indicators**: Clear "sitting out" or "away" status
19. **Hero Highlighting**: Hero player has distinct border/glow
20. **Seat Numbers**: Small seat numbers visible (1-10)

### Pre-Deal Phase (Tests 21-30)
21. **Blind Posts**: Small/big blind animations from players to pot
22. **Blind Deduction**: Chip counts update smoothly
23. **Pot Creation**: Pot appears with fade-in at center
24. **Dealer Button Movement**: Smooth rotation to next dealer
25. **Shuffle Animation**: Brief shuffle effect at dealer position
26. **Deck Appearance**: Deck appears at dealer spot
27. **Ante Collection**: If applicable, antes animate to pot
28. **Sound Sync**: Chip sounds synchronized with movements
29. **Timer Start**: Action timer appears for first player
30. **UI Ready State**: All UI elements in position before deal

### Card Dealing Phase (Tests 31-40)
31. **Deal Order**: Cards dealt clockwise from dealer's left
32. **Deal Speed**: Each card takes 150-200ms to reach player
33. **Card Trajectory**: Smooth arc from dealer to player, not straight line
34. **Card Rotation**: Slight rotation during flight for realism
35. **Deal Rhythm**: Consistent timing between cards (50ms pause)
36. **Card Arrival**: Cards slide into exact positions, no overlap
37. **Card Spacing**: Exactly 2-3 pixels between cards horizontally
38. **Card Alignment**: All cards perfectly aligned vertically
39. **Face Down**: All cards dealt face down initially
40. **Z-Order**: Later cards appear on top of earlier cards

### Hero Cards Reveal (Tests 41-45)
41. **Reveal Delay**: 500ms pause after all cards dealt
42. **Flip Animation**: Cards flip with 3D effect (200ms each)
43. **Sequential Flip**: Cards flip left-to-right, not simultaneously
44. **Flip Sound**: Card flip sound for each card
45. **Final Position**: Cards remain in exact dealt positions

### Betting Round Animation (Tests 46-55)
46. **Action Indicator**: Green glow/border on active player
47. **Timer Display**: Countdown timer visible (30s default)
48. **Timer Warning**: Timer turns red at 5 seconds
49. **Fold Animation**: Cards darken and shrink when folding
50. **Bet Animation**: Chips slide from player to center
51. **Chip Stack**: Multiple denominations shown for large bets
52. **Bet Amount Display**: Floating text shows bet amount
53. **Pot Update**: Pot total updates after each action
54. **Action Log**: Small action text appears briefly ("Mike raises to $400")
55. **Turn Transition**: 200ms pause between player actions

### Draw Phase (Tests 56-65)
56. **Discard Selection**: Selected cards highlight with glow
57. **Discard Animation**: Cards slide back to dealer and fade
58. **Draw Animation**: New cards dealt same as initial deal
59. **Draw Order**: Players draw in betting order
60. **Card Replacement**: New cards appear in exact same positions
61. **Mucked Cards**: Discarded cards disappear cleanly
62. **Draw Count Display**: "Drawing 2" indicator per player
63. **Deck Depletion**: Deck visually smaller after draws
64. **Simultaneous Draws**: Multiple players can draw at once
65. **Draw Completion**: Clear indication when draw phase ends

### Showdown Phase (Tests 66-75)
66. **Showdown Order**: Players reveal in correct order
67. **Card Reveal**: Smooth flip animation for each hand
68. **Hand Highlighting**: Winning cards glow/highlight
69. **Hand Rank Display**: "7-5-4-3-2" appears below cards
70. **Winner Indication**: Winner's info box glows gold
71. **Pot Award**: Pot slides to winner with chip sound
72. **Split Pot**: Pot divides evenly if tied
73. **Muck Option**: Losing players can muck without showing
74. **Hand History**: Small log shows all shown hands
75. **Celebration**: Brief winner animation (confetti/glow)

### Hand Completion Phase (Tests 76-85)
76. **Card Collection**: All cards slide back to dealer
77. **Collection Order**: Mucked cards first, then shown cards
78. **Pot Settlement**: Winner's chips update smoothly
79. **Rake Display**: If applicable, rake amount shown
80. **Hand Number**: Hand # increments in corner
81. **Statistics Update**: Player stats update (VPIP, etc.)
82. **Break Timer**: If tournament, break timer shown
83. **Seat Open**: Eliminated players' seats marked "OPEN"
84. **Chip Stack Arrange**: Stacks reorganize if very large
85. **Ready State**: Table returns to clean state for next hand

### UI/UX Consistency (Tests 86-95)
86. **Font Consistency**: All text uses same font family
87. **Color Scheme**: Consistent color palette throughout
88. **Animation Timing**: All animations under 500ms
89. **No Overlaps**: No UI elements ever overlap
90. **Responsive Scaling**: Adapts to terminal size changes
91. **Z-Order Consistency**: Proper layering maintained always
92. **Smooth Transitions**: No jarring movements
93. **Loading States**: Clear feedback during any delays
94. **Error Recovery**: Graceful handling of missing assets
95. **Performance**: Maintains 30+ FPS throughout

### Professional Polish (Tests 96-100)
96. **Chip Denominations**: Realistic chip colors ($1=white, $5=red, etc.)
97. **Card Back Design**: Professional card back pattern
98. **Table Branding**: Subtle site logo on felt
99. **Anti-Aliasing**: Smooth edges on all graphics
100. **Production Quality**: Matches PokerStars/GGPoker quality

---

## Test Implementation Strategy

Each test will check:
1. **Visual Correctness**: Elements appear as specified
2. **Timing Accuracy**: Animations complete within specified duration
3. **Positioning**: Pixel-perfect placement
4. **State Consistency**: Game state matches visual state
5. **Performance**: No frame drops during animation