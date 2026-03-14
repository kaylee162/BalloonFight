#include "gba.h"
#include "mode0.h"
#include "sprites.h"
#include "print.h"
#include "game.h"

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
static GameState state;

// Player
static Player player;

// Object pools
static Bullet playerBullets[MAX_PLAYER_BULLETS];
static Bullet enemyBullets[MAX_ENEMY_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static Balloon balloons[MAX_BALLOONS];
static Star stars[MAX_STARS];

// Score and progression
static int score;
static int level2BalloonsRemaining;
static int enemiesRemaining;

// Level state
// Level state
static int level1HOff;
static int level1VOff;
static int level2HOff;
static int menuNeedsRedraw;

// Door state for finishing level 1
static int doorVisible;
static int doorX;
static int doorY;

// Frame counter
static int frameCount;

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
static void clearHUD(void);
static void drawHUD(void);
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
static void initLevel1(void);
static void initLevel2(void);
static void resetPlayerForCurrentLevel(void);
static void buildLevel1MapAndCollision(void);
static void buildLevel2Map(void);


// Update handlers
static void updateStart(void);
static void updateLevel1(void);
static void updateLevel2(void);
static void updatePause(void);
static void updateWin(void);
static void updateLose(void);

// Draw handlers
static void drawLevel1(void);
static void drawLevel2(void);

// Object updates
static void updatePlayerLevel1(void);
static void updatePlayerLevel2(void);
static void updatePlayerAnimation(void);
static void firePlayerBullet(void);
static void updatePlayerBullets(void);
static void updateEnemyBullets(void);
static void updateEnemies(void);
static void spawnEnemyBullet(Enemy* e);
static void updateBalloonsLevel1(void);
static void updateBalloonsLevel2(void);
static void updateStars(void);

// Physics / collisions
static int isSolidPixel(int x, int y);
static int canMoveTo(int x, int y, int width, int height);
static int findGroundYBelow(int x, int y, int width, int height);
static int findLowestGroundYAtX(int x, int width, int height);
static void damagePlayer(void);

// Sprite drawing
static void drawSpritesLevel1(void);
static void drawSpritesLevel2(void);
static void drawPlayerSprite(int screenX, int screenY);
static void drawEnemySprite(int i, int screenX, int screenY);
static void drawBulletSprite(int oamIndex, int screenX, int screenY, int enemyBullet);
static void drawBalloonSprite(int oamIndex, int screenX, int screenY);
static void drawStarSprite(int oamIndex, int screenX, int screenY);
static void drawDoorSprite(int screenX, int screenY);

// Level One 
static u8 getCollisionPixel(int x, int y);
static int rectTouchesColor(int x, int y, int width, int height, u8 color);
static int isLadderPixel(int x, int y);
static int isHazardPixel(int x, int y);
static int isGoalPixel(int x, int y);


// Utility
static void hideUnusedSpritesFrom(int startIndex);
static int absInt(int x);

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
//                    GRAPHICS INITIALIZATION
// ======================================================

// Sets up palettes and all generated graphics
static void initGraphics(void) {
    // Initialize both BG and OBJ palettes
    initPalettes();

    // Build the HUD / font tiles in BG charblock 0
    buildHudTiles();

    // Build the gameplay background tiles in BG charblock 1
    buildBackgroundTiles();

    // Build sprite graphics in OBJ tile memory
    buildSpriteTiles();

    // Hide all sprites at startup
    hideSprites();
}

// Initializes a simple palette for backgrounds and sprites
static void initPalettes(void) {
    // Background palette
    BG_PALETTE[0]  = BLACK;               // black
    BG_PALETTE[1]  = WHITE;               // white
    BG_PALETTE[2]  = RGB(6, 10, 25);      // dark blue
    BG_PALETTE[3]  = RED;                 // red
    BG_PALETTE[4]  = GREEN;               // green
    BG_PALETTE[5]  = YELLOW;              // yellow
    BG_PALETTE[6]  = CYAN;                // cyan
    BG_PALETTE[7]  = RGB(18, 18, 18);     // gray
    BG_PALETTE[8]  = RGB(10, 25, 31);     // sky blue
    BG_PALETTE[9]  = RGB(24, 24, 30);     // cloud white
    BG_PALETTE[10] = RGB(20, 14, 6);      // brown
    BG_PALETTE[11] = RGB(31, 20, 10);     // tan
    BG_PALETTE[12] = RGB(31, 10, 20);     // pink
    BG_PALETTE[13] = RGB(14, 31, 14);     // lime
    BG_PALETTE[14] = RGB(25, 25, 0);      // gold
    BG_PALETTE[15] = RGB(31, 31, 31);     // bright white

    // Sprite palette
    SPRITE_PAL[0]  = RGB(31, 0, 31);      // transparent color key
    SPRITE_PAL[1]  = WHITE;               // white
    SPRITE_PAL[2]  = RGB(6, 10, 25);      // dark blue
    SPRITE_PAL[3]  = RED;                 // red
    SPRITE_PAL[4]  = GREEN;               // green
    SPRITE_PAL[5]  = YELLOW;              // yellow
    SPRITE_PAL[6]  = CYAN;                // cyan
    SPRITE_PAL[7]  = RGB(18, 18, 18);     // gray
    SPRITE_PAL[8]  = RGB(31, 20, 10);     // tan
    SPRITE_PAL[9]  = RGB(31, 10, 20);     // pink
    SPRITE_PAL[10] = RGB(14, 31, 14);     // lime
    SPRITE_PAL[11] = RGB(25, 25, 0);      // gold
    SPRITE_PAL[12] = RGB(31, 31, 31);     // bright white
    SPRITE_PAL[13] = RGB(8, 8, 8);        // near black
    SPRITE_PAL[14] = RGB(12, 20, 31);     // light blue
    SPRITE_PAL[15] = RGB(31, 31, 31);     // bright white
}

// Creates HUD / menu font tiles
static void buildHudTiles(void) {
    // Blank tile
    makeSolidTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), TILE_BLANK, 0);

    // Basic font glyphs used by the HUD and menus
    // Each byte is one 8-pixel row, MSB = leftmost pixel
    static const u8 glyph0[8] = {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00};
    static const u8 glyph1[8] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00};
    static const u8 glyph2[8] = {0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00};
    static const u8 glyph3[8] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00};
    static const u8 glyph4[8] = {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00};
    static const u8 glyph5[8] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00};
    static const u8 glyph6[8] = {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00};
    static const u8 glyph7[8] = {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00};
    static const u8 glyph8[8] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00};
    static const u8 glyph9[8] = {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00};

    static const u8 glyphA[8] = {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00};
    static const u8 glyphB[8] = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00};
    static const u8 glyphC[8] = {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00};
    static const u8 glyphD[8] = {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00};
    static const u8 glyphE[8] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00};
    static const u8 glyphG[8] = {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00};
    static const u8 glyphI[8] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00};
    static const u8 glyphL[8] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00};
    static const u8 glyphN[8] = {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00};
    static const u8 glyphO[8] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00};
    static const u8 glyphP[8] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00};
    static const u8 glyphR[8] = {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00};
    static const u8 glyphS[8] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00};
    static const u8 glyphT[8] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00};
    static const u8 glyphU[8] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00};
    static const u8 glyphV[8] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00};
    static const u8 glyphW[8] = {0x66,0x66,0x66,0x7E,0x7E,0x76,0x66,0x00};
    static const u8 glyphY[8] = {0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0x00};
    static const u8 glyphColon[8] = {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00};
    static const u8 glyphDash[8] = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00};
    static const u8 glyphExcl[8] = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00};

    // Store them starting at FONT_BASE_TILE
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 0, glyph0, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 1, glyph1, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 2, glyph2, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 3, glyph3, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 4, glyph4, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 5, glyph5, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 6, glyph6, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 7, glyph7, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 8, glyph8, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 9, glyph9, 1, 0);

    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 10, glyphA, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 11, glyphB, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 12, glyphC, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 13, glyphD, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 14, glyphE, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 15, glyphG, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 16, glyphI, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 17, glyphL, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 18, glyphN, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 19, glyphO, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 20, glyphP, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 21, glyphR, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 22, glyphS, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 23, glyphT, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 24, glyphU, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 25, glyphV, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 26, glyphW, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 27, glyphY, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 28, glyphColon, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 29, glyphDash, 1, 0);
    makeGlyphTile(BG_TILE_MEM + (HUD_CHARBLOCK * 2048), FONT_BASE_TILE + 30, glyphExcl, 1, 0);
}

// Creates a few simple background tiles used for the levels
static void buildBackgroundTiles(void) {
    // Base pointer for BG charblock 1
    u32* bgBase = BG_TILE_MEM + (LEVEL_CHARBLOCK * 2048);

    // Blank tile
    makeSolidTile(bgBase, TILE_BLANK, 0);

    // Solid brown tile
    makeSolidTile(bgBase, TILE_SOLID, 10);

    // Platform checker tile
    makeCheckerTile(bgBase, TILE_PLATFORM, 11, 10);

    // Sky tile
    makeSolidTile(bgBase, TILE_SKY, 8);

    // Cloud tile
    makeCheckerTile(bgBase, TILE_CLOUD, 9, 8);

    // Star background tile
    makeCheckerTile(bgBase, TILE_STARBG, 2, 8);

    // Door tile
    makeCheckerTile(bgBase, TILE_DOOR, 14, 10);

    // Balloon tile for background decoration if needed
    makeCheckerTile(bgBase, TILE_BALLOONBG, 12, 8);
}

// Builds simple sprite graphics
static void buildSpriteTiles(void) {
    // First clear a safe amount of OBJ tile memory
    clearObjTiles();

    // Build 4 tiles of a 16x16 sprite
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_PLAYER0;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        // Transparent outside shape
                        if (px >= 3 && px <= 12 && py >= 2 && py <= 13) {
                            // Body
                            color = 6;

                            // Shirt center
                            if (py >= 6 && py <= 13 && px >= 4 && px <= 11) {
                                color = 4;
                            }

                            // Face
                            if (py >= 2 && py <= 5 && px >= 5 && px <= 10) {
                                color = 8;
                            }

                            // Balloons above player to suggest remaining balloons
                            if (py <= 2 && (px == 5 || px == 8 || px == 11)) {
                                color = 5;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }

    // ------------------------------
    // Player frame 1
    // ------------------------------
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_PLAYER1;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        if (px >= 3 && px <= 12 && py >= 2 && py <= 13) {
                            color = 6;

                            if (py >= 6 && py <= 13 && px >= 4 && px <= 11) {
                                color = 13;
                            }

                            if (py >= 2 && py <= 5 && px >= 5 && px <= 10) {
                                color = 8;
                            }

                            // Slightly shifted balloon placement for animation
                            if (py <= 2 && (px == 4 || px == 8 || px == 12)) {
                                color = 5;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }

    // ------------------------------
    // Floating enemy frame 0
    // ------------------------------
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_ENEMYF0;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        if (px >= 2 && px <= 13 && py >= 3 && py <= 13) {
                            color = 3;

                            if (py <= 5 && px >= 4 && px <= 11) {
                                color = 9;
                            }

                            if (py <= 2 && (px == 4 || px == 8 || px == 12)) {
                                color = 12;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }

    // ------------------------------
    // Floating enemy frame 1
    // ------------------------------
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_ENEMYF1;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        if (px >= 2 && px <= 13 && py >= 3 && py <= 13) {
                            color = 3;

                            if (py <= 5 && px >= 4 && px <= 11) {
                                color = 8;
                            }

                            if (py <= 2 && (px == 3 || px == 8 || px == 13)) {
                                color = 12;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }

    // ------------------------------
    // Walking enemy frame 0
    // ------------------------------
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_ENEMYW0;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        if (px >= 3 && px <= 12 && py >= 4 && py <= 13) {
                            color = 3;

                            if (py <= 6 && px >= 5 && px <= 10) {
                                color = 8;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }

    // ------------------------------
    // Walking enemy frame 1
    // ------------------------------
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_ENEMYW1;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        if (px >= 3 && px <= 12 && py >= 4 && py <= 13) {
                            color = 13;

                            if (py <= 6 && px >= 5 && px <= 10) {
                                color = 8;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }

    // ------------------------------
    // Player bullet
    // ------------------------------
    {
        u8 tile[64];
        int x, y;
        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                tile[y * 8 + x] = 0;
                if (x >= 2 && x <= 5 && y >= 3 && y <= 4) {
                    tile[y * 8 + x] = 5;
                }
            }
        }
        write4bppTile(OBJ_TILE_MEM, OBJ_TILE_BULLET, tile);
    }

    // ------------------------------
    // Enemy bullet
    // ------------------------------
    {
        u8 tile[64];
        int x, y;
        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                tile[y * 8 + x] = 0;
                if (x >= 2 && x <= 5 && y >= 2 && y <= 5) {
                    tile[y * 8 + x] = 3;
                }
            }
        }
        write4bppTile(OBJ_TILE_MEM, OBJ_TILE_EBULLET, tile);
    }

    // ------------------------------
    // Balloon collectible (8x16 => 2 tall tiles)
    // ------------------------------
    {
        u8 tileTop[64];
        u8 tileBottom[64];
        int x, y;

        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                tileTop[y * 8 + x] = 0;
                tileBottom[y * 8 + x] = 0;

                if ((x >= 2 && x <= 5 && y >= 1 && y <= 6) ||
                    (x == 1 && y >= 2 && y <= 5) ||
                    (x == 6 && y >= 2 && y <= 5)) {
                    tileTop[y * 8 + x] = 12;
                }

                if (x == 3 && y <= 6) {
                    tileBottom[y * 8 + x] = 1;
                }
            }
        }

        write4bppTile(OBJ_TILE_MEM, OBJ_TILE_BALLOON, tileTop);
        write4bppTile(OBJ_TILE_MEM, OBJ_TILE_BALLOON + 1, tileBottom);
    }

    // ------------------------------
    // Star hazard (16x16 => 4 tiles)
    // ------------------------------
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_STAR;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        if ((px >= 6 && px <= 9) ||
                            (py >= 6 && py <= 9) ||
                            (px + py >= 11 && px + py <= 15) ||
                            (px - py >= -2 && px - py <= 2)) {
                            if (px >= 2 && px <= 13 && py >= 2 && py <= 13) {
                                color = 5;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }

    // ------------------------------
    // Door sprite (16x16)
    // ------------------------------
    {
        u8 tile[64];
        int tx, ty, x, y;
        int tileBase = OBJ_TILE_DOOR;

        for (ty = 0; ty < 2; ty++) {
            for (tx = 0; tx < 2; tx++) {
                for (y = 0; y < 8; y++) {
                    for (x = 0; x < 8; x++) {
                        int px = tx * 8 + x;
                        int py = ty * 8 + y;
                        u8 color = 0;

                        if (px >= 2 && px <= 13 && py >= 1 && py <= 14) {
                            color = 11;
                            if (px == 2 || px == 13 || py == 1 || py == 14) {
                                color = 10;
                            }
                            if (px == 10 && py == 8) {
                                color = 1;
                            }
                        }

                        tile[y * 8 + x] = color;
                    }
                }
                write4bppTile(OBJ_TILE_MEM, tileBase + ty * 2 + tx, tile);
            }
        }
    }
}

// Clears a chunk of OBJ tile memory
static void clearObjTiles(void) {
    int i;

    // Clear 256 u32s worth of OBJ tile data
    for (i = 0; i < 2048; i++) {
        OBJ_TILE_MEM[i] = 0;
    }
}

// ======================================================
//                        TILE HELPERS
// ======================================================

// Writes one 4bpp tile into VRAM
static void write4bppTile(u32* base, int tileIndex, const u8 pixels[64]) {
    int row;

    // Each 8x8 4bpp tile takes 32 bytes = 8 u32 rows
    for (row = 0; row < 8; row++) {
        u32 packed = 0;

        // Pack 8 4-bit pixels into one 32-bit row
        packed |= (pixels[row * 8 + 0] & 0xF) << 0;
        packed |= (pixels[row * 8 + 1] & 0xF) << 4;
        packed |= (pixels[row * 8 + 2] & 0xF) << 8;
        packed |= (pixels[row * 8 + 3] & 0xF) << 12;
        packed |= (pixels[row * 8 + 4] & 0xF) << 16;
        packed |= (pixels[row * 8 + 5] & 0xF) << 20;
        packed |= (pixels[row * 8 + 6] & 0xF) << 24;
        packed |= (pixels[row * 8 + 7] & 0xF) << 28;

        base[tileIndex * 8 + row] = packed;
    }
}

// Makes a tile filled with one palette color
static void makeSolidTile(u32* base, int tileIndex, u8 color) {
    u8 pixels[64];
    int i;

    for (i = 0; i < 64; i++) {
        pixels[i] = color;
    }

    write4bppTile(base, tileIndex, pixels);
}

// Makes a basic checker tile
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

// Makes a glyph tile from 1bpp row masks
static void makeGlyphTile(u32* base, int tileIndex, const u8 rows[8], u8 fg, u8 bg) {
    u8 pixels[64];
    int x, y;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            // Bit 7 is the leftmost pixel
            pixels[y * 8 + x] = (rows[y] & (1 << (7 - x))) ? fg : bg;
        }
    }

    write4bppTile(base, tileIndex, pixels);
}

// ======================================================
//                      HUD / TEXT HELPERS
// ======================================================

// Clears the HUD tilemap
static void clearHUD(void) {
    clearScreenblock(HUD_SCREENBLOCK);
}

// Draws the in-game HUD
static void drawHUD(void) {
    // Put labels on the first row
    putStringHUD(1, 1, "SCORE");
    putStringHUD(14, 1, "LIVES");

    // Put numeric values under them
    putNumberHUD(1, 3, score, 5);
    putNumberHUD(14, 3, player.lives, 1);
}

// Writes one character to the HUD tilemap
static void putCharHUD(int col, int row, char c) {
    int tile = glyphIndexForChar(c);

    // If character is unsupported, use blank tile
    if (tile < 0) {
        tile = TILE_BLANK;
    }

    SCREENBLOCK[HUD_SCREENBLOCK].tilemap[row * 32 + col] = tile;
}

// Writes a string to the HUD tilemap
static void putStringHUD(int col, int row, const char* str) {
    int i = 0;

    while (str[i]) {
        putCharHUD(col + i, row, str[i]);
        i++;
    }
}

// Writes a fixed-width number to the HUD tilemap
static void putNumberHUD(int col, int row, int value, int digits) {
    int i;

    // Draw right-aligned digits
    for (i = digits - 1; i >= 0; i--) {
        int digit = value % 10;
        putCharHUD(col + i, row, (char)('0' + digit));
        value /= 10;
    }
}

// Returns a tile index for supported glyph characters
static int glyphIndexForChar(char c) {
    // Digits
    if (c >= '0' && c <= '9') {
        return FONT_BASE_TILE + (c - '0');
    }

    // Selected uppercase letters
    switch (c) {
        case 'A': return FONT_BASE_TILE + 10;
        case 'B': return FONT_BASE_TILE + 11;
        case 'C': return FONT_BASE_TILE + 12;
        case 'D': return FONT_BASE_TILE + 13;
        case 'E': return FONT_BASE_TILE + 14;
        case 'G': return FONT_BASE_TILE + 15;
        case 'I': return FONT_BASE_TILE + 16;
        case 'L': return FONT_BASE_TILE + 17;
        case 'N': return FONT_BASE_TILE + 18;
        case 'O': return FONT_BASE_TILE + 19;
        case 'P': return FONT_BASE_TILE + 20;
        case 'R': return FONT_BASE_TILE + 21;
        case 'S': return FONT_BASE_TILE + 22;
        case 'T': return FONT_BASE_TILE + 23;
        case 'U': return FONT_BASE_TILE + 24;
        case 'V': return FONT_BASE_TILE + 25;
        case 'W': return FONT_BASE_TILE + 26;
        case 'Y': return FONT_BASE_TILE + 27;
        case ':': return FONT_BASE_TILE + 28;
        case '-': return FONT_BASE_TILE + 29;
        case '!': return FONT_BASE_TILE + 30;
        case ' ': return TILE_BLANK;
        default:  return TILE_BLANK;
    }
}

// ======================================================
//                        MENU SCREENS
// ======================================================

// Draws the start screen
static void drawStartScreen(void) {
    // Configure BG0 for text and BG1 for a simple background
    initMode0();

    // Reset scrolling
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);

    // Clear both tilemaps
    clearScreenblock(HUD_SCREENBLOCK);
    clearScreenblock(LEVEL_SCREENBLOCK);

    // Fill the background with sky tiles
    {
        int i;
        for (i = 0; i < 1024; i++) {
            SCREENBLOCK[LEVEL_SCREENBLOCK].tilemap[i] = TILE_SKY;
        }
    }

    // Show title and instructions
    putStringHUD(8, 4, "BALLOON BOY");
    putStringHUD(6, 8, "PRESS START");
    putStringHUD(3, 11, "A FLOATS  B SHOOTS");
    putStringHUD(6, 14, "LEVEL1 COMBAT");
    putStringHUD(6, 16, "LEVEL2 STARS");

    // Hide all sprites on the menu
    hideSprites();
}

// Draws the pause screen
static void drawPauseScreen(void) {
    clearHUD();

    // Leave the gameplay background visible, but write pause info on HUD
    putStringHUD(11, 6, "PAUSE");
    putStringHUD(8, 9, "START RESUME");
    putStringHUD(8, 11, "SELECT QUIT");
    putStringHUD(10, 14, "SCORE");
    putNumberHUD(10, 16, score, 5);

    // Hide all sprites while paused
    hideSprites();
}

// Draws the win screen
static void drawWinScreen(void) {
    initMode0();
    clearScreenblock(HUD_SCREENBLOCK);
    clearScreenblock(LEVEL_SCREENBLOCK);

    // Fill with celebratory sky
    {
        int i;
        for (i = 0; i < 1024; i++) {
            SCREENBLOCK[LEVEL_SCREENBLOCK].tilemap[i] = TILE_CLOUD;
        }
    }

    putStringHUD(13, 5, "YOU");
    putStringHUD(13, 7, "WIN");
    putStringHUD(10, 11, "SCORE");
    putNumberHUD(10, 13, score, 5);
    putStringHUD(6, 17, "PRESS START");

    hideSprites();
}

// Draws the lose screen
static void drawLoseScreen(void) {
    initMode0();
    clearScreenblock(HUD_SCREENBLOCK);
    clearScreenblock(LEVEL_SCREENBLOCK);

    // Fill with darker background
    {
        int i;
        for (i = 0; i < 1024; i++) {
            SCREENBLOCK[LEVEL_SCREENBLOCK].tilemap[i] = TILE_STARBG;
        }
    }

    putStringHUD(12, 5, "YOU");
    putStringHUD(12, 7, "LOSE");
    putStringHUD(10, 11, "SCORE");
    putNumberHUD(10, 13, score, 5);
    putStringHUD(6, 17, "PRESS START");

    hideSprites();
}

// ======================================================
//                        LEVEL SETUP
// ======================================================

// Initializes level 1
static void initLevel1(void) {
    int i;

    // Reinitialize Mode 0 layers
    initMode0();

    // BG0 is HUD, BG1 is gameplay layer
    setupBackground(0, BG_PRIORITY(0) | BG_CHARBLOCK(HUD_CHARBLOCK) | BG_SCREENBLOCK(HUD_SCREENBLOCK) | BG_4BPP | BG_SIZE_SMALL);
    setupBackground(1, BG_PRIORITY(1) | BG_CHARBLOCK(LEVEL_CHARBLOCK) | BG_SCREENBLOCK(LEVEL_SCREENBLOCK) | BG_4BPP | BG_SIZE_SMALL);

    // Reset scrolling
    level1HOff = 0;
    level1VOff = 0;
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, level1HOff, level1VOff);

    // Build tilemap and collision map for level 1
    buildLevel1MapAndCollision();

    // Reset player lives for a fresh level start
    player.lives = 3;
    player.invincibleTimer = 0;

    // Place the player for the current level
    resetPlayerForCurrentLevel();

    // Reset level state
    enemiesRemaining = MAX_ENEMIES;
    doorVisible = 0;

    // Reset bullets
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        playerBullets[i].active = 0;
        playerBullets[i].width = 8;
        playerBullets[i].height = 8;
    }

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        enemyBullets[i].active = 0;
        enemyBullets[i].width = 8;
        enemyBullets[i].height = 8;
    }

    // Initialize enemies
    enemies[0].x = 80;   enemies[0].y = 56;   enemies[0].minX = 56;  enemies[0].maxX = 120;
    enemies[1].x = 150;  enemies[1].y = 40;   enemies[1].minX = 128; enemies[1].maxX = 200;
    enemies[2].x = 60;   enemies[2].y = 88;   enemies[2].minX = 32;  enemies[2].maxX = 96;
    enemies[3].x = 178;  enemies[3].y = 72;   enemies[3].minX = 150; enemies[3].maxX = 216;

    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].oldX = enemies[i].x;
        enemies[i].oldY = enemies[i].y;
        enemies[i].xVel = 1;
        enemies[i].yVel = 0;
        enemies[i].width = 16;
        enemies[i].height = 16;
        enemies[i].direction = DIR_RIGHT;
        enemies[i].active = 1;
        enemies[i].shootTimer = 60 + i * 20;
        enemies[i].animCounter = 0;
        enemies[i].currentFrame = 0;
        enemies[i].phase = ENEMY_FLOATING;
    }

    // Initialize balloons for level 1
    for (i = 0; i < MAX_BALLOONS; i++) {
        balloons[i].active = 0;
        balloons[i].width = 8;
        balloons[i].height = 16;
    }

    // At least two balloons in this level
    balloons[0].x = 36;
    balloons[0].y = 76;
    balloons[0].active = 1;

    balloons[1].x = 186;
    balloons[1].y = 52;
    balloons[1].active = 1;

    // No stars in level 1
    for (i = 0; i < MAX_STARS; i++) {
        stars[i].active = 0;
    }
}

// Initializes level 2
static void initLevel2(void) {
    int i;

    // Reinitialize Mode 0
    initMode0();

    // BG0 remains the HUD
    // BG1 becomes a wide scrolling map for level 2
    setupBackground(0, BG_PRIORITY(0) | BG_CHARBLOCK(HUD_CHARBLOCK) | BG_SCREENBLOCK(HUD_SCREENBLOCK) | BG_4BPP | BG_SIZE_SMALL);
    setupBackground(1, BG_PRIORITY(1) | BG_CHARBLOCK(LEVEL_CHARBLOCK) | BG_SCREENBLOCK(LEVEL_SCREENBLOCK) | BG_4BPP | BG_SIZE_WIDE);

    // Start at the left of the scrolling level
    level2HOff = 0;
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);

    // Build the level 2 tilemap
    buildLevel2Map();

    // Reset bullets and enemies
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        playerBullets[i].active = 0;
        playerBullets[i].width = 8;
        playerBullets[i].height = 8;
    }

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        enemyBullets[i].active = 0;
    }

    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }

    // Initialize balloons for level 2
    for (i = 0; i < MAX_BALLOONS; i++) {
        balloons[i].active = 0;
        balloons[i].width = 8;
        balloons[i].height = 16;
    }

    // Place 10 balloons across the level
    balloons[0].x = 50;   balloons[0].y = 40;   balloons[0].active = 1;
    balloons[1].x = 90;   balloons[1].y = 80;   balloons[1].active = 1;
    balloons[2].x = 140;  balloons[2].y = 34;   balloons[2].active = 1;
    balloons[3].x = 188;  balloons[3].y = 100;  balloons[3].active = 1;
    balloons[4].x = 238;  balloons[4].y = 52;   balloons[4].active = 1;
    balloons[5].x = 286;  balloons[5].y = 26;   balloons[5].active = 1;
    balloons[6].x = 334;  balloons[6].y = 92;   balloons[6].active = 1;
    balloons[7].x = 382;  balloons[7].y = 44;   balloons[7].active = 1;
    balloons[8].x = 430;  balloons[8].y = 64;   balloons[8].active = 1;
    balloons[9].x = 474;  balloons[9].y = 36;   balloons[9].active = 1;

    level2BalloonsRemaining = 10;

    // Initialize vertically moving stars
    for (i = 0; i < MAX_STARS; i++) {
        stars[i].x = 70 + i * 55;
        stars[i].y = 28 + (i % 3) * 30;
        stars[i].oldY = stars[i].y;
        stars[i].width = 16;
        stars[i].height = 16;
        stars[i].yVel = (i % 2 == 0) ? 1 : -1;
        stars[i].minY = 18;
        stars[i].maxY = 126;
        stars[i].active = 1;
    }

    // Reset player with 3 lives for the new level
    resetPlayerForCurrentLevel();

    // Start the player near the beginning of the long level
    player.x = 16;
    player.y = 70;
}

// Resets the player position and movement state for the current level.
// Do not reset lives here.
static void resetPlayerForCurrentLevel(void) {
    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (state == STATE_LEVEL1) {
        player.x = 8;
        player.y = findLowestGroundYAtX(player.x, player.width, player.height);

        // Safety fallback
        if (player.y < 0) {
            player.y = LEVEL1_PIXEL_H - 24 - player.height;
        }
    } else {
        player.x = PLAYER_START_X;
        player.y = PLAYER_START_Y;
    }

    player.oldX = player.x;
    player.oldY = player.y;
    player.xVel = 0;
    player.yVel = 0;
    player.direction = DIR_RIGHT;
    player.currentFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
    player.grounded = 0;
    player.invincibleTimer = 0;
    player.onLadder = 0;
}

// Loads the exported tileset, tilemap, and prepares level 1 collision usage
static void buildLevel1MapAndCollision(void) {
    // Clear out old level map data first
    clearScreenblock(LEVEL_SCREENBLOCK);

    // Load the background palette for the level tileset
    loadBgPalette(tilesetPal, 16);

    // Load the 4bpp tile graphics into the selected background charblock
    loadTilesToCharblock(LEVEL_CHARBLOCK, tilesetTiles, 16384);

    // Load the level 1 tilemap exported from Tiled into the level screenblock
    loadMapToScreenblock(LEVEL_SCREENBLOCK, leveloneMap, leveloneLen / 2);
}

// Builds the star level background with wide scrolling
static void buildLevel2Map(void) {
    int x, y;

    // Clear both screenblocks used by the wide map
    clearScreenblock(24);
    clearScreenblock(25);

    // Fill the whole 64x32 map using two screenblocks
    for (y = 0; y < LEVEL2_MAP_H; y++) {
        for (x = 0; x < LEVEL2_MAP_W; x++) {
            int entry = TILE_SKY;

            // Scatter star background tiles
            if (((x + y) % 7) == 0) {
                entry = TILE_STARBG;
            }

            // Add clouds every so often
            if ((y == 6 || y == 12) && (x % 9 == 0 || x % 13 == 0)) {
                entry = TILE_CLOUD;
            }

            // 64x32 uses two 32x32 screenblocks horizontally
            if (x < 32) {
                SCREENBLOCK[24].tilemap[y * 32 + x] = entry;
            } else {
                SCREENBLOCK[25].tilemap[y * 32 + (x - 32)] = entry;
            }
        }
    }
}

// ======================================================
//                      STATE UPDATES
// ======================================================

// Start screen input
static void updateStart(void) {
    // Start the game on START press
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Set state first so level 1 spawn logic works correctly.
        state = STATE_LEVEL1;
        initLevel1();
        menuNeedsRedraw = 0;
    }
}

// Level 1 logic
static void updateLevel1(void) {
    // Pause on START
    if (BUTTON_PRESSED(BUTTON_START)) {
        state = STATE_PAUSE;
        menuNeedsRedraw = 1;
        return;
    }

    // Update invincibility timer
    if (player.invincibleTimer > 0) {
        player.invincibleTimer--;
    }

    // Update the player and gameplay objects
    updatePlayerLevel1();
    updatePlayerBullets();
    updateEnemyBullets();
    updateEnemies();
    updateBalloonsLevel1();
    updatePlayerAnimation();

    // Center the level 1 camera on the player while keeping the camera inside
    // the 256x256 world bounds. HUD stays fixed on BG0.
    level1HOff = CLAMP(player.x + player.width / 2 - SCREENWIDTH / 2, 0, LEVEL1_PIXEL_W - SCREENWIDTH);
    level1VOff = CLAMP(player.y + player.height / 2 - SCREENHEIGHT / 2, 0, LEVEL1_PIXEL_H - SCREENHEIGHT);
    setBackgroundOffset(1, level1HOff, level1VOff);

    // If all enemies are dead, reveal the door
    if (enemiesRemaining <= 0) {
        doorVisible = 1;
    }

    // Enter the door to move to level 2
    if (doorVisible && collision(player.x, player.y, player.width, player.height, doorX, doorY, 16, 16)) {
        initLevel2();
        state = STATE_LEVEL2;
        return;
    }

    // Lose if out of lives
    if (player.lives <= 0) {
        state = STATE_LOSE;
        menuNeedsRedraw = 1;
    }
}

// Level 2 logic
static void updateLevel2(void) {
    // Pause on START
    if (BUTTON_PRESSED(BUTTON_START)) {
        state = STATE_PAUSE;
        menuNeedsRedraw = 1;
        return;
    }

    // Update invincibility timer
    if (player.invincibleTimer > 0) {
        player.invincibleTimer--;
    }

    // Update level 2 entities
    updatePlayerLevel2();
    updatePlayerBullets();
    updateStars();
    updateBalloonsLevel2();
    updatePlayerAnimation();

    // Update scrolling based on player position
    level2HOff = CLAMP(player.x - 120, 0, LEVEL2_PIXEL_W - SCREENWIDTH);
    setBackgroundOffset(1, level2HOff, 0);

    // Win when all 10 balloons have been collected
    if (level2BalloonsRemaining <= 0) {
        state = STATE_WIN;
        menuNeedsRedraw = 1;
        return;
    }

    // Lose if out of lives
    if (player.lives <= 0) {
        state = STATE_LOSE;
        menuNeedsRedraw = 1;
    }
}

// Pause screen logic
static void updatePause(void) {
    // Resume on START
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Return to the previous gameplay state
        // Since pause can happen in both levels, infer from scrolling / door / balloons remaining
        if (level2BalloonsRemaining > 0 && (stars[0].active || level2HOff > 0 || player.x > 240)) {
            state = STATE_LEVEL2;
        } else {
            state = STATE_LEVEL1;
        }

        menuNeedsRedraw = 0;
        return;
    }

    // Quit to title on SELECT
    if (BUTTON_PRESSED(BUTTON_SELECT)) {
        score = 0;
        state = STATE_START;
        menuNeedsRedraw = 1;
    }
}

// Win screen logic
static void updateWin(void) {
    // Restart on START
    if (BUTTON_PRESSED(BUTTON_START)) {
        score = 0;
        state = STATE_START;
        menuNeedsRedraw = 1;
    }
}

// Lose screen logic
static void updateLose(void) {
    // Restart on START
    if (BUTTON_PRESSED(BUTTON_START)) {
        score = 0;
        state = STATE_START;
        menuNeedsRedraw = 1;
    }
}

// ======================================================
//                     PLAYER UPDATES
// ======================================================

// Level 1 player controls and physics
// Level 1 player controls and physics using the exported collision bitmap
static void updatePlayerLevel1(void) {
    int nextX;
    int nextY;
    int floatStrength;
    int climbing = 0;

    // Save old position
    player.oldX = player.x;
    player.oldY = player.y;

    // Default movement state
    player.isMoving = 0;

    // Check whether the player is currently overlapping ladder pixels
    player.onLadder = rectTouchesColor(player.x, player.y, player.width, player.height, CM_LADDER);

    // Horizontal movement
    // Move left with a small step-up assist for bridge/platform edges.
    if (BUTTON_HELD(BUTTON_LEFT)) {
        int i;
        for (i = 0; i < 2; i++) {
            nextX = player.x - 1;

            if (canMoveTo(nextX, player.y, player.width, player.height)) {
                player.x = nextX;
            } else if (canMoveTo(nextX, player.y - 1, player.width, player.height)) {
                // Allow the player to step up by 1 pixel onto edges like bridges.
                player.x = nextX;
                player.y -= 1;
            } else {
                break;
            }
        }

        player.direction = DIR_LEFT;
        player.isMoving = 1;
    }

    // Move right with a small step-up assist for bridge/platform edges.
    if (BUTTON_HELD(BUTTON_RIGHT)) {
        int i;
        for (i = 0; i < 2; i++) {
            nextX = player.x + 1;

            if (canMoveTo(nextX, player.y, player.width, player.height)) {
                player.x = nextX;
            } else if (canMoveTo(nextX, player.y - 1, player.width, player.height)) {
                // Allow the player to step up by 1 pixel onto edges like bridges.
                player.x = nextX;
                player.y -= 1;
            } else {
                break;
            }
        }

        player.direction = DIR_RIGHT;
        player.isMoving = 1;
    }

    // Ladder movement
    if (player.onLadder) {
        if (BUTTON_HELD(BUTTON_UP)) {
            nextY = player.y - 2;
            if (canMoveTo(player.x, nextY, player.width, player.height)) {
                player.y = nextY;
            }
            player.yVel = 0;
            climbing = 1;
            player.isMoving = 1;
        } else if (BUTTON_HELD(BUTTON_DOWN)) {
            nextY = player.y + 2;
            if (canMoveTo(player.x, nextY, player.width, player.height)) {
                player.y = nextY;
            }
            player.yVel = 0;
            climbing = 1;
            player.isMoving = 1;
        }
    }

    // Hold A to float upward.
    // This gives a soft balloon-like jump that still feels affected by gravity.
    if (BUTTON_HELD(BUTTON_A) && !climbing) {
        player.yVel -= 2;

        // Cap upward speed so it does not become too strong.
        if (player.yVel < -6) {
            player.yVel = -6;
        }
    }

    // Apply gravity unless actively climbing.
    if (!climbing) {
        player.yVel += 1;

        // Cap downward speed so falling looks natural and controllable.
        if (player.yVel > 4) {
            player.yVel = 4;
        }
    }

    // Shoot bullets with B
    if (BUTTON_PRESSED(BUTTON_B)) {
        firePlayerBullet();
    }

    // Apply gravity unless actively climbing
    if (!climbing) {
        player.yVel += 1;
        if (player.yVel > 3) {
            player.yVel = 3;
        }
    }

    // Move vertically one pixel at a time for smoother collision handling.
    if (player.yVel != 0) {
        int step = (player.yVel > 0) ? 1 : -1;
        int amount = absInt(player.yVel);
        int i;

        for (i = 0; i < amount; i++) {
            nextY = player.y + step;

            if (canMoveTo(player.x, nextY, player.width, player.height)) {
                player.y = nextY;
                player.grounded = 0;
            } else {
                // If falling, we landed on the ground/platform.
                if (step > 0) {
                    player.grounded = 1;
                }

                // Stop movement when we hit something above or below.
                player.yVel = 0;
                break;
            }
        }
    }

    // Keep the player inside the full level 1 world, not just the 240x160
    // visible camera window.
    player.x = CLAMP(player.x, 0, LEVEL1_PIXEL_W - player.width);
    player.y = CLAMP(player.y, 0, LEVEL1_PIXEL_H - player.height);

    // Grounded check
    if (isSolidPixel(player.x + 2, player.y + player.height) ||
        isSolidPixel(player.x + player.width - 3, player.y + player.height)) {
        player.grounded = 1;
    } else {
        player.grounded = 0;
    }

    // Red collision = instant damage / death zone
    if (rectTouchesColor(player.x, player.y, player.width, player.height, CM_KILL)) {
        damagePlayer();
        return;
    }

    // Green collision reserved for later
    // This makes it easy to add a win object later
    if (rectTouchesColor(player.x, player.y, player.width, player.height, CM_GOAL)) {
        // TODO: hook this up to your future win/transition condition
    }
}

// Level 2 player controls and physics
static void updatePlayerLevel2(void) {
    // Save old position
    player.oldX = player.x;
    player.oldY = player.y;

    // Default movement state
    player.isMoving = 0;

    // Free movement in the traversal level
    if (BUTTON_HELD(BUTTON_LEFT)) {
        player.x -= 2;
        player.direction = DIR_LEFT;
        player.isMoving = 1;
    }

    if (BUTTON_HELD(BUTTON_RIGHT)) {
        player.x += 2;
        player.direction = DIR_RIGHT;
        player.isMoving = 1;
    }

    // Upward control with A and/or UP
    if (BUTTON_HELD(BUTTON_A) || BUTTON_HELD(BUTTON_UP)) {
        player.y -= 2;
        player.isMoving = 1;
    }

    // Downward control
    if (BUTTON_HELD(BUTTON_DOWN)) {
        player.y += 2;
        player.isMoving = 1;
    }

    // Shooting is still allowed
    if (BUTTON_PRESSED(BUTTON_B)) {
        firePlayerBullet();
    }

    // Keep player inside level 2 bounds
    player.x = CLAMP(player.x, 0, LEVEL2_PIXEL_W - player.width);
    player.y = CLAMP(player.y, 16, SCREENHEIGHT - player.height);
}

// Updates player animation
static void updatePlayerAnimation(void) {
    // Slow down the animation rate
    player.animCounter++;

    if (player.animCounter >= 12) {
        player.animCounter = 0;

        // Only animate while moving
        if (player.isMoving) {
            player.currentFrame ^= 1;
        } else {
            player.currentFrame = 0;
        }
    }
}

// Fires a player bullet
static void firePlayerBullet(void) {
    int i;

    // Find an inactive bullet in the object pool
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (!playerBullets[i].active) {
            playerBullets[i].active = 1;
            playerBullets[i].x = player.x + player.width / 2 - 4;
            playerBullets[i].y = player.y + 6;
            playerBullets[i].oldX = playerBullets[i].x;
            playerBullets[i].oldY = playerBullets[i].y;
            playerBullets[i].xVel = (player.direction == DIR_RIGHT) ? 4 : -4;
            playerBullets[i].yVel = 0;
            playerBullets[i].width = 8;
            playerBullets[i].height = 8;
            break;
        }
    }
}

// ======================================================
//                    BULLET / ENEMY UPDATES
// ======================================================

// Updates player bullets
static void updatePlayerBullets(void) {
    int i, j;

    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (playerBullets[i].active) {
            playerBullets[i].oldX = playerBullets[i].x;
            playerBullets[i].oldY = playerBullets[i].y;
            playerBullets[i].x += playerBullets[i].xVel;

            // Remove bullets that leave the level
            if (state == STATE_LEVEL1) {
                if (playerBullets[i].x < -8 || playerBullets[i].x > SCREENWIDTH) {
                    playerBullets[i].active = 0;
                }
            } else {
                if (playerBullets[i].x < -8 || playerBullets[i].x > LEVEL2_PIXEL_W) {
                    playerBullets[i].active = 0;
                }
            }

            // Only level 1 enemies react to bullets
            if (state == STATE_LEVEL1 && playerBullets[i].active) {
                for (j = 0; j < MAX_ENEMIES; j++) {
                    if (enemies[j].active &&
                        collision(playerBullets[i].x, playerBullets[i].y, playerBullets[i].width, playerBullets[i].height,
                                  enemies[j].x, enemies[j].y, enemies[j].width, enemies[j].height)) {

                        // Bullets pop balloons in phase 1 only
                        if (enemies[j].phase == ENEMY_FLOATING) {
                            int groundY;

                            enemies[j].phase = ENEMY_WALKING;
                            enemies[j].yVel = 1;

                            // Immediately determine if ground exists below
                            groundY = findGroundYBelow(enemies[j].x, enemies[j].y, enemies[j].width, enemies[j].height);

                            // If there is no ground, the enemy dies right away
                            if (groundY < 0) {
                                enemies[j].phase = ENEMY_DEAD;
                                enemies[j].active = 0;
                                enemiesRemaining--;
                            }
                        }

                        // Remove the bullet on impact
                        playerBullets[i].active = 0;
                        break;
                    }
                }
            }
        }
    }
}

// Updates enemy bullets
static void updateEnemyBullets(void) {
    int i;

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].active) {
            enemyBullets[i].oldX = enemyBullets[i].x;
            enemyBullets[i].oldY = enemyBullets[i].y;
            enemyBullets[i].x += enemyBullets[i].xVel;
            enemyBullets[i].y += enemyBullets[i].yVel;

            // Remove offscreen bullets
            if (enemyBullets[i].x < -8 || enemyBullets[i].x > SCREENWIDTH ||
                enemyBullets[i].y < -8 || enemyBullets[i].y > SCREENHEIGHT) {
                enemyBullets[i].active = 0;
            }

            // Enemy bullets can hurt the player in level 1
            if (enemyBullets[i].active &&
                collision(enemyBullets[i].x, enemyBullets[i].y, enemyBullets[i].width, enemyBullets[i].height,
                          player.x, player.y, player.width, player.height)) {
                enemyBullets[i].active = 0;
                damagePlayer();
            }
        }
    }
}

// Updates level 1 enemies
static void updateEnemies(void) {
    int i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            continue;
        }

        enemies[i].oldX = enemies[i].x;
        enemies[i].oldY = enemies[i].y;
        enemies[i].animCounter++;

        if (enemies[i].animCounter >= 16) {
            enemies[i].animCounter = 0;
            enemies[i].currentFrame ^= 1;
        }

        // ----------------------------------------
        // Phase 1: floating enemy with bullets
        // ----------------------------------------
        if (enemies[i].phase == ENEMY_FLOATING) {
            int nextX = enemies[i].x + enemies[i].xVel;
            int nextY = enemies[i].y;

            // Floating enemies still respect world collision, so they cannot
            // drift through platforms or walls.
            if ((frameCount + i * 7) % 24 < 12) {
                nextY--;
            } else {
                nextY++;
            }

            if (nextX <= enemies[i].minX) {
                nextX = enemies[i].minX;
                enemies[i].xVel = 1;
                enemies[i].direction = DIR_RIGHT;
            }

            if (nextX >= enemies[i].maxX) {
                nextX = enemies[i].maxX;
                enemies[i].xVel = -1;
                enemies[i].direction = DIR_LEFT;
            }

            if (canMoveTo(nextX, enemies[i].y, enemies[i].width, enemies[i].height)) {
                enemies[i].x = nextX;
            } else {
                enemies[i].xVel = -enemies[i].xVel;
                enemies[i].direction = (enemies[i].xVel > 0) ? DIR_RIGHT : DIR_LEFT;
            }

            if (canMoveTo(enemies[i].x, nextY, enemies[i].width, enemies[i].height)) {
                enemies[i].y = nextY;
            }

            // Shooting timer
            enemies[i].shootTimer--;
            if (enemies[i].shootTimer <= 0) {
                spawnEnemyBullet(&enemies[i]);
                enemies[i].shootTimer = 90 + i * 10;
            }

            // Contact damage in phase 1
            if (collision(enemies[i].x, enemies[i].y, enemies[i].width, enemies[i].height,
                        player.x, player.y, player.width, player.height)) {
                damagePlayer();
            }
        }

        // ----------------------------------------
        // Phase 2: walking enemy after balloon pop
        // ----------------------------------------
        else if (enemies[i].phase == ENEMY_WALKING) {
            int grounded;

            // Fall straight down until landing on ground
            grounded = isSolidPixel(enemies[i].x + 2, enemies[i].y + enemies[i].height + 1) ||
                       isSolidPixel(enemies[i].x + enemies[i].width - 3, enemies[i].y + enemies[i].height + 1);

            if (!grounded) {
                enemies[i].y += 2;

                // If enemy falls off the visible combat area, treat it as dead
                if (enemies[i].y > LEVEL1_PIXEL_H) {
                    enemies[i].phase = ENEMY_DEAD;
                    enemies[i].active = 0;
                    enemiesRemaining--;
                }
            } else {
                // Walk left/right on platforms
                int nextX = enemies[i].x + enemies[i].xVel;

                if (canMoveTo(nextX, enemies[i].y, enemies[i].width, enemies[i].height) &&
                    isSolidPixel(nextX + 2, enemies[i].y + enemies[i].height + 1)) {
                    enemies[i].x = nextX;
                } else {
                    enemies[i].xVel = -enemies[i].xVel;
                    enemies[i].direction = (enemies[i].xVel > 0) ? DIR_RIGHT : DIR_LEFT;
                }
            }

            // If player stomps from above, kill the enemy
            if (collision(enemies[i].x, enemies[i].y, enemies[i].width, enemies[i].height,
                          player.x, player.y, player.width, player.height)) {
                if (player.oldY + player.height <= enemies[i].y + 4) {
                    enemies[i].phase = ENEMY_DEAD;
                    enemies[i].active = 0;
                    enemiesRemaining--;
                    score += SCORE_ENEMY_KILL;
                    player.yVel = -3;
                } else {
                    damagePlayer();
                }
            }
        }
    }
}

// Spawns a bullet from a floating enemy
static void spawnEnemyBullet(Enemy* e) {
    int i;

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!enemyBullets[i].active) {
            enemyBullets[i].active = 1;
            enemyBullets[i].x = e->x + 4;
            enemyBullets[i].y = e->y + 6;
            enemyBullets[i].oldX = enemyBullets[i].x;
            enemyBullets[i].oldY = enemyBullets[i].y;
            enemyBullets[i].xVel = (player.x < e->x) ? -2 : 2;
            enemyBullets[i].yVel = 0;
            enemyBullets[i].width = 8;
            enemyBullets[i].height = 8;
            break;
        }
    }
}

// ======================================================
//                  BALLOONS / STARS / DAMAGE
// ======================================================

// Level 1 balloon collection
static void updateBalloonsLevel1(void) {
    int i;

    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active &&
            collision(player.x, player.y, player.width, player.height,
                      balloons[i].x, balloons[i].y, balloons[i].width, balloons[i].height)) {

            balloons[i].active = 0;

            // Full health gives points, otherwise restore one life
            if (player.lives == 3) {
                score += SCORE_BALLOON_BONUS;
            } else {
                player.lives++;
            }
        }
    }
}

// Level 2 balloon collection
static void updateBalloonsLevel2(void) {
    int i;

    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active &&
            collision(player.x, player.y, player.width, player.height,
                      balloons[i].x, balloons[i].y, balloons[i].width, balloons[i].height)) {

            balloons[i].active = 0;
            level2BalloonsRemaining--;

            // Full health gives score, otherwise heal
            if (player.lives == 3) {
                score += SCORE_BALLOON_BONUS;
            } else {
                player.lives++;
            }
        }
    }
}

// Updates stars in level 2
static void updateStars(void) {
    int i;

    for (i = 0; i < MAX_STARS; i++) {
        if (!stars[i].active) {
            continue;
        }

        stars[i].oldY = stars[i].y;
        stars[i].y += stars[i].yVel;

        // Bounce vertically
        if (stars[i].y <= stars[i].minY) {
            stars[i].y = stars[i].minY;
            stars[i].yVel = 1;
        }

        if (stars[i].y >= stars[i].maxY) {
            stars[i].y = stars[i].maxY;
            stars[i].yVel = -1;
        }

        // Stars damage the player
        if (collision(stars[i].x, stars[i].y, stars[i].width, stars[i].height,
                      player.x, player.y, player.width, player.height)) {
            damagePlayer();
        }
    }
}

// Applies damage and respawns the player
static void damagePlayer(void) {
    // Respect invincibility frames
    if (player.invincibleTimer > 0) {
        return;
    }

    // Lose one life
    player.lives--;

    // Start invincibility timer
    player.invincibleTimer = 60;

    // Respawn position depends on the current level
    if (state == STATE_LEVEL1) {
        // Always respawn at the bottom-left safe ground spawn for level 1
        player.x = 8;
        player.y = findLowestGroundYAtX(player.x, player.width, player.height);

        if (player.y < 0) {
            player.y = LEVEL1_PIXEL_H - 24 - player.height;
        }

        player.yVel = 0;
    } else if (state == STATE_LEVEL2) {
        player.x = CLAMP(player.x - 24, 0, LEVEL2_PIXEL_W - player.width);
        player.y = 70;
    }
}

// ======================================================
//                  COLLISION / PHYSICS HELPERS
// ======================================================

// Reads one pixel's collision value from the exported 256x256 collision bitmap
static u8 getCollisionPixel(int x, int y) {
    const u8* collisionData = (const u8*) leveloneBitmap;

    // Treat out-of-bounds as blocked
    if (x < 0 || x >= 256 || y < 0 || y >= 256) {
        return CM_BLOCKED;
    }

    return collisionData[OFFSET(x, y, 256)];
}

static int isSolidPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_BLOCKED;
}

static int isLadderPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_LADDER;
}

static int isHazardPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_KILL;
}

static int isGoalPixel(int x, int y) {
    return getCollisionPixel(x, y) == CM_GOAL;
}

// Checks a rectangle against a specific collision color
static int rectTouchesColor(int x, int y, int width, int height, u8 color) {
    if (getCollisionPixel(x + 2, y + 2) == color) return 1;
    if (getCollisionPixel(x + width - 3, y + 2) == color) return 1;
    if (getCollisionPixel(x + 2, y + height - 3) == color) return 1;
    if (getCollisionPixel(x + width - 3, y + height - 3) == color) return 1;
    if (getCollisionPixel(x + width / 2, y + height / 2) == color) return 1;
    return 0;
}

// Checks whether a rectangle can occupy a position
// Checks whether a rectangle can occupy a position in the collision map.
static int canMoveTo(int x, int y, int width, int height) {
    int left = x + 1;
    int right = x + width - 2;
    int top = y + 1;
    int bottom = y + height - 2;
    int midX = x + width / 2;
    int midY = y + height / 2;

    // Top edge
    if (isSolidPixel(left, top)) return 0;
    if (isSolidPixel(midX, top)) return 0;
    if (isSolidPixel(right, top)) return 0;

    // Bottom edge
    if (isSolidPixel(left, bottom)) return 0;
    if (isSolidPixel(midX, bottom)) return 0;
    if (isSolidPixel(right, bottom)) return 0;

    // Side middles
    if (isSolidPixel(left, midY)) return 0;
    if (isSolidPixel(right, midY)) return 0;

    return 1;
}

static int findGroundYBelow(int x, int y, int width, int height) {
    int scanY;

    for (scanY = y + height; scanY < 256; scanY++) {
        if (isSolidPixel(x + 2, scanY) || isSolidPixel(x + width - 3, scanY)) {
            return scanY - height;
        }
    }

    return -1;
}

// Finds the lowest safe standable ground at a given x position.
// This avoids spawning on an upper platform and also avoids hazard overlap.
static int findLowestGroundYAtX(int x, int width, int height) {
    int scanY;

    for (scanY = LEVEL1_PIXEL_H - 1; scanY >= height; scanY--) {
        int leftSolid = isSolidPixel(x + 2, scanY);
        int rightSolid = isSolidPixel(x + width - 3, scanY);

        int leftAboveOpen = !isSolidPixel(x + 2, scanY - 1);
        int rightAboveOpen = !isSolidPixel(x + width - 3, scanY - 1);

        if ((leftSolid || rightSolid) && leftAboveOpen && rightAboveOpen) {
            int candidateY = scanY - height;

            // Only accept this spawn if the player's body would not overlap
            // a kill area and the rectangle fits in empty space.
            if (canMoveTo(x, candidateY, width, height)
                && !rectTouchesColor(x, candidateY, width, height, CM_KILL)) {
                return candidateY;
            }
        }
    }

    return -1;
}

// ======================================================
//                        DRAWING
// ======================================================

// Draws level 1
static void drawLevel1(void) {
    // Clear the HUD and redraw score/lives every frame
    clearHUD();
    drawHUD();

    // Draw gameplay sprites
    drawSpritesLevel1();
    DMANow(3, shadowOAM, OAM, 128*4);
}

// Draws level 2
static void drawLevel2(void) {
    // Clear the HUD and redraw score/lives every frame
    clearHUD();
    drawHUD();

    // Draw gameplay sprites with horizontal scroll adjustment
    drawSpritesLevel2();
    DMANow(3, shadowOAM, OAM, 128*4);
}

// Draws all level 1 sprites
static void drawSpritesLevel1(void) {
    int i;
    int oamIndex = 0;

    // Draw the player
    drawPlayerSprite(player.x - level1HOff, player.y - level1VOff);
    oamIndex = 1;

    // Draw enemies
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            drawEnemySprite(oamIndex, enemies[i].x - level1HOff, enemies[i].y - level1VOff);
            oamIndex++;
        }
    }

    // Draw player bullets
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (playerBullets[i].active) {
            drawBulletSprite(oamIndex, playerBullets[i].x - level1HOff, playerBullets[i].y - level1VOff, 0);
            oamIndex++;
        }
    }

    // Draw enemy bullets
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].active) {
            drawBulletSprite(oamIndex, enemyBullets[i].x - level1HOff, enemyBullets[i].y - level1VOff, 1);
            oamIndex++;
        }
    }

    // Draw balloons
    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active) {
            drawBalloonSprite(oamIndex, balloons[i].x - level1HOff, balloons[i].y - level1VOff);
            oamIndex++;
        }
    }

    // Draw door if visible
    if (doorVisible) {
        drawDoorSprite(doorX - level1HOff, doorY - level1VOff);
        oamIndex++;
    }

    // Hide any remaining OAM entries
    hideUnusedSpritesFrom(oamIndex);
}

// Draws all level 2 sprites with scroll adjustment
static void drawSpritesLevel2(void) {
    int i;
    int oamIndex = 0;

    // Draw player
    drawPlayerSprite(player.x - level2HOff, player.y);
    oamIndex = 1;

    // Draw player bullets
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (playerBullets[i].active) {
            drawBulletSprite(oamIndex, playerBullets[i].x - level2HOff, playerBullets[i].y, 0);
            oamIndex++;
        }
    }

    // Draw balloons
    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active) {
            drawBalloonSprite(oamIndex, balloons[i].x - level2HOff, balloons[i].y);
            oamIndex++;
        }
    }

    // Draw stars
    for (i = 0; i < MAX_STARS; i++) {
        if (stars[i].active) {
            drawStarSprite(oamIndex, stars[i].x - level2HOff, stars[i].y);
            oamIndex++;
        }
    }

    // Hide the rest
    hideUnusedSpritesFrom(oamIndex);
}

// Draws the player sprite
static void drawPlayerSprite(int screenX, int screenY) {
    int tileBase = player.currentFrame ? OBJ_TILE_PLAYER1 : OBJ_TILE_PLAYER0;

    shadowOAM[0].attr0 = ATTR0_Y(screenY) | ATTR0_REGULAR | ATTR0_4BPP | ATTR0_SQUARE;
    shadowOAM[0].attr1 = ATTR1_X(screenX) | ATTR1_SMALL |
                         ((player.direction == DIR_LEFT) ? ATTR1_HFLIP : 0);
    shadowOAM[0].attr2 = ATTR2_TILEID(tileBase, 0) | ATTR2_PRIORITY(0) | ATTR2_PALROW(0);

    // Flash while invincible
    if (player.invincibleTimer > 0 && ((player.invincibleTimer / 4) % 2 == 0)) {
        shadowOAM[0].attr0 = ATTR0_HIDE;
    }
}

// Draws an enemy sprite
static void drawEnemySprite(int oamIndex, int screenX, int screenY) {
    Enemy* e = &enemies[oamIndex - 1];
    int tileBase;

    if (e->phase == ENEMY_FLOATING) {
        tileBase = e->currentFrame ? OBJ_TILE_ENEMYF1 : OBJ_TILE_ENEMYF0;
    } else {
        tileBase = e->currentFrame ? OBJ_TILE_ENEMYW1 : OBJ_TILE_ENEMYW0;
    }

    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_REGULAR | ATTR0_4BPP | ATTR0_SQUARE;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_SMALL |
        ((e->direction == DIR_LEFT) ? ATTR1_HFLIP : 0);

    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(tileBase, 0) |
        ATTR2_PRIORITY(0) | ATTR2_PALROW(0);
}

// Draws a bullet sprite
static void drawBulletSprite(int oamIndex, int screenX, int screenY, int enemyBullet) {
    int tile = enemyBullet ? OBJ_TILE_EBULLET : OBJ_TILE_BULLET;

    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_REGULAR | ATTR0_4BPP | ATTR0_SQUARE;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_TINY;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(tile, 0) | ATTR2_PRIORITY(0) | ATTR2_PALROW(0);
}

// Draws a balloon collectible
static void drawBalloonSprite(int oamIndex, int screenX, int screenY) {
    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_REGULAR | ATTR0_4BPP | ATTR0_TALL;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_TINY;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(OBJ_TILE_BALLOON, 0) | ATTR2_PRIORITY(0) | ATTR2_PALROW(0);
}

// Draws a star hazard
static void drawStarSprite(int oamIndex, int screenX, int screenY) {
    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_REGULAR | ATTR0_4BPP | ATTR0_SQUARE;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_SMALL;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(OBJ_TILE_STAR, 0) | ATTR2_PRIORITY(0) | ATTR2_PALROW(0);
}

// Draws the level 1 exit door
static void drawDoorSprite(int screenX, int screenY) {
    int oamIndex = 127;

    shadowOAM[oamIndex].attr0 = ATTR0_Y(screenY) | ATTR0_REGULAR | ATTR0_4BPP | ATTR0_SQUARE;
    shadowOAM[oamIndex].attr1 = ATTR1_X(screenX) | ATTR1_SMALL;
    shadowOAM[oamIndex].attr2 = ATTR2_TILEID(OBJ_TILE_DOOR, 0) | ATTR2_PRIORITY(0) | ATTR2_PALROW(0);
}

// Hides all sprite slots from a given index upward
static void hideUnusedSpritesFrom(int startIndex) {
    int i;

    for (i = startIndex; i < 128; i++) {
        shadowOAM[i].attr0 = ATTR0_HIDE;
        shadowOAM[i].attr1 = 0;
        shadowOAM[i].attr2 = 0;
    }
}

// ======================================================
//                         UTILITIES
// ======================================================

// Small absolute value helper
static int absInt(int x) {
    return (x < 0) ? -x : x;
}