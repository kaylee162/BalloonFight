#include "gba.h"
#include "mode0.h"
#include "sprites.h"
#include "print.h"
#include "game.h"
#include "levelone_logic.h"
#include "leveltwo_logic.h"

#include "tileset.h"
#include "levelone.h"
#include "collisionMapOne.h"

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

// Simple OBJ tile indices
#define OBJ_TILE_PLAYER0    0
#define OBJ_TILE_PLAYER1    4
#define OBJ_TILE_ENEMYF0    8
#define OBJ_TILE_ENEMYF1    12
#define OBJ_TILE_ENEMYW0    16
#define OBJ_TILE_ENEMYW1    20
#define OBJ_TILE_BULLET     24
#define OBJ_TILE_EBULLET    25
#define OBJ_TILE_BALLOON    26
#define OBJ_TILE_STAR       28
#define OBJ_TILE_DOOR       32

// Player directions
#define DIR_LEFT  0
#define DIR_RIGHT 1

// Score values
#define SCORE_BALLOON_BONUS 100
#define SCORE_ENEMY_KILL    200

// ======================================================
//                     GLOBAL GAME DATA
// ======================================================

// Current game state
GameState state;

// Player
Player player;

// Object pools
Bullet playerBullets[MAX_PLAYER_BULLETS];
Bullet enemyBullets[MAX_ENEMY_BULLETS];
Enemy enemies[MAX_ENEMIES];
Balloon balloons[MAX_BALLOONS];
Star stars[MAX_STARS];

// Score and progression
int score;
int level2BalloonsRemaining;
int enemiesRemaining;

// Level state
// Level state
int level1HOff;
int level1VOff;
int level2HOff;
int menuNeedsRedraw;

// Door state for finishing level 1
int doorVisible;
int doorX;
int doorY;

// Frame counter
int frameCount;

// ======================================================
//                   FORWARD DECLARATIONS
// ======================================================

// Asset / VRAM setup
static void initGraphics(void);
static void initPalettes(void);
static void buildHudTiles(void);
static void buildBackgroundTiles(void);
static void buildSpriteTiles(void);
static void clearObjTiles(void);

// Tile helpers
static void write4bppTile(u32* base, int tileIndex, const u8 pixels[64]);
static void makeSolidTile(u32* base, int tileIndex, u8 color);
static void makeCheckerTile(u32* base, int tileIndex, u8 c1, u8 c2);
static void makeGlyphTile(u32* base, int tileIndex, const u8 rows[8], u8 fg, u8 bg);

// BG helpers
void clearHUD(void);
void drawHUD(void);
static void putCharHUD(int col, int row, char c);
static void putStringHUD(int col, int row, const char* str);
static void putNumberHUD(int col, int row, int value, int digits);
static int glyphIndexForChar(char c);

// Menu screens
static void drawStartScreen(void);
static void drawPauseScreen(void);
static void drawWinScreen(void);
static void drawLoseScreen(void);

// Level setup
void resetPlayerForCurrentLevel(void);


// Update handlers
static void updateStart(void);
static void updatePause(void);
static void updateWin(void);
static void updateLose(void);

// Object updates
void updatePlayerAnimation(void);
void firePlayerBullet(void);
void updatePlayerBullets(void);
void updateEnemyBullets(void);
void updateEnemies(void);
static void spawnEnemyBullet(Enemy* e);

// Physics / collisions
u8 getCollisionPixel(int x, int y);
int isSolidPixel(int x, int y);
int canMoveTo(int x, int y, int width, int height);
int findGroundYBelow(int x, int y, int width, int height);
int findLowestGroundYAtX(int x, int width, int height);
void damagePlayer(void);

// Sprite drawing
void drawPlayerSprite(int screenX, int screenY);
void drawEnemySprite(int i, int screenX, int screenY);
void drawBulletSprite(int oamIndex, int screenX, int screenY, int enemyBullet);
void drawBalloonSprite(int oamIndex, int screenX, int screenY);
void drawStarSprite(int oamIndex, int screenX, int screenY);
void drawDoorSprite(int screenX, int screenY);

// Level One 
int rectTouchesColor(int x, int y, int width, int height, u8 color);
int isLadderPixel(int x, int y);
int isHazardPixel(int x, int y);
int isGoalPixel(int x, int y);


// Utility
void hideUnusedSpritesFrom(int startIndex);
int absInt(int x);

// ======================================================
//                    PUBLIC GAME API
// ======================================================

// Initializes the whole game system
void initGame(void) {
    // Start with score reset for a fresh run
    score = 0;

    // Start on the title screen
    state = STATE_START;

    // Reset frame counter
    frameCount = 0;

    // Door starts hidden until all enemies are defeated
    doorVisible = 0;
    doorX = 208;
    doorY = 112;

    // Make sure the menu is drawn on the first frame
    menuNeedsRedraw = 1;

    // Set up palettes, tiles, HUD tiles, and sprite tiles
    initGraphics();

    // Draw the start screen immediately
    drawStartScreen();
}

// Main per-frame update
void updateGame(void) {
    // Advance global frame count
    frameCount++;

    // Run logic based on current state
    switch (state) {
        case STATE_START:
            updateStart();
            break;
        case STATE_LEVEL1:
            updateLevel1();
            break;
        case STATE_LEVEL2:
            updateLevel2();
            break;
        case STATE_PAUSE:
            updatePause();
            break;
        case STATE_WIN:
            updateWin();
            break;
        case STATE_LOSE:
            updateLose();
            break;
    }
}

// Main per-frame draw
void drawGame(void) {
    // Draw based on the current state
    switch (state) {
        case STATE_START:
            if (menuNeedsRedraw) {
                drawStartScreen();
                menuNeedsRedraw = 0;
            }
            break;

        case STATE_LEVEL1:
            drawLevel1();
            break;

        case STATE_LEVEL2:
            drawLevel2();
            break;

        case STATE_PAUSE:
            if (menuNeedsRedraw) {
                drawPauseScreen();
                menuNeedsRedraw = 0;
            }
            break;

        case STATE_WIN:
            if (menuNeedsRedraw) {
                drawWinScreen();
                menuNeedsRedraw = 0;
            }
            break;

        case STATE_LOSE:
            if (menuNeedsRedraw) {
                drawLoseScreen();
                menuNeedsRedraw = 0;
            }
            break;
    }
}

// ======================================================
//                   STATE UPDATE HELPERS
// ======================================================

static void updateStart(void) {
    // Start the game on START
    if (BUTTON_PRESSED(BUTTON_START)) {
        initLevel1();
        state = STATE_LEVEL1;
    }
}

static void updatePause(void) {
    // Unpause back to whichever level is active
    if (BUTTON_PRESSED(BUTTON_START)) {
        if (level2BalloonsRemaining > 0 && player.x >= 0 && state == STATE_PAUSE) {
            // Resume using the existing background state
            state = (level2HOff > 0 || level2BalloonsRemaining < 10) ? STATE_LEVEL2 : STATE_LEVEL1;
        }
    }
}

static void updateWin(void) {
    // Restart from the title screen
    if (BUTTON_PRESSED(BUTTON_START)) {
        initGame();
    }
}

static void updateLose(void) {
    // Restart from the title screen
    if (BUTTON_PRESSED(BUTTON_START)) {
        initGame();
    }
}

// ======================================================
//                 PLAYER RESET / ANIMATION
// ======================================================

void resetPlayerForCurrentLevel(void) {
    // Common player fields
    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;
    player.oldX = PLAYER_START_X;
    player.oldY = PLAYER_START_Y;
    player.xVel = 0;
    player.yVel = 0;
    player.direction = DIR_RIGHT;
    player.currentFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
    player.grounded = 0;
    player.onLadder = 0;

    // Start positions depend on the current state
    if (state == STATE_LEVEL2) {
        player.x = 16;
        player.y = 70;
    } else {
        player.x = PLAYER_START_X;
        player.y = PLAYER_START_Y;
    }
}

void updatePlayerAnimation(void) {
    // Idle if not moving
    if (!player.isMoving) {
        player.currentFrame = 0;
        player.animCounter = 0;
        return;
    }

    // Toggle between two simple frames
    player.animCounter++;
    if (player.animCounter > 12) {
        player.animCounter = 0;
        player.currentFrame ^= 1;
    }
}

// ======================================================
//                     BULLET HELPERS
// ======================================================

void firePlayerBullet(void) {
    int i;

    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (!playerBullets[i].active) {
            playerBullets[i].active = 1;
            playerBullets[i].width = 8;
            playerBullets[i].height = 8;
            playerBullets[i].x = player.x + (player.direction == DIR_RIGHT ? player.width : -4);
            playerBullets[i].y = player.y + 4;
            playerBullets[i].oldX = playerBullets[i].x;
            playerBullets[i].oldY = playerBullets[i].y;
            playerBullets[i].xVel = (player.direction == DIR_RIGHT) ? 3 : -3;
            playerBullets[i].yVel = 0;
            break;
        }
    }
}

void updatePlayerBullets(void) {
    int i;
    int j;

    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (!playerBullets[i].active) {
            continue;
        }

        playerBullets[i].oldX = playerBullets[i].x;
        playerBullets[i].x += playerBullets[i].xVel;

        // Deactivate off-screen / out-of-level bullets
        if (playerBullets[i].x < -8 || playerBullets[i].x > LEVEL2_PIXEL_W) {
            playerBullets[i].active = 0;
            continue;
        }

        // Check against enemies in level 1
        for (j = 0; j < MAX_ENEMIES; j++) {
            if (enemies[j].active &&
                collision(playerBullets[i].x, playerBullets[i].y, playerBullets[i].width, playerBullets[i].height,
                          enemies[j].x, enemies[j].y, enemies[j].width, enemies[j].height)) {

                playerBullets[i].active = 0;
                enemies[j].active = 0;
                enemies[j].phase = ENEMY_DEAD;
                enemiesRemaining--;
                score += SCORE_ENEMY_KILL;
                break;
            }
        }
    }
}

static void spawnEnemyBullet(Enemy* e) {
    int i;

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!enemyBullets[i].active) {
            enemyBullets[i].active = 1;
            enemyBullets[i].width = 8;
            enemyBullets[i].height = 8;
            enemyBullets[i].x = e->x + (e->direction == DIR_RIGHT ? e->width : -4);
            enemyBullets[i].y = e->y + 4;
            enemyBullets[i].oldX = enemyBullets[i].x;
            enemyBullets[i].oldY = enemyBullets[i].y;
            enemyBullets[i].xVel = (e->direction == DIR_RIGHT) ? 2 : -2;
            enemyBullets[i].yVel = 0;
            break;
        }
    }
}

void updateEnemyBullets(void) {
    int i;

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!enemyBullets[i].active) {
            continue;
        }

        enemyBullets[i].oldX = enemyBullets[i].x;
        enemyBullets[i].x += enemyBullets[i].xVel;

        // Remove bullets that leave the level area
        if (enemyBullets[i].x < -8 || enemyBullets[i].x > LEVEL1_PIXEL_W + 8) {
            enemyBullets[i].active = 0;
            continue;
        }

        // Damage the player on contact
        if (collision(enemyBullets[i].x, enemyBullets[i].y, enemyBullets[i].width, enemyBullets[i].height,
                      player.x, player.y, player.width, player.height)) {

            enemyBullets[i].active = 0;
            damagePlayer();
        }
    }
}

// ======================================================
//                      ENEMY HELPERS
// ======================================================

void updateEnemies(void) {
    int i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            continue;
        }

        enemies[i].oldX = enemies[i].x;
        enemies[i].oldY = enemies[i].y;

        // Horizontal patrolling
        enemies[i].x += (enemies[i].direction == DIR_RIGHT) ? enemies[i].xVel : -enemies[i].xVel;

        if (enemies[i].x <= enemies[i].minX) {
            enemies[i].x = enemies[i].minX;
            enemies[i].direction = DIR_RIGHT;
        }

        if (enemies[i].x >= enemies[i].maxX) {
            enemies[i].x = enemies[i].maxX;
            enemies[i].direction = DIR_LEFT;
        }

        // Animation
        enemies[i].animCounter++;
        if (enemies[i].animCounter > 16) {
            enemies[i].animCounter = 0;
            enemies[i].currentFrame ^= 1;
        }

        // Timed shooting
        enemies[i].shootTimer--;
        if (enemies[i].shootTimer <= 0) {
            spawnEnemyBullet(&enemies[i]);
            enemies[i].shootTimer = 75;
        }

        // Direct collision with the player also hurts
        if (collision(enemies[i].x, enemies[i].y, enemies[i].width, enemies[i].height,
                      player.x, player.y, player.width, player.height)) {
            damagePlayer();
        }
    }
}

// ======================================================
//                     DAMAGE / RESPAWN
// ======================================================

void damagePlayer(void) {
    // Ignore damage while invincible
    if (player.invincibleTimer > 0) {
        return;
    }

    // Lose one life and grant invincibility frames
    player.lives--;
    player.invincibleTimer = 60;

    // Respawn position depends on the current level
    if (state == STATE_LEVEL1) {
        int spawnGroundY;

        player.x = 8;
        spawnGroundY = findGroundYBelow(player.x, 0, player.width, player.height);
        player.y = (spawnGroundY >= 0) ? spawnGroundY : PLAYER_START_Y;
        player.yVel = 0;
    } else if (state == STATE_LEVEL2) {
        player.x = CLAMP(player.x - 24, 0, LEVEL2_PIXEL_W - player.width);
        player.y = 70;
    }
}

// ======================================================
//                    COLLISION HELPERS
// ======================================================

u8 getCollisionPixel(int x, int y) {
    // Out of bounds is treated as solid
    if (x < 0 || x >= collisionMapOneBitmapLen / 2 || y < 0 || y >= LEVEL1_PIXEL_H) {
        return CM_BLOCKED;
    }

    // The collision map is a 240x160 8bpp bitmap exported by grit/usenti.
    // It is indexed as a flat array of color indices.
    return collisionMapOneBitmap[y * 240 + x];
}

int isSolidPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_BLOCKED;
}

int isLadderPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_LADDER;
}

int isHazardPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_KILL;
}

int isGoalPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_GOAL;
}

int rectTouchesColor(int x, int y, int width, int height, u8 color) {
    int px;
    int py;

    for (py = y; py < y + height; py++) {
        for (px = x; px < x + width; px++) {
            if (getCollisionPixel(px, py) == color) {
                return 1;
            }
        }
    }

    return 0;
}

int canMoveTo(int x, int y, int width, int height) {
    int px;
    int py;

    // Reject if outside the playable map
    if (x < 0 || y < 0 || x + width > LEVEL1_PIXEL_W || y + height > LEVEL1_PIXEL_H) {
        return 0;
    }

    // Allow ladders and open/goal/hazard pixels.
    // Only blocked pixels stop movement.
    for (py = y; py < y + height; py++) {
        for (px = x; px < x + width; px++) {
            if (isSolidPixel(px, py)) {
                return 0;
            }
        }
    }

    return 1;
}

int findGroundYBelow(int x, int y, int width, int height) {
    int testY;

    // Scan downward until the first blocked pixel row below the player
    for (testY = y; testY < LEVEL1_PIXEL_H - height; testY++) {
        if (!canMoveTo(x, testY, width, height)) {
            return testY - 1;
        }
    }

    return -1;
}

int findLowestGroundYAtX(int x, int width, int height) {
    return findGroundYBelow(x, 0, width, height);
}

int absInt(int x) {
    return (x < 0) ? -x : x;
}

// ======================================================
//                     HUD DRAWING
// ======================================================

void clearHUD(void) {
    int i;
    for (i = 0; i < 32 * 32; i++) {
        SCREENBLOCK[HUD_SCREENBLOCK].tilemap[i] = TILE_BLANK;
    }
}

static int glyphIndexForChar(char c) {
    if (c >= 'A' && c <= 'Z') return FONT_BASE_TILE + (c - 'A');
    if (c >= '0' && c <= '9') return FONT_BASE_TILE + 26 + (c - '0');

    switch (c) {
        case ':': return FONT_BASE_TILE + 36;
        case '-': return FONT_BASE_TILE + 37;
        case '!': return FONT_BASE_TILE + 38;
        case ' ': return TILE_BLANK;
        default:  return TILE_BLANK;
    }
}

static void putCharHUD(int col, int row, char c) {
    SCREENBLOCK[HUD_SCREENBLOCK].tilemap[row * 32 + col] = glyphIndexForChar(c);
}

static void putStringHUD(int col, int row, const char* str) {
    while (*str) {
        putCharHUD(col++, row, *str++);
    }
}

static void putNumberHUD(int col, int row, int value, int digits) {
    int i;
    char buffer[10];

    for (i = digits - 1; i >= 0; i--) {
        buffer[i] = '0' + (value % 10);
        value /= 10;
    }

    buffer[digits] = '\0';
    putStringHUD(col, row, buffer);
}

void drawHUD(void) {
    clearHUD();

    putStringHUD(1, 1, "SCORE");
    putNumberHUD(7, 1, score, 4);

    putStringHUD(14, 1, "LIVES");
    putNumberHUD(20, 1, player.lives, 1);

    if (state == STATE_LEVEL2) {
        putStringHUD(1, 3, "BALS");
        putNumberHUD(6, 3, level2BalloonsRemaining, 2);
    } else if (state == STATE_LEVEL1) {
        putStringHUD(1, 3, "ENEM");
        putNumberHUD(6, 3, enemiesRemaining, 1);
    }
}

// ======================================================
//                     MENU DRAWING
// ======================================================

static void drawStartScreen(void) {
    fillScreen4(0);

    drawRect4(0, 0, SCREENWIDTH, SCREENHEIGHT, 12);
    drawRect4(0, 0, SCREENWIDTH, 24, 14);
    drawRect4(0, 136, SCREENWIDTH, 24, 10);

    drawRect4(16, 34, 32, 24, 13);
    drawRect4(58, 22, 24, 18, 15);
    drawRect4(92, 40, 28, 20, 13);
    drawRect4(140, 30, 40, 28, 15);
    drawRect4(196, 44, 24, 18, 13);

    drawString4(58, 12, "BALLOON GAME", 0);
    drawString4(40, 100, "PRESS START", 0);
    drawString4(20, 120, "B TO SHOOT", 0);
    drawString4(20, 132, "A TO FLOAT", 0);

    waitForVBlank();
    flipPage();
}

static void drawPauseScreen(void) {
    fillScreen4(0);
    drawRect4(0, 0, SCREENWIDTH, SCREENHEIGHT, 7);
    drawString4(86, 60, "PAUSED", 0);
    drawString4(38, 90, "PRESS START TO RESUME", 0);

    waitForVBlank();
    flipPage();
}

static void drawWinScreen(void) {
    fillScreen4(0);
    drawRect4(0, 0, SCREENWIDTH, SCREENHEIGHT, 11);
    drawString4(90, 56, "YOU WIN!", 0);
    drawString4(54, 78, "FINAL SCORE", 0);
    drawString4(118, 94, "0000", 0);
    drawString4(32, 124, "PRESS START TO RESTART", 0);

    waitForVBlank();
    flipPage();
}

static void drawLoseScreen(void) {
    fillScreen4(0);
    drawRect4(0, 0, SCREENWIDTH, SCREENHEIGHT, 3);
    drawString4(86, 56, "YOU LOSE", 0);
    drawString4(50, 84, "TRY AGAIN", 0);
    drawString4(32, 124, "PRESS START TO RESTART", 0);

    waitForVBlank();
    flipPage();
}

// ======================================================
//                  SPRITE DRAWING HELPERS
// ======================================================

void drawPlayerSprite(int screenX, int screenY) {
    int attr2 = ATTR2_TILEID((player.currentFrame ? OBJ_TILE_PLAYER1 : OBJ_TILE_PLAYER0), 0);

    shadowOAM[0].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[0].attr1 = ATTR1_X(screenX) | ATTR1_SMALL |
                         (player.direction == DIR_LEFT ? ATTR1_HFLIP : 0);
    shadowOAM[0].attr2 = attr2;
}

void drawEnemySprite(int oamIndex, int screenX, int screenY) {
    int tileBase;

    if (enemies[oamIndex - 1].phase == ENEMY_WALKING) {
        tileBase = enemies[oamIndex - 1].currentFrame ? OBJ_TILE_ENEMYW1 : OBJ_TILE_ENEMYW0;
    } else {
        tileBase = enemies[oamIndex - 1].currentFrame ? OBJ_TILE_ENEMYF1 : OBJ_TILE_ENEMYF0;
    }

    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_SMALL |
                                (enemies[oamIndex - 1].direction == DIR_LEFT ? ATTR1_HFLIP : 0);
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(tileBase, 0);
}

void drawBulletSprite(int oamIndex, int screenX, int screenY, int enemyBullet) {
    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_TINY;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(enemyBullet ? OBJ_TILE_EBULLET : OBJ_TILE_BULLET, 0);
}

void drawBalloonSprite(int oamIndex, int screenX, int screenY) {
    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_TALL | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_TINY;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(OBJ_TILE_BALLOON, 0);
}

void drawStarSprite(int oamIndex, int screenX, int screenY) {
    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_SMALL;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(OBJ_TILE_STAR, 0);
}

void drawDoorSprite(int screenX, int screenY) {
    shadowOAM[120].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[120].attr1 = ATTR1_X(screenX) | ATTR1_SMALL;
    shadowOAM[120].attr2 = ATTR2_TILEID(OBJ_TILE_DOOR, 0);
}

void hideUnusedSpritesFrom(int startIndex) {
    int i;
    for (i = startIndex; i < 128; i++) {
        if (i == 120 && doorVisible) {
            continue;
        }
        shadowOAM[i].attr0 = ATTR0_HIDE;
    }
}

// ======================================================
//                  GRAPHICS / TILE BUILDING
// ======================================================

static void initGraphics(void) {
    // Initialize mode 4 first for menu rendering
    REG_DISPCNT = MODE4 | BG2_ENABLE | DISP_BACKBUFFER;

    // Build palettes and tile art used once the game enters Mode 0
    initPalettes();

    // Build BG tiles for the HUD and level backgrounds
    buildHudTiles();
    buildBackgroundTiles();

    // Build OBJ tiles for player, enemies, bullets, balloons, stars, and door
    buildSpriteTiles();

    // Hide every sprite initially
    hideSprites();
}

static void initPalettes(void) {
    // BG palette
    BG_PALETTE[0] = RGB(0, 0, 0);
    BG_PALETTE[1] = RGB(31, 31, 31);
    BG_PALETTE[2] = RGB(12, 20, 31);
    BG_PALETTE[3] = RGB(18, 26, 31);
    BG_PALETTE[4] = RGB(28, 28, 28);
    BG_PALETTE[5] = RGB(31, 10, 10);
    BG_PALETTE[6] = RGB(10, 31, 10);
    BG_PALETTE[7] = RGB(31, 28, 10);
    BG_PALETTE[8] = RGB(18, 10, 31);
    BG_PALETTE[9] = RGB(0, 0, 0);
    BG_PALETTE[10] = RGB(24, 12, 4);
    BG_PALETTE[11] = RGB(16, 16, 16);
    BG_PALETTE[12] = RGB(31, 31, 0);
    BG_PALETTE[13] = RGB(31, 15, 20);
    BG_PALETTE[14] = RGB(14, 24, 31);
    BG_PALETTE[15] = RGB(31, 31, 31);

    // Sprite palette
    SPRITEPALETTE[0] = RGB(0, 0, 0);
    SPRITEPALETTE[1] = RGB(31, 31, 31);
    SPRITEPALETTE[2] = RGB(31, 24, 8);
    SPRITEPALETTE[3] = RGB(31, 10, 16);
    SPRITEPALETTE[4] = RGB(8, 16, 31);
    SPRITEPALETTE[5] = RGB(10, 28, 10);
    SPRITEPALETTE[6] = RGB(31, 31, 0);
    SPRITEPALETTE[7] = RGB(20, 10, 31);
    SPRITEPALETTE[8] = RGB(31, 18, 0);
    SPRITEPALETTE[9] = RGB(20, 20, 20);
    SPRITEPALETTE[10] = RGB(31, 0, 0);
    SPRITEPALETTE[11] = RGB(0, 31, 0);
    SPRITEPALETTE[12] = RGB(0, 0, 31);
    SPRITEPALETTE[13] = RGB(31, 31, 31);
    SPRITEPALETTE[14] = RGB(31, 20, 20);
    SPRITEPALETTE[15] = RGB(28, 28, 10);
}

static void buildHudTiles(void) {
    // Clear HUD charblock
    clearCharBlock(HUD_CHARBLOCK);

    // Blank tile for empty HUD cells
    makeSolidTile(BG_TILE_MEM + (HUD_CHARBLOCK * 1024), TILE_BLANK, 0);

    // Glyph patterns for A-Z, 0-9, and a few punctuation marks
    {
        static const u8 glyphA[8] = {0x18,0x24,0x24,0x3C,0x24,0x24,0x24,0x00};
        static const u8 glyphB[8] = {0x38,0x24,0x24,0x38,0x24,0x24,0x38,0x00};
        static const u8 glyphC[8] = {0x18,0x24,0x20,0x20,0x20,0x24,0x18,0x00};
        static const u8 glyphD[8] = {0x38,0x24,0x24,0x24,0x24,0x24,0x38,0x00};
        static const u8 glyphE[8] = {0x3C,0x20,0x20,0x38,0x20,0x20,0x3C,0x00};
        static const u8 glyphF[8] = {0x3C,0x20,0x20,0x38,0x20,0x20,0x20,0x00};
        static const u8 glyphG[8] = {0x18,0x24,0x20,0x2C,0x24,0x24,0x18,0x00};
        static const u8 glyphH[8] = {0x24,0x24,0x24,0x3C,0x24,0x24,0x24,0x00};
        static const u8 glyphI[8] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00};
        static const u8 glyphJ[8] = {0x1C,0x08,0x08,0x08,0x08,0x28,0x10,0x00};
        static const u8 glyphK[8] = {0x24,0x28,0x30,0x20,0x30,0x28,0x24,0x00};
        static const u8 glyphL[8] = {0x20,0x20,0x20,0x20,0x20,0x20,0x3C,0x00};
        static const u8 glyphM[8] = {0x22,0x36,0x2A,0x2A,0x22,0x22,0x22,0x00};
        static const u8 glyphN[8] = {0x24,0x34,0x34,0x2C,0x2C,0x24,0x24,0x00};
        static const u8 glyphO[8] = {0x18,0x24,0x24,0x24,0x24,0x24,0x18,0x00};
        static const u8 glyphP[8] = {0x38,0x24,0x24,0x38,0x20,0x20,0x20,0x00};
        static const u8 glyphQ[8] = {0x18,0x24,0x24,0x24,0x2C,0x28,0x14,0x00};
        static const u8 glyphR[8] = {0x38,0x24,0x24,0x38,0x30,0x28,0x24,0x00};
        static const u8 glyphS[8] = {0x1C,0x20,0x20,0x18,0x04,0x04,0x38,0x00};
        static const u8 glyphT[8] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x00};
        static const u8 glyphU[8] = {0x24,0x24,0x24,0x24,0x24,0x24,0x18,0x00};
        static const u8 glyphV[8] = {0x24,0x24,0x24,0x24,0x24,0x18,0x18,0x00};
        static const u8 glyphW[8] = {0x22,0x22,0x22,0x2A,0x2A,0x36,0x22,0x00};
        static const u8 glyphX[8] = {0x24,0x24,0x18,0x18,0x18,0x24,0x24,0x00};
        static const u8 glyphY[8] = {0x24,0x24,0x24,0x18,0x18,0x18,0x18,0x00};
        static const u8 glyphZ[8] = {0x3C,0x04,0x08,0x18,0x10,0x20,0x3C,0x00};

        static const u8 glyph0[8] = {0x18,0x24,0x2C,0x34,0x24,0x24,0x18,0x00};
        static const u8 glyph1[8] = {0x08,0x18,0x08,0x08,0x08,0x08,0x1C,0x00};
        static const u8 glyph2[8] = {0x18,0x24,0x04,0x08,0x10,0x20,0x3C,0x00};
        static const u8 glyph3[8] = {0x38,0x04,0x04,0x18,0x04,0x04,0x38,0x00};
        static const u8 glyph4[8] = {0x08,0x18,0x28,0x28,0x3C,0x08,0x08,0x00};
        static const u8 glyph5[8] = {0x3C,0x20,0x20,0x38,0x04,0x04,0x38,0x00};
        static const u8 glyph6[8] = {0x18,0x20,0x20,0x38,0x24,0x24,0x18,0x00};
        static const u8 glyph7[8] = {0x3C,0x04,0x08,0x10,0x10,0x10,0x10,0x00};
        static const u8 glyph8[8] = {0x18,0x24,0x24,0x18,0x24,0x24,0x18,0x00};
        static const u8 glyph9[8] = {0x18,0x24,0x24,0x1C,0x04,0x04,0x18,0x00};

        static const u8 glyphColon[8] = {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00};
        static const u8 glyphDash[8]  = {0x00,0x00,0x00,0x3C,0x00,0x00,0x00,0x00};
        static const u8 glyphBang[8]  = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00};

        const u8* glyphs[39] = {
            glyphA,glyphB,glyphC,glyphD,glyphE,glyphF,glyphG,glyphH,glyphI,glyphJ,glyphK,glyphL,glyphM,
            glyphN,glyphO,glyphP,glyphQ,glyphR,glyphS,glyphT,glyphU,glyphV,glyphW,glyphX,glyphY,glyphZ,
            glyph0,glyph1,glyph2,glyph3,glyph4,glyph5,glyph6,glyph7,glyph8,glyph9,
            glyphColon,glyphDash,glyphBang
        };

        int i;
        for (i = 0; i < 39; i++) {
            makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 1024), FONT_BASE_TILE + i, glyphs[i], 1, 0);
        }
    }
}

static void buildBackgroundTiles(void) {
    u32* base = BG_TILE_MEM + (LEVEL_CHARBLOCK * 1024);

    clearCharBlock(LEVEL_CHARBLOCK);

    makeSolidTile(base, TILE_BLANK, 0);
    makeSolidTile(base, TILE_SOLID, 10);
    makeCheckerTile(base, TILE_PLATFORM, 10, 11);
    makeSolidTile(base, TILE_SKY, 14);
    makeCheckerTile(base, TILE_CLOUD, 15, 14);
    makeCheckerTile(base, TILE_STARBG, 8, 14);
    makeCheckerTile(base, TILE_DOOR, 6, 10);
    makeCheckerTile(base, TILE_BALLOONBG, 13, 14);
}

static void clearObjTiles(void) {
    int i;
    for (i = 0; i < 1024 * 16; i++) {
        ((u16*)OBJ_TILE_MEM)[i] = 0;
    }
}

static void buildSpriteTiles(void) {
    u32* base = OBJ_TILE_MEM;
    int i;
    u8 pixels[64];

    clearObjTiles();

    // --------------------------------------------------
    // Player frame 0 (16x16 = 4 tiles)
    // --------------------------------------------------
    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        for (y = 2; y < 14; y++) {
            for (x = 4; x < 12; x++) {
                pixels[y * 16 + x] = 2;
            }
        }
        for (y = 0; y < 4; y++) {
            for (x = 5; x < 11; x++) {
                pixels[y * 16 + x] = 6;
            }
        }
        pixels[6 * 16 + 5] = 1;
        pixels[6 * 16 + 10] = 1;
    }

    write4bppTile(base, OBJ_TILE_PLAYER0 + 0, pixels);
    write4bppTile(base, OBJ_TILE_PLAYER0 + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_PLAYER0 + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_PLAYER0 + 3, pixels + 136);

    // --------------------------------------------------
    // Player frame 1
    // --------------------------------------------------
    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        for (y = 2; y < 14; y++) {
            for (x = 4; x < 12; x++) {
                pixels[y * 16 + x] = 2;
            }
        }
        for (y = 0; y < 4; y++) {
            for (x = 5; x < 11; x++) {
                pixels[y * 16 + x] = 6;
            }
        }
        pixels[12 * 16 + 4] = 2;
        pixels[13 * 16 + 4] = 2;
        pixels[12 * 16 + 11] = 2;
        pixels[13 * 16 + 11] = 2;
    }

    write4bppTile(base, OBJ_TILE_PLAYER1 + 0, pixels);
    write4bppTile(base, OBJ_TILE_PLAYER1 + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_PLAYER1 + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_PLAYER1 + 3, pixels + 136);

    // --------------------------------------------------
    // Floating enemy frames
    // --------------------------------------------------
    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        for (y = 3; y < 13; y++) {
            for (x = 3; x < 13; x++) {
                pixels[y * 16 + x] = 3;
            }
        }
    }

    write4bppTile(base, OBJ_TILE_ENEMYF0 + 0, pixels);
    write4bppTile(base, OBJ_TILE_ENEMYF0 + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_ENEMYF0 + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_ENEMYF0 + 3, pixels + 136);

    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        for (y = 2; y < 12; y++) {
            for (x = 3; x < 13; x++) {
                pixels[y * 16 + x] = 3;
            }
        }
    }

    write4bppTile(base, OBJ_TILE_ENEMYF1 + 0, pixels);
    write4bppTile(base, OBJ_TILE_ENEMYF1 + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_ENEMYF1 + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_ENEMYF1 + 3, pixels + 136);

    // --------------------------------------------------
    // Walking enemy frames
    // --------------------------------------------------
    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        for (y = 4; y < 14; y++) {
            for (x = 3; x < 13; x++) {
                pixels[y * 16 + x] = 5;
            }
        }
    }

    write4bppTile(base, OBJ_TILE_ENEMYW0 + 0, pixels);
    write4bppTile(base, OBJ_TILE_ENEMYW0 + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_ENEMYW0 + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_ENEMYW0 + 3, pixels + 136);

    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        for (y = 4; y < 14; y++) {
            for (x = 3; x < 13; x++) {
                pixels[y * 16 + x] = 5;
            }
        }
        pixels[13 * 16 + 4] = 5;
        pixels[13 * 16 + 10] = 5;
    }

    write4bppTile(base, OBJ_TILE_ENEMYW1 + 0, pixels);
    write4bppTile(base, OBJ_TILE_ENEMYW1 + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_ENEMYW1 + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_ENEMYW1 + 3, pixels + 136);

    // --------------------------------------------------
    // Bullet tiles (8x8)
    // --------------------------------------------------
    {
        u8 bullet[64] = {
            0,0,6,6,6,6,0,0,
            0,6,6,6,6,6,6,0,
            6,6,6,6,6,6,6,6,
            6,6,6,6,6,6,6,6,
            6,6,6,6,6,6,6,6,
            6,6,6,6,6,6,6,6,
            0,6,6,6,6,6,6,0,
            0,0,6,6,6,6,0,0
        };

        u8 ebullet[64] = {
            0,0,10,10,10,10,0,0,
            0,10,10,10,10,10,10,0,
            10,10,10,10,10,10,10,10,
            10,10,10,10,10,10,10,10,
            10,10,10,10,10,10,10,10,
            10,10,10,10,10,10,10,10,
            0,10,10,10,10,10,10,0,
            0,0,10,10,10,10,0,0
        };

        write4bppTile(base, OBJ_TILE_BULLET, bullet);
        write4bppTile(base, OBJ_TILE_EBULLET, ebullet);
    }

    // --------------------------------------------------
    // Balloon (8x16 = 2 tiles)
    // --------------------------------------------------
    {
        u8 balloon[128] = {0};
        int x, y;
        for (y = 0; y < 10; y++) {
            for (x = 1; x < 7; x++) {
                balloon[y * 8 + x] = 3;
            }
        }
        balloon[10 * 8 + 3] = 3;
        balloon[11 * 8 + 3] = 3;
        balloon[12 * 8 + 3] = 1;

        write4bppTile(base, OBJ_TILE_BALLOON + 0, balloon);
        write4bppTile(base, OBJ_TILE_BALLOON + 1, balloon + 64);
    }

    // --------------------------------------------------
    // Star (16x16 = 4 tiles)
    // --------------------------------------------------
    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        pixels[1 * 16 + 7] = 6;
        pixels[2 * 16 + 7] = 6;
        pixels[3 * 16 + 7] = 6;
        for (x = 4; x < 11; x++) pixels[4 * 16 + x] = 6;
        for (x = 3; x < 12; x++) pixels[5 * 16 + x] = 6;
        for (x = 2; x < 13; x++) pixels[6 * 16 + x] = 6;
        for (x = 1; x < 14; x++) pixels[7 * 16 + x] = 6;
        for (x = 2; x < 13; x++) pixels[8 * 16 + x] = 6;
        for (x = 3; x < 12; x++) pixels[9 * 16 + x] = 6;
        for (x = 4; x < 11; x++) pixels[10 * 16 + x] = 6;
        pixels[11 * 16 + 7] = 6;
        pixels[12 * 16 + 7] = 6;
        pixels[13 * 16 + 7] = 6;
        for (y = 4; y < 11; y++) pixels[y * 16 + 7] = 6;
    }

    write4bppTile(base, OBJ_TILE_STAR + 0, pixels);
    write4bppTile(base, OBJ_TILE_STAR + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_STAR + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_STAR + 3, pixels + 136);

    // --------------------------------------------------
    // Door (16x16 = 4 tiles)
    // --------------------------------------------------
    for (i = 0; i < 64; i++) pixels[i] = 0;
    {
        int x, y;
        for (y = 0; y < 16; y++) {
            for (x = 3; x < 13; x++) {
                pixels[y * 16 + x] = 10;
            }
        }
        for (y = 2; y < 14; y++) {
            for (x = 5; x < 11; x++) {
                pixels[y * 16 + x] = 6;
            }
        }
    }

    write4bppTile(base, OBJ_TILE_DOOR + 0, pixels);
    write4bppTile(base, OBJ_TILE_DOOR + 1, pixels + 8);
    write4bppTile(base, OBJ_TILE_DOOR + 2, pixels + 128);
    write4bppTile(base, OBJ_TILE_DOOR + 3, pixels + 136);
}

// ======================================================
//                      TILE HELPERS
// ======================================================

static void write4bppTile(u32* base, int tileIndex, const u8 pixels[64]) {
    int i;
    u32* tile = base + tileIndex * 8;

    for (i = 0; i < 8; i++) {
        u32 row = 0;
        row |= (pixels[i * 8 + 0] & 0xF) << 0;
        row |= (pixels[i * 8 + 1] & 0xF) << 4;
        row |= (pixels[i * 8 + 2] & 0xF) << 8;
        row |= (pixels[i * 8 + 3] & 0xF) << 12;
        row |= (pixels[i * 8 + 4] & 0xF) << 16;
        row |= (pixels[i * 8 + 5] & 0xF) << 20;
        row |= (pixels[i * 8 + 6] & 0xF) << 24;
        row |= (pixels[i * 8 + 7] & 0xF) << 28;
        tile[i] = row;
    }
}

static void makeSolidTile(u32* base, int tileIndex, u8 color) {
    u8 pixels[64];
    int i;

    for (i = 0; i < 64; i++) {
        pixels[i] = color;
    }

    write4bppTile(base, tileIndex, pixels);
}

static void makeCheckerTile(u32* base, int tileIndex, u8 c1, u8 c2) {
    u8 pixels[64];
    int x, y;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            pixels[y * 8 + x] = ((x + y) & 1) ? c1 : c2;
        }
    }

    write4bppTile(base, tileIndex, pixels);
}

static void makeGlyphTile(u32* base, int tileIndex, const u8 rows[8], u8 fg, u8 bg) {
    u8 pixels[64];
    int x, y;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            pixels[y * 8 + x] = (rows[y] & (1 << (7 - x))) ? fg : bg;
        }
    }

    write4bppTile(base, tileIndex, pixels);
}