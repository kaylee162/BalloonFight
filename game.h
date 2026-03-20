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
#define CM_BLOCKED 0
#define CM_OPEN    1
#define CM_KILL    2
#define CM_GOAL    3

#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

#define LEVEL1_MAP_W 32
#define LEVEL1_MAP_H 32
#define LEVEL2_MAP_W 64
#define LEVEL2_MAP_H 32

#define LEVEL1_PIXEL_W (LEVEL1_MAP_W * 8)
#define LEVEL1_PIXEL_H (LEVEL1_MAP_H * 8)
#define LEVEL2_PIXEL_W (LEVEL2_MAP_W * 8)
#define LEVEL2_PIXEL_H (LEVEL2_MAP_H * 8)

#define HUD_SCREENBLOCK 28
#define HUD_CHARBLOCK 0
#define LEVEL_SCREENBLOCK 24
#define LEVEL_CHARBLOCK 1

#define TILE_BLANK      0
#define TILE_SOLID      1
#define TILE_PLATFORM   2
#define TILE_SKY        3
#define TILE_CLOUD      4
#define TILE_STARBG     5
#define TILE_DOOR       6
#define TILE_BALLOONBG  7

#define FONT_BASE_TILE 32

// OBJ tile layout
// Player right:  0   - 63
// Player left:   64  - 127
// Player up:     128 - 191
// Player down:   192 - 255
// Enemy:         256 - 303
// Star:          304
// Bullets:       305 - 306
// Balloons:      320 - 383
// Door:          400+

#define OBJ_TILE_STAR            304
#define OBJ_TILE_BULLET          305
#define OBJ_TILE_EBULLET         306
#define OBJ_TILE_BALLOON         320
#define OBJ_TILE_DOOR_CLOSED     400
#define OBJ_TILE_DOOR_OPEN_TOP   416
#define OBJ_TILE_DOOR_OPEN_BOT   432

#define DIR_LEFT  0
#define DIR_RIGHT 1
#define DIR_UP    2
#define DIR_DOWN  3

// Animation: 4 frames per direction, each frame is a 2x2 block of 8x8 tiles
// (the duck sprite is 16x16).  Between each direction row in the spritesheet
// there is a 1-tile (8-pixel) gap column, so each direction occupies
// 4 frames * 4 tiles + 4 gap tiles = 20 tile slots in 1D VRAM layout.
// Adjust PLAYER_DIR_STRIDE if your grit export uses different spacing.
#define PLAYER_FRAMES         4
#define PLAYER_TILES_PER_FRAME 4   /* 2x2 tiles per 16x16 frame          */
#define PLAYER_FRAME_STRIDE   5    /* tiles between frame starts (4 + 1 gap column) */
#define PLAYER_DIR_STRIDE    20    /* tiles between direction row starts  */

#define SCORE_BALLOON_BONUS 100
#define SCORE_ENEMY_KILL    200

#define PLAYER_FLOAT_ONE_LIFE     -4
#define PLAYER_FLOAT_TWO_LIVES    -6
#define PLAYER_FLOAT_THREE_LIVES  -8
#define PLAYER_GRAVITY             1
#define PLAYER_MAX_FALL_SPEED      4

// ======================================================
//                       VRAM HELPERS
// ======================================================

// BG tile memory starts at 0x06000000
#define BG_TILE_MEM ((u32*) 0x06000000)

// OBJ tile memory starts at 0x06010000
#define OBJ_TILE_MEM ((u32*) 0x06010000)

// OBJ palette already exists in sprites.h as SPRITE_PAL

// Useful clamp macro
#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

// Level dimensions in tiles
#define LEVEL1_MAP_W 32
#define LEVEL1_MAP_H 32
#define LEVEL2_MAP_W 64
#define LEVEL2_MAP_H 32

// Level dimensions in pixels
#define LEVEL1_PIXEL_W (LEVEL1_MAP_W * 8)
#define LEVEL1_PIXEL_H (LEVEL1_MAP_H * 8)
#define LEVEL2_PIXEL_W (LEVEL2_MAP_W * 8)
#define LEVEL2_PIXEL_H (LEVEL2_MAP_H * 8)

// HUD screenblock and charblock choices
#define HUD_SCREENBLOCK 28
#define HUD_CHARBLOCK 0

// Gameplay background screenblock choices
#define LEVEL_SCREENBLOCK 24
#define LEVEL_CHARBLOCK 1

// Simple tile indices for BG tiles
#define TILE_BLANK      0
#define TILE_SOLID      1
#define TILE_PLATFORM   2
#define TILE_SKY        3
#define TILE_CLOUD      4
#define TILE_STARBG     5
#define TILE_DOOR       6
#define TILE_BALLOONBG  7

// Font tile base for BG0
#define FONT_BASE_TILE 32

// ======================================================
// OBJ tile layout in 1D sprite memory
// 32x32 sprite = 16 tiles
// 8x8 sprite   = 1 tile
// ======================================================
#define OBJ_TILE_PLAYER_RIGHT   0
#define OBJ_TILE_PLAYER_LEFT    64
#define OBJ_TILE_PLAYER_UP      128
#define OBJ_TILE_PLAYER_DOWN    192

#define OBJ_TILE_ENEMY          256

// Player directions
#define DIR_LEFT  0
#define DIR_RIGHT 1

// Score values
#define SCORE_BALLOON_BONUS 100
#define SCORE_ENEMY_KILL    200

typedef enum {
    STATE_START,
    STATE_LEVEL1_INTRO,
    STATE_LEVEL1,
    STATE_LEVEL2_INTRO,
    STATE_LEVEL2,
    STATE_PAUSE,
    STATE_SCOREBOARD,
    STATE_WIN,
    STATE_LOSE
} GameState;

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

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int active;
    int spriteVariant;
} Balloon;

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

typedef enum {
    ENEMY_FLOATING,
    ENEMY_WALKING,
    ENEMY_DEAD
} EnemyPhase;

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
    int phase;
    int grounded;
    int landingY;
} Enemy;

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
    int floatBoostTimer;
} Player;

// Shared global game data
extern GameState state;
extern GameState pausedState;
extern Player player;
extern Bullet playerBullets[MAX_PLAYER_BULLETS];
extern Bullet enemyBullets[MAX_ENEMY_BULLETS];
extern Enemy enemies[MAX_ENEMIES];
extern Balloon balloons[MAX_BALLOONS];
extern Star stars[MAX_STARS];
extern int score;
extern int highScore;
extern int level2BalloonsRemaining;
extern int enemiesRemaining;
extern int level1HOff;
extern int level1VOff;
extern int level2HOff;
extern int level2VOff;
extern int menuNeedsRedraw;
extern int pendingLevel1Load;
extern int pendingLevel2Load;
extern int doorVisible;
extern int doorOpen;
extern int doorX;
extern int doorY;
extern int frameCount;

// Shared helpers
void clearHUD(void);
void drawHUD(void);
void resetPlayerForCurrentLevel(void);
void updatePlayerAnimation(void);
void firePlayerBullet(void);
void updatePlayerBullets(void);
void updateEnemyBullets(void);
void updateEnemies(void);
void drawPlayerSprite(int screenX, int screenY);
void drawEnemySprite(int enemyIndex, int oamIndex, int screenX, int screenY);
void drawBulletSprite(int oamIndex, int screenX, int screenY, int enemyBullet);
void drawBalloonSprite(int oamIndex, int screenX, int screenY, int variant);
void drawStarSprite(int oamIndex, int screenX, int screenY);
void drawDoorSprite(int screenX, int screenY);
void hideUnusedSpritesFrom(int startIndex);
void damagePlayer(void);
int nextRandomSpriteVariant(void);

u8 getCollisionPixel(int x, int y);
int isSolidPixel(int x, int y);
int isHazardPixel(int x, int y);
int isGoalPixel(int x, int y);
int rectTouchesColor(int x, int y, int width, int height, u8 color);
int canMoveTo(int x, int y, int width, int height);
int findGroundYBelow(int x, int y, int width, int height);
int findLowestGroundYAtX(int x, int width, int height);
int absInt(int x);
int getFloatVelocityForLives(int lives);

// Main game interface
void initGame(void);
void updateGame(void);
void drawGame(void);

#endif