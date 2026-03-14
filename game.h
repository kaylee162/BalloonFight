#ifndef GAME_H
#define GAME_H

#include "gba.h"
#include "mode0.h"
#include "sprites.h"

// Maximum counts for gameplay objects
#define MAX_PLAYER_BULLETS 5
#define MAX_ENEMY_BULLETS 8
#define MAX_ENEMIES 4
#define MAX_BALLOONS 12
#define MAX_STARS 8

// Useful gameplay constants
#define PLAYER_START_X 20
#define PLAYER_START_Y 112
#define PLAYER_WIDTH 16
#define PLAYER_HEIGHT 16

// Level 1 collision map color indices
#define CM_BLOCKED 0   // black
#define CM_OPEN    1   // white
#define CM_LADDER  2   // blue
#define CM_KILL    3   // red
#define CM_GOAL    4   // green

// Game states required by the assignment PDF
typedef enum {
    STATE_START,
    STATE_LEVEL1,
    STATE_LEVEL2,
    STATE_PAUSE,
    STATE_WIN,
    STATE_LOSE
} GameState;

// Generic projectile
typedef struct {
    int x;
    int y;
    int oldX;
    int oldY;
    int xVel;
    int yVel;
    int width;
    int height;
    int active;
} Bullet;

// Balloon collectible
typedef struct {
    int x;
    int y;
    int width;
    int height;
    int active;
} Balloon;

// Star hazard for level 2
typedef struct {
    int x;
    int y;
    int oldY;
    int width;
    int height;
    int yVel;
    int minY;
    int maxY;
    int active;
} Star;

// Enemy phases for level 1
typedef enum {
    ENEMY_FLOATING,
    ENEMY_WALKING,
    ENEMY_DEAD
} EnemyPhase;

// Enemy object
typedef struct {
    int x;
    int y;
    int oldX;
    int oldY;
    int xVel;
    int yVel;
    int width;
    int height;
    int minX;
    int maxX;
    int direction;
    int active;
    int shootTimer;
    int animCounter;
    int currentFrame;
    EnemyPhase phase;
} Enemy;

// Player object
typedef struct {
    int x;
    int y;
    int oldX;
    int oldY;
    int xVel;
    int yVel;
    int width;
    int height;
    int direction;
    int currentFrame;
    int animCounter;
    int isMoving;
    int grounded;
    int lives;
    int invincibleTimer;
    int onLadder;
} Player;

// Main game interface
void initGame(void);
void updateGame(void);
void drawGame(void);

#endif