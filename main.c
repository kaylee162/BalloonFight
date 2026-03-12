#include "gba.h"
#include "mode0.h"
#include "sprites.h"
#include "print.h"
#include "game.h"

// These globals are declared as extern in gba.h
// They must be defined exactly once somewhere in the project
unsigned short oldButtons;
unsigned short buttons;

// This global is declared as extern in sprites.h
// It is the shadow copy of OAM that we DMA into real OAM every frame
OBJ_ATTR shadowOAM[128];

// These functions will live in game.c later
// For now, we forward declare them so main.c is ready to use them
void initGame(void);
void updateGame(void);
void drawGame(void);

int main() {
    // Open the mGBA debug console if available
    mgba_open();

    // Initialize button states before the loop starts
    buttons = REG_BUTTONS;
    oldButtons = buttons;

    // Set up the display in Mode 0 with backgrounds and sprites enabled
    initMode0();

    // Hide every sprite before the game starts
    hideSprites();

    // Copy hidden sprite data into the real OAM
    DMANow(3, shadowOAM, OAM, 128 * 4);

    // Let the future game code initialize all gameplay data
    initGame();

    while (1) {
        // Save the previous frame's input
        oldButtons = buttons;

        // Read the current frame's input
        buttons = REG_BUTTONS;

        // Run the current state's update logic
        updateGame();

        // Wait until VBlank before touching VRAM / OAM
        waitForVBlank();

        // Draw the current frame
        drawGame();

        // Push the shadow OAM into real OAM once per frame
        DMANow(3, shadowOAM, OAM, 128 * 4);
    }

    return 0;
}