# Sprite Rendering Experiments

## Hypothesis
The card sprites are blurry because I'm not using the optimal combination of:
1. Blitter type
2. Scaling method  
3. Image dimensions
4. Terminal capabilities

## Experiments to Run

### Experiment 1: Blitter Comparison
**Question**: Which blitter produces the clearest card images?
**Test**: Render same card with all available blitters side by side
**Variables**: NCBLIT_PIXEL, NCBLIT_2x1, NCBLIT_2x2, NCBLIT_BRAILLE, NCBLIT_1x1

### Experiment 2: Scaling Method Comparison  
**Question**: Is pre-resizing vs. notcurses scaling better?
**Test**: Compare ncvisual_resize() vs NCSCALE_SCALE vs NCSCALE_STRETCH
**Variables**: Pre-resize to different sizes, let notcurses handle scaling

### Experiment 3: Optimal Dimensions
**Question**: What card size produces the clearest result?
**Test**: Try different dimensions (4x8, 6x10, 8x12, 10x15)
**Variables**: Width/height ratios that match card proportions

### Experiment 4: Terminal Capability Detection
**Question**: What does this terminal actually support?
**Test**: Query notcurses capabilities and use the best available
**Variables**: Pixel vs character rendering capabilities

### Experiment 5: Image Quality Investigation
**Question**: Are the source images the problem?
**Test**: Try different source images, formats, and processing
**Variables**: Different PNG files, maybe convert to smaller format first

## Success Criteria
- Cards should be clearly readable
- Suits and ranks should be distinct
- No excessive blurriness or artifacts
- Good contrast and color representation