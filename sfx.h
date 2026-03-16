#ifndef SFX_H
#define SFX_H

#include "analogSound.h"

// Call once at program start
void sfxInit(void);

// Existing simple wrappers
void sfxShoot(void);
void sfxHit(void);
void sfxBomb(void);
void sfxWin(void);
void sfxLose(void);
void sfxPowerUp(void);

// New game-specific wrappers
void sfxEnemyShoot(void);
void sfxDoorAppear(void);
void sfxDoorOpen(void);
void sfxDoorEnter(void);
void sfxBalloon(void);
void sfxStarHit(void);
void sfxEnemyDrop(void);
void sfxEnemyStomp(void);

#endif