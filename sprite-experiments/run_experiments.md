# Sprint Experiments - Run These and Report Back!

I've created focused experiments to diagnose the blurry sprite issue. **Only you can see the graphics output**, so please run these and tell me what you observe:

## Experiment 4 - The Key Test (Run This First!)
```bash
cd /home/bob/projects/custom-notcurses-wip/poker/sprite-experiments
./exp04_pixel_vs_character
```

**This tests my main hypothesis**: Maybe PIXEL blitter actually works better than I thought, and pre-resizing is causing the blurriness.

**Questions for you to answer:**
1. Which of the 3 methods produces the CLEAREST, most readable cards?
   - Method 1: PIXEL blitter
   - Method 2: 2x1 blitter  
   - Method 3: PRE-RESIZE 2x1 (what I was doing in the broken demo)

2. Is the PIXEL blitter actually better quality than character blitters?
3. Is pre-resizing (Method 3) causing the blurriness problem?
4. Can you clearly read the letters and see the suit symbols in each method?

## If You Have Time - Additional Experiments:
```bash
./exp01_blitter_comparison  # Compare all available blitters
./exp02_scaling_methods     # Compare different scaling approaches  
./exp03_terminal_capabilities  # What your terminal supports
```

## What I Need from You:
Just tell me:
- **Which method in exp04 looks best?**
- **Is PIXEL blitter clearer than 2x1?**
- **Is pre-resizing the cause of blurriness?**

Based on your observations, I'll fix the sprite demo and update CLAUDE.md with the correct approach!