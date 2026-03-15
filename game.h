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

#define OBJ_TILE_STAR            304
#define OBJ_TILE_BULLET          305
#define OBJ_TILE_EBULLET         306
#define OBJ_TILE_BALLOON         320
#define OBJ_TILE_DOOR_CLOSED     400
#define OBJ_TILE_DOOR_OPEN_TOP   416
#define OBJ_TILE_DOOR_OPEN_BOT   432

#define DIR_LEFT  0
#define DIR_RIGHT 1

#define SCORE_BALLOON_BONUS 100
#define SCORE_ENEMY_KILL    200

#define PLAYER_FLOAT_ONE_LIFE     -4
#define PLAYER_FLOAT_TWO_LIVES    -6
#define PLAYER_FLOAT_THREE_LIVES  -8
#define PLAYER_GRAVITY             1
#define PLAYER_MAX_FALL_SPEED      4

typedef enum {
    STATE_START,
    STATE_LEVEL1_INTRO,
    STATE_LEVEL1,
    STATE_LEVEL2_INTRO,
    STATE_LEVEL2,
    STATE_PAUSE,
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
extern int level2BalloonsRemaining;
extern int enemiesRemaining;
extern int level1HOff;
extern int level1VOff;
extern int level2HOff;
extern int level2VOff;
extern int menuNeedsRedraw;
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