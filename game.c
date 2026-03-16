#include "gba.h"
#include "mode0.h"
#include "sprites.h"
#include "print.h"
#include "game.h"
#include "levelone_logic.h"
#include "leveltwo_logic.h"

#include "tileset.h"
#include "levelone.h"
#include "leveltwo.h"
#include "collisionMapOne.h"
#include "collisionMapTwo.h"
#include "spriteSheet.h"

#include "screen.h"


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

// ======================================================
//                     GLOBAL GAME DATA
// ======================================================

// Current game state
GameState state;
GameState pausedState;

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
int level2VOff;
int menuNeedsRedraw;

// Door state for finishing level 1
int doorVisible;
int doorOpen;
int doorX;
int doorY;

// Frame counter
int frameCount;

// Small pseudo-random state used for balloon color selection
static unsigned int spriteRngState = 0xA341316Cu;

// Defer level VRAM/tilemap initialization until draw time.
// This prevents intro-screen glitches when switching states.
static int pendingLevel1Load = 0;
static int pendingLevel2Load = 0;

static void clearObjTileIndex(int tileIndex);
// ======================================================
//                   FORWARD DECLARATIONS
// ======================================================

// Asset / VRAM setup
static void initGraphics(void);
static void initPalettes(void);
static void buildHudTiles(void);
static void buildSpriteTiles(void);
static void clearObjTiles(void);

// Tile helpers
static void write4bppTile(u32* base, int tileIndex, const u8 pixels[64]);
static void makeSolidTile(u32* base, int tileIndex, u8 color);
static void makeGlyphTile(u32* base, int tileIndex, const u8 rows[8], u8 fg, u8 bg);
static void buildSimpleStarTile(int tileIndex);

// BG helpers
void clearHUD(void);
void drawHUD(void);
static void putCharHUD(int col, int row, char c);
static void putStringHUD(int col, int row, const char* str);
static void putNumberHUD(int col, int row, int value, int digits);
static int glyphIndexForChar(char c);
static void prepareMenuLayers(void);
static void loadMenuBackground(void);
static void drawInfoScreen(const char* title, const char* subtitle);
static void restorePausedGameplayLayers(void);

// Menu screens
static void drawStartScreen(void);
static void drawLevel1IntroScreen(void);
static void drawLevel2IntroScreen(void);
static void drawPauseScreen(void);
static void drawWinScreen(void);
static void drawLoseScreen(void);

// Level setup
void resetPlayerForCurrentLevel(void);

// Update handlers
static void updateStart(void);
static void updateLevel1Intro(void);
static void updatePause(void);
static void updateLevel2Intro(void);
static void updateWin(void);
static void updateLose(void);
static void clearMenuBackground(unsigned short entry);

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
int isHazardPixel(int x, int y);
int isGoalPixel(int x, int y);
int rectTouchesColor(int x, int y, int width, int height, u8 color);
int canMoveTo(int x, int y, int width, int height);
int findGroundYBelow(int x, int y, int width, int height);
int findLowestGroundYAtX(int x, int width, int height);
void damagePlayer(void);

// Sprite drawing
void drawPlayerSprite(int screenX, int screenY);
void drawEnemySprite(int enemyIndex, int oamIndex, int screenX, int screenY);
void drawBulletSprite(int oamIndex, int screenX, int screenY, int enemyBullet);
void drawBalloonSprite(int oamIndex, int screenX, int screenY, int variant);
void drawStarSprite(int oamIndex, int screenX, int screenY);
void drawDoorSprite(int screenX, int screenY);

// Utility
void hideUnusedSpritesFrom(int startIndex);
int absInt(int x);
int getFloatVelocityForLives(int lives);
int nextRandomSpriteVariant(void);

// ======================================================
//                    PUBLIC GAME API
// ======================================================

// Initializes the whole game system
void initGame(void) {
    score = 0;
    state = STATE_START;
    pausedState = STATE_LEVEL1;
    frameCount = 0;

    doorVisible = 0;
    doorOpen = 0;
    doorX = 208;
    doorY = 112;

    menuNeedsRedraw = 1;

    initGraphics();
    drawStartScreen();
}

void updateGame(void) {
    frameCount++;

    switch (state) {
        case STATE_START:
            updateStart();
            break;
        case STATE_LEVEL1_INTRO:
            updateLevel1Intro();
            break;
        case STATE_LEVEL1:
            updateLevel1();
            break;
        case STATE_LEVEL2_INTRO:
            updateLevel2Intro();
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

void drawGame(void) {
    switch (state) {
        case STATE_START:
            if (menuNeedsRedraw) {
                drawStartScreen();
                menuNeedsRedraw = 0;
            }
            break;

        case STATE_LEVEL1_INTRO:
            if (menuNeedsRedraw) {
                drawLevel1IntroScreen();
                menuNeedsRedraw = 0;
            }
            break;

        case STATE_LEVEL1:
            if (pendingLevel1Load) {
                initLevel1();
                pendingLevel1Load = 0;
            }
            drawLevel1();
            break;

        case STATE_LEVEL2_INTRO:
            if (menuNeedsRedraw) {
                drawLevel2IntroScreen();
                menuNeedsRedraw = 0;
            }
            break;

        case STATE_LEVEL2:
            if (pendingLevel2Load) {
                initLevel2();
                pendingLevel2Load = 0;
            }
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
    if (BUTTON_PRESSED(BUTTON_START)) {
        pendingLevel1Load = 1;
        state = STATE_LEVEL1;
    }
}

static void updateLevel1Intro(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Do NOT initialize the level here.
        // updateGame() runs before waitForVBlank(), so touching VRAM here can glitch.
        pendingLevel1Load = 1;
        state = STATE_LEVEL1;
    }
}

static void updateLevel2Intro(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Defer the actual VRAM/tilemap setup until drawGame(), after VBlank.
        pendingLevel2Load = 1;
        menuNeedsRedraw = 0;
        state = STATE_LEVEL2;
    }
}

static void updatePause(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        // The pause screen reuses BG memory for the menu image.
        // So before resuming gameplay, rebuild the gameplay layers for
        // whichever level we paused from.
        restorePausedGameplayLayers();

        // Resume the exact level we were previously in.
        state = pausedState;
    }
}

static void updateWin(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        initGame();
    }
}

static void updateLose(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        initGame();
    }
}

// ======================================================
//                 PLAYER RESET / ANIMATION
// ======================================================

void resetPlayerForCurrentLevel(void) {
    int spawnGroundY;

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
    player.floatBoostTimer = 0;

    if (state == STATE_LEVEL2 || state == STATE_LEVEL2_INTRO) {
        // Spawn on top of the left-most platform in level 2.
        // Using collision keeps the spawn correct even if the art changes later.
        player.x = 24;
        spawnGroundY = findGroundYBelow(player.x, 0, player.width, player.height);
        player.y = (spawnGroundY >= 0) ? spawnGroundY : 28;
    } else {
        // Level 1 keeps its collision-based spawn.
        player.x = PLAYER_START_X;
        spawnGroundY = findGroundYBelow(player.x, 0, player.width, player.height);
        player.y = (spawnGroundY >= 0) ? spawnGroundY : PLAYER_START_Y;
    }
}

void updatePlayerAnimation(void) {
    // Snap to frame 0 when idle so the sprite rests in a clean pose.
    if (!player.isMoving) {
        player.currentFrame = 0;
        player.animCounter  = 0;
        return;
    }

    // Advance the counter each frame the player is moving.
    // At 60fps a threshold of 8 gives roughly 7-8 frame changes per second,
    // producing a smooth 4-frame walk/fly cycle.
    player.animCounter++;
    if (player.animCounter >= 8) {
        player.animCounter  = 0;
        // Cycle 0 -> 1 -> 2 -> 3 -> 0 through all four animation frames.
        player.currentFrame = (player.currentFrame + 1) % 4;
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

        // Check bullet collision with enemies
        for (j = 0; j < MAX_ENEMIES; j++) {
            if (!enemies[j].active) continue;

            if (collision(playerBullets[i].x, playerBullets[i].y,
                        playerBullets[i].width, playerBullets[i].height,
                        enemies[j].x, enemies[j].y,
                        enemies[j].width, enemies[j].height)) {

                playerBullets[i].active = 0;

                // -------------------------------
                // PHASE 1 -> PHASE 2 TRANSITION
                // -------------------------------
                if (enemies[j].phase == ENEMY_FLOATING) {

                    // Per spec: if no ground below, enemy dies immediately
                    if (findGroundYBelow(enemies[j].x, enemies[j].y,
                                        enemies[j].width, enemies[j].height) < 0) {
                        enemies[j].active = 0;
                        enemies[j].phase = ENEMY_DEAD;
                        enemiesRemaining--;
                        score += SCORE_ENEMY_KILL;
                    } else {
                        enemies[j].phase = ENEMY_WALKING;
                        enemies[j].yVel = 2;
                    }

                }
                // -------------------------------
                // PHASE 2 -> DEAD
                // -------------------------------
                else if (enemies[j].phase == ENEMY_WALKING) {

                    enemies[j].active = 0;
                    enemies[j].phase = ENEMY_DEAD;

                    enemiesRemaining--;
                    score += SCORE_ENEMY_KILL;
                }

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
        if (!enemies[i].active) continue;

        enemies[i].oldX = enemies[i].x;
        enemies[i].oldY = enemies[i].y;

        // ==================================================
        // PHASE 1 : FLOATING ENEMY
        // ==================================================
        if (enemies[i].phase == ENEMY_FLOATING) {

            // Horizontal floating movement
            enemies[i].x += (enemies[i].direction == DIR_RIGHT)
                            ? enemies[i].xVel
                            : -enemies[i].xVel;

            if (enemies[i].x <= enemies[i].minX) {
                enemies[i].x = enemies[i].minX;
                enemies[i].direction = DIR_RIGHT;
            }

            if (enemies[i].x >= enemies[i].maxX) {
                enemies[i].x = enemies[i].maxX;
                enemies[i].direction = DIR_LEFT;
            }

            // Floating enemies can shoot
            enemies[i].shootTimer--;
            if (enemies[i].shootTimer <= 0) {
                spawnEnemyBullet(&enemies[i]);
                enemies[i].shootTimer = 75;
            }
        }

        // ==================================================
        // PHASE 2 : WALKING ENEMY (NO BALLOON)
        // ==================================================
        else if (enemies[i].phase == ENEMY_WALKING) {
            int step;
            int xDelta;

            // Apply gravity with collision, one pixel at a time
            enemies[i].yVel += 1;
            if (enemies[i].yVel > 3) enemies[i].yVel = 3;

            for (step = 0; step < enemies[i].yVel; step++) {
                if (canMoveTo(enemies[i].x, enemies[i].y + 1,
                              enemies[i].width, enemies[i].height)) {
                    enemies[i].y++;
                } else {
                    enemies[i].yVel = 0;
                    break;
                }
            }

            // Horizontal movement with collision
            xDelta = (enemies[i].direction == DIR_RIGHT) ? enemies[i].xVel : -enemies[i].xVel;
            if (canMoveTo(enemies[i].x + xDelta, enemies[i].y,
                          enemies[i].width, enemies[i].height)) {
                enemies[i].x += xDelta;
            } else {
                // Flip direction on wall collision
                enemies[i].direction = (enemies[i].direction == DIR_RIGHT) ? DIR_LEFT : DIR_RIGHT;
            }

            if (enemies[i].x <= enemies[i].minX) {
                enemies[i].x = enemies[i].minX;
                enemies[i].direction = DIR_RIGHT;
            }

            if (enemies[i].x >= enemies[i].maxX) {
                enemies[i].x = enemies[i].maxX;
                enemies[i].direction = DIR_LEFT;
            }
        }

        // ==================================================
        // PLAYER COLLISION
        // ==================================================
        if (collision(enemies[i].x, enemies[i].y,
                    enemies[i].width, enemies[i].height,
                    player.x, player.y,
                    player.width, player.height)) {

            // Player stomps enemy from above
            if (player.y + player.height <= enemies[i].y + 4 &&
                enemies[i].phase == ENEMY_WALKING) {

                enemies[i].active = 0;
                enemiesRemaining--;
                score += SCORE_ENEMY_KILL;

                player.yVel = -4; // bounce effect
            }
            else {
                damagePlayer();
            }
        }

        // Animation
        enemies[i].animCounter++;
        if (enemies[i].animCounter >= 10) {
            enemies[i].animCounter = 0;
            enemies[i].currentFrame =
                (enemies[i].currentFrame + 1) % 3;
        }
    }
}

// ======================================================
//                     DAMAGE / RESPAWN
// ======================================================

void damagePlayer(void) {
    int spawnGroundY;

    // Ignore damage while invincible
    if (player.invincibleTimer > 0) {
        return;
    }

    // Lose one life and grant invincibility frames
    player.lives--;
    player.invincibleTimer = 60;

    // Reset movement state
    player.yVel = 0;
    player.xVel = 0;
    player.grounded = 0;
    player.floatBoostTimer = 0;

    if (state == STATE_LEVEL1) {
        player.x = PLAYER_START_X;

        // Find the ground under the level 1 spawn
        spawnGroundY = findGroundYBelow(player.x, 0, player.width, player.height);
        player.y = (spawnGroundY >= 0) ? spawnGroundY : PLAYER_START_Y;
    } else if (state == STATE_LEVEL2) {
        // Respawn on the left platform in level 2.
        player.x = 24;
        spawnGroundY = findGroundYBelow(player.x, 0, player.width, player.height);
        player.y = (spawnGroundY >= 0) ? spawnGroundY : 28;
    }
}

// ======================================================
//                    COLLISION HELPERS
// ======================================================
u8 getCollisionPixel(int x, int y) {
    int usingLevel2 = (state == STATE_LEVEL2 || state == STATE_LEVEL2_INTRO ||
                       pausedState == STATE_LEVEL2);

    if (usingLevel2) {
        const u8* map = (const u8*) collisionMapTwoTiles;
        int tileX;
        int tileY;
        int tileIndex;
        int localX;
        int localY;
        int byteIndex;

        if (x < 0 || x >= LEVEL2_PIXEL_W || y < 0 || y >= LEVEL2_PIXEL_H) {
            return CM_BLOCKED;
        }

        // collisionMapTwo was exported as tiled 8bpp data.
        // Convert pixel position into:
        //   1) tile index
        //   2) pixel offset inside that 8x8 tile
        tileX = x >> 3;
        tileY = y >> 3;
        tileIndex = tileY * LEVEL2_MAP_W + tileX;

        localX = x & 7;
        localY = y & 7;

        // 8x8 tile, 8bpp => 64 bytes per tile
        byteIndex = tileIndex * 64 + localY * 8 + localX;

        return map[byteIndex];
    } else {
        const u8* map = (const u8*) collisionMapOneBitmap;

        if (x < 0 || x >= LEVEL1_PIXEL_W || y < 0 || y >= LEVEL1_PIXEL_H) {
            return CM_BLOCKED;
        }

        return map[y * LEVEL1_PIXEL_W + x];
    }
}

int isSolidPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_BLOCKED;
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
    int maxWidth = (state == STATE_LEVEL2 || state == STATE_LEVEL2_INTRO || pausedState == STATE_LEVEL2)
        ? LEVEL2_PIXEL_W
        : LEVEL1_PIXEL_W;
    int maxHeight = (state == STATE_LEVEL2 || state == STATE_LEVEL2_INTRO || pausedState == STATE_LEVEL2)
        ? LEVEL2_PIXEL_H
        : LEVEL1_PIXEL_H;

    // Reject if outside the playable map
    if (x < 0 || y < 0 || x + width > maxWidth || y + height > maxHeight) {
        return 0;
    }

    // Only blocked pixels stop movement
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
    int maxHeight = (state == STATE_LEVEL2 || state == STATE_LEVEL2_INTRO || pausedState == STATE_LEVEL2)
        ? LEVEL2_PIXEL_H
        : LEVEL1_PIXEL_H;

    // Scan downward until we hit solid ground
    for (testY = y; testY < maxHeight - height; testY++) {
        if (!canMoveTo(x, testY, width, height)) {
            return testY - 1;
        }
    }

    return -1;
}

int findLowestGroundYAtX(int x, int width, int height) {
    return findGroundYBelow(x, 0, width, height);
}

int nextRandomSpriteVariant(void) {
    // Simple xorshift RNG.
    // Good enough for choosing one of the 4 balloon colors.
    spriteRngState ^= spriteRngState << 13;
    spriteRngState ^= spriteRngState >> 17;
    spriteRngState ^= spriteRngState << 5;

    return (int)(spriteRngState & 3);
}

int absInt(int x) {
    return (x < 0) ? -x : x;
}

int getFloatVelocityForLives(int lives) {
    // More lives = stronger/faster upward flight.
    //
    // IMPORTANT:
    // Upward movement uses NEGATIVE y velocity in this project.
    // So 3 lives should be the most negative (fastest upward).
    //
    // You can tweak these a little if needed, but this gives a clearly
    // noticeable difference between 1, 2, and 3 lives.
    switch (lives) {
        case 3:
            return -4;   // fastest upward
        case 2:
            return -3;   // medium upward
        case 1:
        default:
            return -2;   // weakest upward
    }
}

// ======================================================
//                     HUD DRAWING
// ======================================================

void clearHUD(void) {
    int i;
    for (i = 0; i < 32 * 32; i++) {
        SCREENBLOCK[HUD_SCREENBLOCK].tilemap[i] = TILE_BLANK | TILEMAP_ENTRY_PALROW(15);
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
    SCREENBLOCK[HUD_SCREENBLOCK].tilemap[row * 32 + col] =
        glyphIndexForChar(c) | TILEMAP_ENTRY_PALROW(15);
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

static void prepareMenuLayers(void) {
    initMode0();

    // BG0 = HUD/text layer
    setupBackground(0,
        BG_PRIORITY(0) |
        BG_CHARBLOCK(HUD_CHARBLOCK) |
        BG_SCREENBLOCK(HUD_SCREENBLOCK) |
        BG_4BPP |
        BG_SIZE_SMALL);

    // BG1 = menu/state background image layer
    setupBackground(1,
        BG_PRIORITY(1) |
        BG_CHARBLOCK(LEVEL_CHARBLOCK) |
        BG_SCREENBLOCK(LEVEL_SCREENBLOCK) |
        BG_4BPP |
        BG_SIZE_SMALL);

    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);

    clearHUD();
    clearScreenblock(LEVEL_SCREENBLOCK);
    hideSprites();
}

static void loadMenuBackground(void) {
    int row;
    int col;
    int tileId = 0;

    // Load the 4bpp background palette for the menu/state screen art.
    loadBgPalette(screenPal, screenPalLen / 2);

    // Keep HUD font colors in palette row 15 so the text overlays still draw
    // correctly on top of the menu background.
    BG_PALETTE[240] = BLACK;
    BG_PALETTE[241] = WHITE;

    // The menu background uses 600 4bpp tiles, which spills past one charblock.
    // Clear both charblocks that the art can touch so there is no stale tile data.
    clearCharBlock(LEVEL_CHARBLOCK);
    clearCharBlock(LEVEL_CHARBLOCK + 1);

    // Copy the full 4bpp tile data into BG tile memory starting at charblock 1.
    DMANow(3,
        (volatile void*) screenTiles,
        (volatile void*) &CHARBLOCK[LEVEL_CHARBLOCK].tileimg[0],
        screenTilesLen / 2);

    clearScreenblock(LEVEL_SCREENBLOCK);

    // Build the 30x20 tilemap for the full-screen image.
    // Since the art was exported as sequential tiles, tile IDs go in order.
    for (row = 0; row < 20; row++) {
        for (col = 0; col < 30; col++) {
            SCREENBLOCK[LEVEL_SCREENBLOCK].tilemap[row * 32 + col] =
                TILEMAP_ENTRY_TILEID(tileId++);
        }
    }
}

static void drawInfoScreen(const char* title, const char* subtitle) {
    prepareMenuLayers();
    loadMenuBackground();

    putStringHUD(2, 8, title);
    putStringHUD(2, 14, subtitle);
}

static void restorePausedGameplayLayers(void) {
    initMode0();

    if (pausedState == STATE_LEVEL1) {
        // Recreate the same BG setup used by level 1.
        setupBackground(0,
            BG_PRIORITY(0) |
            BG_CHARBLOCK(HUD_CHARBLOCK) |
            BG_SCREENBLOCK(HUD_SCREENBLOCK) |
            BG_4BPP |
            BG_SIZE_SMALL);

        setupBackground(1,
            BG_PRIORITY(1) |
            BG_CHARBLOCK(LEVEL_CHARBLOCK) |
            BG_SCREENBLOCK(LEVEL_SCREENBLOCK) |
            BG_4BPP |
            BG_SIZE_SMALL);

        // Reload gameplay palette/tiles because pause used the screen image.
        loadBgPalette(tilesetPal, tilesetPalLen / 2);

        // Keep HUD text colors in palette row 15.
        BG_PALETTE[240] = BLACK;
        BG_PALETTE[241] = WHITE;

        loadTilesToCharblock(LEVEL_CHARBLOCK, tilesetTiles, tilesetTilesLen / 2);

        // Rebuild the actual level 1 tilemap art into the gameplay screenblock.
        buildLevel1MapAndCollision();

        // Restore the camera offsets from before pausing.
        setBackgroundOffset(0, 0, 0);
        setBackgroundOffset(1, level1HOff, level1VOff);

        // Clear old pause text from the HUD map.
        clearHUD();
    } else if (pausedState == STATE_LEVEL2) {
        // Recreate the same BG setup used by level 2.
        setupBackground(0,
            BG_PRIORITY(0) |
            BG_CHARBLOCK(HUD_CHARBLOCK) |
            BG_SCREENBLOCK(HUD_SCREENBLOCK) |
            BG_4BPP |
            BG_SIZE_SMALL);

        setupBackground(1,
            BG_PRIORITY(1) |
            BG_CHARBLOCK(LEVEL_CHARBLOCK) |
            BG_SCREENBLOCK(LEVEL_SCREENBLOCK) |
            BG_4BPP |
            BG_SIZE_WIDE);

        // Reload gameplay palette/tiles because pause used the screen image.
        loadBgPalette(tilesetPal, tilesetPalLen / 2);

        // Keep HUD text colors in palette row 15.
        BG_PALETTE[240] = BLACK;
        BG_PALETTE[241] = WHITE;

        loadTilesToCharblock(LEVEL_CHARBLOCK, tilesetTiles, tilesetTilesLen / 2);

        // Rebuild the level 2 map into the gameplay screenblocks.
        buildLevel2Map();

        // Restore the camera position from before pausing.
        setBackgroundOffset(0, 0, 0);
        setBackgroundOffset(1, level2HOff, level2VOff);

        // Clear old pause text from the HUD map.
        clearHUD();
    }
}

static void clearMenuBackground(unsigned short entry) {
    int i;
    /* Fill the level screenblock with a single tile entry (usually 0 = blank) */
    for (i = 0; i < 32 * 32; i++) {
        SCREENBLOCK[LEVEL_SCREENBLOCK].tilemap[i] = entry;
    }
}

static void drawStartScreen(void) {
    drawInfoScreen("BALLOON FIGHT", "PRESS START TO PLAY");
}

static void drawLevel1IntroScreen(void) {
    drawInfoScreen("BALLOON FIGHT", "LEVEL ONE PRESS START");
}

static void drawLevel2IntroScreen(void) {
    prepareMenuLayers();
    clearMenuBackground(0);
    drawInfoScreen("BALLOON FIGHT", "LEVEL TWO PRESS START");
}

static void drawPauseScreen(void) {
    drawInfoScreen("BALLOON FIGHT", "PAUSED PRESS START");
}

static void drawWinScreen(void) {
    drawInfoScreen("BALLOON FIGHT", "YOU WON PRESS START");
}

static void drawLoseScreen(void) {
    drawInfoScreen("BALLOON FIGHT", "OH NO YOU LOST");
}

// ======================================================
//                  SPRITE DRAWING HELPERS
// ======================================================

//HERE
void drawPlayerSprite(int screenX, int screenY) {
    int tileBase;
    int movedUp   = (player.y < player.oldY);
    int movedDown = (player.y > player.oldY) && !player.grounded;

    if (movedUp) {
        tileBase = OBJ_TILE_PLAYER_UP + player.currentFrame * 16;
    } else if (movedDown) {
        tileBase = OBJ_TILE_PLAYER_DOWN + player.currentFrame * 16;
    } else if (player.direction == DIR_LEFT) {
        tileBase = OBJ_TILE_PLAYER_LEFT + player.currentFrame * 16;
    } else {
        tileBase = OBJ_TILE_PLAYER_RIGHT + player.currentFrame * 16;
    }

    shadowOAM[0].attr0 = ATTR0_Y(screenY - 8) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[0].attr1 = ATTR1_X(screenX - 4) | ATTR1_MEDIUM;
    shadowOAM[0].attr2 = ATTR2_TILEID(tileBase) | ATTR2_PALROW(0);
}

void drawEnemySprite(int enemyIndex, int oamIndex, int screenX, int screenY) {
    int tileBase = OBJ_TILE_ENEMY + (enemies[enemyIndex].currentFrame % 3) * 16;

    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY - 8) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX - 4) | ATTR1_MEDIUM;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(tileBase) | ATTR2_PALROW(0);
}

void drawBulletSprite(int oamIndex, int screenX, int screenY, int enemyBullet) {
    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_TINY;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(enemyBullet ? OBJ_TILE_EBULLET : OBJ_TILE_BULLET) | ATTR2_PALROW(0);
}

void drawBalloonSprite(int oamIndex, int screenX, int screenY, int variant) {
    int tileBase = OBJ_TILE_BALLOON + ((variant & 3) * 16);

    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_MEDIUM;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(tileBase) | ATTR2_PALROW(0);
}

void drawStarSprite(int oamIndex, int screenX, int screenY) {
    // Simple 8x8 OBJ star built directly in code.
    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_TINY;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(OBJ_TILE_STAR) | ATTR2_PALROW(0);
}

void drawDoorSprite(int screenX, int screenY) {
    if (!doorOpen) {
        // Closed door: 32x32 pixels = SQUARE + MEDIUM
        shadowOAM[120].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
        shadowOAM[120].attr1 = ATTR1_X(screenX) | ATTR1_MEDIUM;
        shadowOAM[120].attr2 = ATTR2_TILEID(OBJ_TILE_DOOR_CLOSED) | ATTR2_PALROW(0);
        shadowOAM[121].attr0 = ATTR0_HIDE;
    } else {
        // Open door top: 32x32 pixels = SQUARE + MEDIUM
        shadowOAM[120].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
        shadowOAM[120].attr1 = ATTR1_X(screenX) | ATTR1_MEDIUM;
        shadowOAM[120].attr2 = ATTR2_TILEID(OBJ_TILE_DOOR_OPEN_TOP) | ATTR2_PALROW(0);
        // Open door bottom row: 32x8 pixels = WIDE + SMALL, 32px below the top sprite
        shadowOAM[121].attr0 = ATTR0_Y(screenY + 32) | ATTR0_WIDE | ATTR0_4BPP;
        shadowOAM[121].attr1 = ATTR1_X(screenX) | ATTR1_SMALL;
        shadowOAM[121].attr2 = ATTR2_TILEID(OBJ_TILE_DOOR_OPEN_BOT) | ATTR2_PALROW(0);
    }
}

void hideUnusedSpritesFrom(int startIndex) {
    int i;
    for (i = startIndex; i < 128; i++) {
        if (doorVisible && (i == 120 || i == 121)) {
            continue;
        }
        shadowOAM[i].attr0 = ATTR0_HIDE;
    }
}

static void buildSimpleBulletTile(int tileIndex, u8 colorIndex) {
    /* Build a simple 8x8 circular bullet tile using the given palette index. */
    u8 pixels[64] = {
        0,0,colorIndex,colorIndex,colorIndex,colorIndex,0,0,
        0,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,0,
        colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,
        colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,
        colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,
        colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,
        0,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,colorIndex,0,
        0,0,colorIndex,colorIndex,colorIndex,colorIndex,0,0
    };
    write4bppTile(OBJ_TILE_MEM, tileIndex, pixels);
}

static void buildSimpleStarTile(int tileIndex) {
    // Palette indices pulled from the sprite sheet palette:
    // 0  = transparent
    // 5  = dark outline
    // 15 = orange fill
    // 3  = darker orange accent
    u8 pixels[64] = {
        0,0,0,5,5,0,0,0,
        0,0,5,15,15,5,0,0,
        0,5,15,15,15,15,5,0,
        5,15,15,3,3,15,15,5,
        0,5,15,15,15,15,5,0,
        0,0,5,15,15,5,0,0,
        0,0,0,5,5,0,0,0,
        0,0,0,0,0,0,0,0
    };

    write4bppTile(OBJ_TILE_MEM, tileIndex, pixels);
}

// ======================================================
//                  GRAPHICS / TILE BUILDING
// ======================================================

static void initGraphics(void) {
    // Keep the whole game in Mode 0.
    initMode0();

    // Load the artist-made tileset and palette used by the level tilemaps.
    loadBgPalette(tilesetPal, tilesetPalLen / 2);
    loadTilesToCharblock(LEVEL_CHARBLOCK, tilesetTiles, tilesetTilesLen / 2);

    // Build HUD font tiles in their own charblock.
    buildHudTiles();

    // Load sprite palette BEFORE building sprite tiles so that the colour
    // assignments made in buildSpriteTiles() use the correct palette indices.
    // This call was previously missing, which left SPRITE_PAL[0] as whatever
    // the GBA BIOS happened to leave in memory (usually 0 = black), causing
    // all transparent regions of sprites to render as solid black.
    initPalettes();

    // Build OBJ tiles for player, enemies, bullets, balloons, stars, and door.
    buildSpriteTiles();

    // Hide every sprite initially.
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
    SPRITE_PAL[0] = RGB(0, 0, 0);
    SPRITE_PAL[1] = RGB(31, 31, 31);
    SPRITE_PAL[2] = RGB(31, 24, 8);
    // Pastel balloon colours — one per palette slot so each balloon index can
    // pick a different hue.  Slots 3-8 map to: pink, sky-blue, mint-green,
    // lemon-yellow, lavender, and peach.
    SPRITE_PAL[3] = RGB(31, 18, 22);   /* pastel pink    */
    SPRITE_PAL[4] = RGB(16, 24, 31);   /* pastel sky-blue */
    SPRITE_PAL[5] = RGB(16, 31, 20);   /* pastel mint    */
    SPRITE_PAL[6] = RGB(31, 31, 14);   /* pastel lemon   */
    SPRITE_PAL[7] = RGB(24, 18, 31);   /* pastel lavender*/
    SPRITE_PAL[8] = RGB(31, 22, 14);   /* pastel peach   */
    SPRITE_PAL[9] = RGB(20, 20, 20);
    SPRITE_PAL[10] = RGB(31, 0, 0);
    SPRITE_PAL[11] = RGB(0, 31, 0);
    SPRITE_PAL[12] = RGB(0, 0, 31);
    SPRITE_PAL[13] = RGB(31, 31, 31);
    SPRITE_PAL[14] = RGB(31, 20, 20);
    SPRITE_PAL[15] = RGB(28, 28, 10);
}

static void buildHudTiles(void) {
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
    u32* base = (u32*) 0x06000000;

    clearCharBlock(HUD_CHARBLOCK);

    makeSolidTile(base + (HUD_CHARBLOCK * 1024), TILE_BLANK, 0);

    for (i = 0; i < 39; i++) {
        makeGlyphTile(base + (HUD_CHARBLOCK * 1024), FONT_BASE_TILE + i, glyphs[i], 1, 0);
    }
}

static void clearObjTiles(void) {
    int i;
    for (i = 0; i < 1024 * 16; i++) {
        ((u16*)OBJ_TILE_MEM)[i] = 0;
    }
}

static void copyObjTileRemapped(int dstTileIndex, int srcTileX, int srcTileY) {
    int i;
    int srcTileIndex = srcTileY * 32 + srcTileX;
    const unsigned char* src = ((const unsigned char*)spriteSheetTiles) + srcTileIndex * 32;
    volatile unsigned short* dst = (volatile unsigned short*)((unsigned char*)OBJ_TILE_MEM + dstTileIndex * 32);

    // The sprite sheet already uses palette index 0 as magenta/transparent.
    // Preserve the original indices exactly so the artist-authored colors line up
    // with the exported palette. Only 16-bit writes are allowed in VRAM.
    for (i = 0; i < 32; i += 2) {
        unsigned char b0 = src[i];
        unsigned char b1 = src[i + 1];

        unsigned char lo0 = b0 & 0x0F;
        unsigned char hi0 = (b0 >> 4) & 0x0F;
        unsigned char lo1 = b1 & 0x0F;
        unsigned char hi1 = (b1 >> 4) & 0x0F;

        dst[i / 2] = (lo0) | (hi0 << 4) | (lo1 << 8) | (hi1 << 12);
    }
}

static void clearObjTileIndex(int tileIndex) {
    int i;
    volatile unsigned short* dst = (volatile unsigned short*)((unsigned char*)OBJ_TILE_MEM + tileIndex * 32);

    // Write 16-bit zeros -- GBA VRAM ignores 8-bit writes.
    for (i = 0; i < 16; i++) {
        dst[i] = 0;
    }
}

static void copyFrameTo32x32Slot(int dstBaseTile, int srcTileX, int srcTileY, int widthTiles, int heightTiles) {
    int row;
    int col;

    // 32x32 OBJ in 1D layout uses 16 contiguous tiles:
    // row 0: +0 +1 +2 +3
    // row 1: +4 +5 +6 +7
    // row 2: +8 +9 +10 +11
    // row 3: +12 +13 +14 +15
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            int dstTile = dstBaseTile + row * 4 + col;

            if (row < heightTiles && col < widthTiles) {
                copyObjTileRemapped(dstTile, srcTileX + col, srcTileY + row);
            } else {
                clearObjTileIndex(dstTile);
            }
        }
    }
}

static void buildSpriteTiles(void) {
    int i;

    clearObjTiles();

    // Copy the exported sprite palette directly.
    for (i = 0; i < 256; i++) {
        SPRITE_PAL[i] = spriteSheetPal[i];
    }

    // Palette index 0 in the sheet is magenta and should be transparent.
    SPRITE_PAL[0] = 0;

    // --------------------------------------------------
    // Duck frames
    // Each frame is 3x3 tiles.
    // Frames are packed directly next to each other horizontally.
    // Direction rows are separated vertically by one blank tile.
    // --------------------------------------------------
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_RIGHT + 0 * 16,  0,  0, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_RIGHT + 1 * 16,  3,  0, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_RIGHT + 2 * 16,  6,  0, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_RIGHT + 3 * 16,  9,  0, 3, 3);

    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_LEFT  + 0 * 16,  0,  4, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_LEFT  + 1 * 16,  3,  4, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_LEFT  + 2 * 16,  6,  4, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_LEFT  + 3 * 16,  9,  4, 3, 3);

    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_UP    + 0 * 16,  0,  8, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_UP    + 1 * 16,  3,  8, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_UP    + 2 * 16,  6,  8, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_UP    + 3 * 16,  9,  8, 3, 3);

    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_DOWN  + 0 * 16,  0, 12, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_DOWN  + 1 * 16,  3, 12, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_DOWN  + 2 * 16,  6, 12, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_PLAYER_DOWN  + 3 * 16,  9, 12, 3, 3);
        
    // --------------------------------------------------
    // Balloons
    // Each balloon frame is 3x3 tiles on row 17.
    // The artist exported four actual color variants at x = 0, 3, 6, 9.
    // Copy each one into its own OBJ slot so we preserve the pastel colors
    // exactly as they appear in the sprite sheet.
    // --------------------------------------------------
    copyFrameTo32x32Slot(OBJ_TILE_BALLOON + 0 * 16, 0, 17, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_BALLOON + 1 * 16, 3, 17, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_BALLOON + 2 * 16, 6, 17, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_BALLOON + 3 * 16, 9, 17, 3, 3);

    // --------------------------------------------------
    // Enemy
    // 3 frames, 3x3 tiles each, row starts at y = 22
    // Starts: x = 0, 3, 6
    // --------------------------------------------------
    copyFrameTo32x32Slot(OBJ_TILE_ENEMY + 0 * 16, 0, 22, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_ENEMY + 1 * 16, 3, 22, 3, 3);
    copyFrameTo32x32Slot(OBJ_TILE_ENEMY + 2 * 16, 6, 22, 3, 3);

    // --------------------------------------------------
    // Star
    // Build a simple 8x8 star directly in code so it stays fully Mode 0
    // and doesn't depend on the sprite sheet.
    // --------------------------------------------------
    buildSimpleStarTile(OBJ_TILE_STAR);
    // --------------------------------------------------
    // Door
    // Top row right side.
    // Keep this here for now.
    // Closed/open can be split later if needed.
    // --------------------------------------------------
    // Door closed: 4x4 tiles at sheet (13,0)
    // --------------------------------------------------
    copyFrameTo32x32Slot(OBJ_TILE_DOOR_CLOSED, 13, 0, 4, 4);

    // --------------------------------------------------
    // Door open top: rows 0-3 at sheet (18,0), 4x4 tiles
    // --------------------------------------------------
    copyFrameTo32x32Slot(OBJ_TILE_DOOR_OPEN_TOP, 18, 0, 4, 4);

    // --------------------------------------------------
    // Door open bottom: row 4 at sheet (18,4), 4 tiles wide x 1 tall
    // WIDE+SMALL OBJ in 1D uses 4 consecutive tile slots
    // --------------------------------------------------
    copyObjTileRemapped(OBJ_TILE_DOOR_OPEN_BOT + 0, 18, 4);
    copyObjTileRemapped(OBJ_TILE_DOOR_OPEN_BOT + 1, 19, 4);
    copyObjTileRemapped(OBJ_TILE_DOOR_OPEN_BOT + 2, 20, 4);
    copyObjTileRemapped(OBJ_TILE_DOOR_OPEN_BOT + 3, 21, 4);

    // --------------------------------------------------
    // Simple bullets so bullets always show up
    // --------------------------------------------------
    buildSimpleBulletTile(OBJ_TILE_BULLET, 3);
    buildSimpleBulletTile(OBJ_TILE_EBULLET, 7);
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