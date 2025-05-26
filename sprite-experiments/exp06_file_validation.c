/*
 * EXPERIMENT 6: File Validation and Format Testing
 * 
 * DISCOVERY: All PNG renders are FAILING - let's find out why!
 * 
 * Question: Are these PNG files actually valid? What's wrong with them?
 */

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <sys/stat.h>

void check_file_details(const char* filename, int line) {
    struct stat st;
    FILE* fp = fopen(filename, "rb");
    
    printf("=== FILE: %s ===\n", filename);
    
    if (stat(filename, &st) == 0) {
        printf("File size: %ld bytes\n", st.st_size);
    } else {
        printf("ERROR: Cannot stat file\n");
        return;
    }
    
    if (!fp) {
        printf("ERROR: Cannot open file\n");
        return;
    }
    
    // Check PNG header
    unsigned char header[8];
    if (fread(header, 1, 8, fp) == 8) {
        printf("Header bytes: ");
        for (int i = 0; i < 8; i++) {
            printf("%02X ", header[i]);
        }
        printf("\n");
        
        // PNG signature should be: 89 50 4E 47 0D 0A 1A 0A
        if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
            printf("✓ Valid PNG signature\n");
        } else {
            printf("✗ INVALID PNG signature!\n");
        }
    }
    
    fclose(fp);
    
    // Test with notcurses
    struct notcurses_options opts = {.flags = NCOPTION_SUPPRESS_BANNERS};
    struct notcurses* nc = notcurses_init(&opts, NULL);
    if (nc) {
        struct ncvisual* ncv = ncvisual_from_file(filename);
        if (ncv) {
            printf("✓ notcurses can load the file\n");
            
            ncvgeom geom;
            if (ncvisual_geom(nc, ncv, NULL, &geom) == 0) {
                printf("✓ Geometry: %ux%u pixels\n", geom.pixx, geom.pixy);
            } else {
                printf("✗ Cannot get geometry\n");
            }
            
            if (ncvisual_decode(ncv) == 0) {
                printf("✓ Can decode image data\n");
            } else {
                printf("✗ Cannot decode image data\n");
            }
            
            ncvisual_destroy(ncv);
        } else {
            printf("✗ notcurses CANNOT load the file\n");
        }
        notcurses_stop(nc);
    }
    
    printf("\n");
}

int main(void) {
    printf("EXPERIMENT 6: File Validation and Format Testing\n");
    printf("=================================================\n\n");
    printf("Testing if these PNG files are actually valid...\n\n");
    
    // Test several files
    const char* test_files[] = {
        "../SVGCards/Decks/Accessible/Horizontal/pngs/spadeAce.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/heartKing.png",
        "../SVGCards/Decks/Accessible/Horizontal/pngs/blueBack.png"
    };
    
    for (int i = 0; i < 3; i++) {
        check_file_details(test_files[i], i);
    }
    
    printf("CONCLUSIONS:\n");
    printf("============\n");
    printf("1. Are the PNG files valid? (Check signatures above)\n");
    printf("2. Can notcurses load them? (Check load status above)\n");
    printf("3. Can notcurses decode them? (Check decode status above)\n");
    printf("4. What might be wrong with these files?\n");
    
    return 0;
}