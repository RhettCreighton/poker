/*
 * EXPERIMENT 7: Minimal Notcurses PNG Test
 * 
 * Let's isolate exactly where notcurses fails with these PNGs
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

int main(void) {
    setlocale(LC_ALL, "");
    
    printf("EXPERIMENT 7: Minimal Notcurses PNG Test\n");
    printf("========================================\n\n");
    
    // Initialize notcurses
    printf("1. Initializing notcurses...\n");
    struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (!nc) {
        printf("✗ FAILED to initialize notcurses\n");
        return 1;
    }
    printf("✓ Notcurses initialized\n");
    
    // Test loading a PNG
    const char* test_file = "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png";
    printf("\n2. Loading PNG file: %s\n", test_file);
    
    struct ncvisual* ncv = ncvisual_from_file(test_file);
    if (!ncv) {
        printf("✗ FAILED to load PNG file\n");
        notcurses_stop(nc);
        return 1;
    }
    printf("✓ PNG file loaded successfully\n");
    
    // Get geometry
    printf("\n3. Getting image geometry...\n");
    ncvgeom geom;
    if (ncvisual_geom(nc, ncv, NULL, &geom) != 0) {
        printf("✗ FAILED to get geometry\n");
        ncvisual_destroy(ncv);
        notcurses_stop(nc);
        return 1;
    }
    printf("✓ Geometry: %ux%u pixels\n", geom.pixx, geom.pixy);
    
    // Test decode
    printf("\n4. Decoding image data...\n");
    if (ncvisual_decode(ncv) != 0) {
        printf("✗ FAILED to decode image\n");
        ncvisual_destroy(ncv);
        notcurses_stop(nc);
        return 1;
    }
    printf("✓ Image decoded successfully\n");
    
    // Test simple blit
    printf("\n5. Testing simple blit...\n");
    struct ncplane* std = notcurses_stdplane(nc);
    
    struct ncvisual_options vopts = {
        .blitter = NCBLIT_PIXEL,
        .scaling = NCSCALE_SCALE,
        .y = 5,
        .x = 5,
        .leny = 6,
        .lenx = 4,
        .n = std,
    };
    
    struct ncplane* result = ncvisual_blit(nc, ncv, &vopts);
    if (!result) {
        printf("✗ FAILED to blit with PIXEL blitter\n");
        
        // Try 2x1 fallback
        printf("   Trying 2x1 blitter...\n");
        vopts.blitter = NCBLIT_2x1;
        result = ncvisual_blit(nc, ncv, &vopts);
        if (!result) {
            printf("✗ FAILED to blit with 2x1 blitter\n");
            
            // Try 1x1 fallback
            printf("   Trying 1x1 blitter...\n");
            vopts.blitter = NCBLIT_1x1;
            result = ncvisual_blit(nc, ncv, &vopts);
            if (!result) {
                printf("✗ FAILED to blit with 1x1 blitter\n");
                printf("   ALL BLITTERS FAILED!\n");
            } else {
                printf("✓ 1x1 blitter worked\n");
            }
        } else {
            printf("✓ 2x1 blitter worked\n");
        }
    } else {
        printf("✓ PIXEL blitter worked\n");
    }
    
    if (result) {
        printf("\n6. Rendering to screen...\n");
        if (notcurses_render(nc) == 0) {
            printf("✓ Rendered successfully - you should see a card image!\n");
            printf("Press Enter to continue...\n");
            getchar();
        } else {
            printf("✗ Render failed\n");
        }
    }
    
    // Cleanup
    ncvisual_destroy(ncv);
    notcurses_stop(nc);
    
    printf("\nTEST COMPLETE\n");
    return 0;
}