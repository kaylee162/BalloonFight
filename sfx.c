#include "sfx.h"
#include "analogSound.h"

void sfxInit(void) {
    initSound();
}

// ------------------------------------------------------
// Existing wrappers
// ------------------------------------------------------
void sfxShoot(void)   { playSfxPreset(SFXP_SHOOT); }
void sfxHit(void)     { playSfxPreset(SFXP_HIT); }
void sfxBomb(void)    { playSfxPreset(SFXP_BOMB); }
void sfxPowerUp(void) { playSfxPreset(SFXP_POWERUP); }   // fixed name
void sfxWin(void)     { playSfxPreset(SFXP_WIN); }
void sfxLose(void)    { playSfxPreset(SFXP_LOSE); }

// ------------------------------------------------------
// New event sounds
// Keep them short/simple and in the same style as HW05.
// ------------------------------------------------------

// Enemy bullet fired
void sfxEnemyShoot(void) {
    playChannel2(NOTE_C5, 8, 1, 0, 1);
}

// Door appears after all enemies are cleared
void sfxDoorAppear(void) {
    playChannel1(NOTE_C5, 18, 0, 0, 0, 2, 0, 2);
    playChannel2(NOTE_G5, 18, 2, 0, 2);
}

// First touch opens the door
void sfxDoorOpen(void) {
    playChannel1(NOTE_E5, 12, 0, 0, 0, 1, 0, 2);
    playChannel2(NOTE_B5, 12, 1, 0, 2);
}

// Entering the open door
void sfxDoorEnter(void) {
    playChannel1(NOTE_G5, 10, 0, 0, 0, 1, 0, 2);
    playChannel2(NOTE_C6, 14, 1, 0, 2);
}

// Collecting a balloon / pickup
void sfxBalloon(void) {
    playChannel2(NOTE_A5, 8, 1, 0, 1);
    playChannel1(NOTE_C6, 10, 0, 0, 0, 1, 0, 1);
}

// Hit by a star in level 2
void sfxStarHit(void) {
    playChannel1(NOTE_DS5, 10, 3, 2, 1, 2, 0, 2);
    playDrumSound(1, 5, 1, 10, 1);
}

// Enemy drops from floating phase to walking phase
void sfxEnemyDrop(void) {
    playChannel1(NOTE_G4, 12, 2, 2, 1, 2, 0, 2);
}

// Enemy stomp / finish-off
void sfxEnemyStomp(void) {
    playDrumSound(2, 4, 1, 16, 1);
    playChannel2(NOTE_C4, 8, 1, 0, 2);
}