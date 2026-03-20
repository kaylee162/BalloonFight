#include "game.h"
#include "leveltwo_logic.h"
#include "sfx.h"

#include "tileset.h"
#include "leveltwo.h"
#include "collisionMapTwo.h"

// Level 2 traversal tuning
#define LEVEL2_BALLOON_COUNT 10
#define LEVEL2_SPAWN_X 24

// Per-star horizontal patrol state
static int starXVel[MAX_STARS];
static int starMinX[MAX_STARS];
static int starMaxX[MAX_STARS];

// Private helpers
static void initStarsLevel2(void);
static void placeFixedLevel2Balloons(void);

// Cheat Helper
static void handleLevel2DebugCheats(void);

void initLevel2(void) {
    int i;

    initMode0();

    // Clear out anything left from the level 2 intro / menu screen.
    clearHUD();
    clearScreenblock(HUD_SCREENBLOCK);
    clearScreenblock(LEVEL_SCREENBLOCK);
    clearScreenblock(LEVEL_SCREENBLOCK + 1);
    clearCharBlock(LEVEL_CHARBLOCK);
    clearCharBlock(LEVEL_CHARBLOCK + 1);

    // Hide all sprites immediately so old OAM data does not flash.
    hideSprites();
    DMANow(3, shadowOAM, OAM, 128 * 4);

    // BG0 = HUD
    setupBackground(0,
        BG_PRIORITY(0) |
        BG_CHARBLOCK(HUD_CHARBLOCK) |
        BG_SCREENBLOCK(HUD_SCREENBLOCK) |
        BG_4BPP |
        BG_SIZE_SMALL);

    // BG1 = gameplay map for the wide level 2 world
    setupBackground(1,
        BG_PRIORITY(1) |
        BG_CHARBLOCK(LEVEL_CHARBLOCK) |
        BG_SCREENBLOCK(LEVEL_SCREENBLOCK) |
        BG_4BPP |
        BG_SIZE_WIDE);

    // Reload the gameplay palette and tiles because the intro screen
    // replaced them with the screen/menu background art.
    loadBgPalette(tilesetPal, tilesetPalLen / 2);

    // Keep HUD text colors in palette row 15.
    BG_PALETTE[240] = BLACK;
    BG_PALETTE[241] = WHITE;

    loadTilesToCharblock(LEVEL_CHARBLOCK, tilesetTiles, tilesetTilesLen / 2);

    level2HOff = 0;
    level2VOff = 0;
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);

    // Now that the correct gameplay tiles are back in VRAM, build the real map.
    buildLevel2Map();

    // Clear player bullets
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        playerBullets[i].active = 0;
        playerBullets[i].width = 8;
        playerBullets[i].height = 8;
    }

    // No enemy bullets in level 2
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        enemyBullets[i].active = 0;
        enemyBullets[i].width = 8;
        enemyBullets[i].height = 8;
    }

    // No enemies in level 2
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }

    // Reset balloons
    for (i = 0; i < MAX_BALLOONS; i++) {
        balloons[i].active = 0;
        balloons[i].width = 24;
        balloons[i].height = 24;
        balloons[i].spriteVariant = nextRandomSpriteVariant();
    }

    int activeBalloonCount = 0;

    placeFixedLevel2Balloons();

    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active) {
            activeBalloonCount++;
        }
    }

    level2BalloonsRemaining = activeBalloonCount;

    // Reset stars
    initStarsLevel2();

    // Advancing to level 2 restores the player to 3 lives.
    player.lives = 3;
    player.invincibleTimer = 0;

    // Reset the player for level 2
    resetPlayerForCurrentLevel();

    menuNeedsRedraw = 0;
}

void buildLevel2Map(void) {
    // Copy the artist-made 64x32 level 2 map into the two horizontal screenblocks.
    DMANow(3, (void*) leveltwoMap, SCREENBLOCK[LEVEL_SCREENBLOCK].tilemap, 1024);
    DMANow(3, (void*) (leveltwoMap + 1024), SCREENBLOCK[LEVEL_SCREENBLOCK + 1].tilemap, 1024);
}

static void handleLevel2DebugCheats(void) {
    int i;

    // Helpful extra: SELECT + A = refill lives
    if (BUTTON_HELD(BUTTON_SELECT) && BUTTON_PRESSED(BUTTON_A)) {
        player.lives = 3;
        return;
    }

    // Helpful extra: SELECT + B = instantly collect all remaining balloons
    // Useful for testing the win screen / end-of-level flow.
    if (BUTTON_HELD(BUTTON_SELECT) && BUTTON_PRESSED(BUTTON_B)) {
        for (i = 0; i < MAX_BALLOONS; i++) {
            balloons[i].active = 0;
        }

        level2BalloonsRemaining = 0;
        return;
    }
}

void updateLevel2(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        pausedState = STATE_LEVEL2;
        state = STATE_PAUSE;
        menuNeedsRedraw = 1;
        return;
    }

    handleLevel2DebugCheats();

    if (player.invincibleTimer > 0) {
        player.invincibleTimer--;
    }

    updatePlayerLevel2();
    updatePlayerBullets();
    updateStars();
    updateBalloonsLevel2();
    updatePlayerAnimation();

    level2HOff = CLAMP(player.x - (SCREENWIDTH / 2) + (player.width / 2),
                       0, LEVEL2_PIXEL_W - SCREENWIDTH);

    level2VOff = CLAMP(player.y - (SCREENHEIGHT / 2) + (player.height / 2),
                       0, LEVEL2_PIXEL_H - SCREENHEIGHT);

    setBackgroundOffset(1, level2HOff, level2VOff);

    if (level2BalloonsRemaining <= 0) {
        sfxWin();
        state = STATE_WIN;
        menuNeedsRedraw = 1;
        return;
    }

    if (player.lives <= 0) {
        state = STATE_LOSE;
        menuNeedsRedraw = 1;
    }
}

void updatePlayerLevel2(void) {
    int nextX;
    int floatStrength;

    // Save old position
    player.oldX = player.x;
    player.oldY = player.y;

    // Default animation state
    player.isMoving = 0;

    // Horizontal movement with one-pixel stepping, same idea as level 1
    if (BUTTON_HELD(BUTTON_LEFT)) {
        int i;
        int steppedUp = 0;

        for (i = 0; i < 2; i++) {
            nextX = player.x - 1;

            if (canMoveTo(nextX, player.y, player.width, player.height)) {
                player.x = nextX;
            } else if (player.grounded && !steppedUp &&
                       canMoveTo(nextX, player.y - 1, player.width, player.height)) {
                // Small step-up assist for platform edges
                player.x = nextX;
                player.y--;
                steppedUp = 1;
            } else {
                break;
            }
        }

        player.direction = DIR_LEFT;
        player.isMoving = 1;
    }

    if (BUTTON_HELD(BUTTON_RIGHT)) {
        int i;
        int steppedUp = 0;

        for (i = 0; i < 2; i++) {
            nextX = player.x + 1;

            if (canMoveTo(nextX, player.y, player.width, player.height)) {
                player.x = nextX;
            } else if (player.grounded && !steppedUp &&
                       canMoveTo(nextX, player.y - 1, player.width, player.height)) {
                // Small step-up assist for platform edges
                player.x = nextX;
                player.y--;
                steppedUp = 1;
            } else {
                break;
            }
        }

        player.direction = DIR_RIGHT;
        player.isMoving = 1;
    }

    // Ground/platform check from collision map
    if (!canMoveTo(player.x, player.y + 1, player.width, player.height)) {
        player.grounded = 1;
    } else {
        player.grounded = 0;
    }

    // Hold UP to float upward
    if (BUTTON_HELD(BUTTON_UP) && player.lives > 0) {
        int targetUpVel = getFloatVelocityForLives(player.lives);

        player.grounded = 0;
        player.isMoving = 1;

        if (player.yVel > targetUpVel) {
            player.yVel--;
        }

        if (player.yVel < targetUpVel) {
            player.yVel = targetUpVel;
        }
    } else {
        // Gravity when not floating
        if (!player.grounded) {
            player.yVel += PLAYER_GRAVITY;

            if (player.yVel > PLAYER_MAX_FALL_SPEED) {
                player.yVel = PLAYER_MAX_FALL_SPEED;
            }
        }
    }

    if (player.grounded && player.yVel > 0) {
        player.yVel = 0;
    }

    player.floatBoostTimer = 0;

    // Vertical collision one pixel at a time
    while (player.yVel < 0) {
        if (canMoveTo(player.x, player.y - 1, player.width, player.height)) {
            player.y--;
            player.yVel++;
        } else {
            player.yVel = 0;
            break;
        }
    }

    while (player.yVel > 0) {
        if (canMoveTo(player.x, player.y + 1, player.width, player.height)) {
            player.y++;
            player.yVel--;
        } else {
            player.yVel = 0;
            break;
        }
    }

    // Re-evaluate grounded after movement
    if (!canMoveTo(player.x, player.y + 1, player.width, player.height)) {
        player.grounded = 1;
    } else {
        player.grounded = 0;
    }

    // Clamp to level bounds
    player.x = CLAMP(player.x, 0, LEVEL2_PIXEL_W - player.width);
    player.y = CLAMP(player.y, 0, LEVEL2_PIXEL_H - player.height);

    // Hazard strip / kill zone check
    if (rectTouchesColor(player.x, player.y, player.width, player.height, CM_KILL)) {
        damagePlayer();
    }

    // Shooting still allowed
    if (BUTTON_PRESSED(BUTTON_B)) {
        firePlayerBullet();
    }

    // Count vertical drifting as movement for animation
    floatStrength = absInt(player.y - player.oldY);
    if (floatStrength > 0) {
        player.isMoving = 1;
    }
}

void updateBalloonsLevel2(void) {
    int i;

    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active &&
            collision(player.x, player.y, player.width, player.height,
                      balloons[i].x, balloons[i].y, balloons[i].width, balloons[i].height)) {

            balloons[i].active = 0;
            level2BalloonsRemaining--;

            // If already at full lives, award points instead
            if (player.lives == 3) {
                score += SCORE_BALLOON_BONUS;
            } else {
                player.lives++;
            }

            sfxBalloon();
        }
    }
}

void updateStars(void) {
    int i;

    for (i = 0; i < MAX_STARS; i++) {
        if (!stars[i].active) {
            continue;
        }

        stars[i].oldY = stars[i].y;

        // Move only vertically so stars travel top-to-bottom as required.
        stars[i].y += stars[i].yVel;

        // Vertical bounce
        if (stars[i].y <= stars[i].minY) {
            stars[i].y = stars[i].minY;
            stars[i].yVel = 1;
        }

        if (stars[i].y >= stars[i].maxY) {
            stars[i].y = stars[i].maxY;
            stars[i].yVel = -1;
        }

        // Damage player on contact
        if (collision(stars[i].x, stars[i].y, stars[i].width, stars[i].height,
                      player.x, player.y, player.width, player.height)) {
            if (player.invincibleTimer <= 0) {
                sfxStarHit();
            }
            damagePlayer();
        }
    }
}

static void placeFixedLevel2Balloons(void) {
    int i;

    // Pre-chosen balloon positions for level 2.
    // These avoid the expensive random-search placement that caused lag.
    static const int balloonX[LEVEL2_BALLOON_COUNT] = {
        36, 84, 136, 188, 244, 296, 344, 392, 444, 484
    };

    static const int balloonY[LEVEL2_BALLOON_COUNT] = {
        48, 104, 64, 156, 92, 52, 116, 72, 148, 180
    };

    for (i = 0; i < LEVEL2_BALLOON_COUNT; i++) {
        balloons[i].width = 24;
        balloons[i].height = 24;
        balloons[i].spriteVariant = nextRandomSpriteVariant();

        // These are fixed designer-chosen positions for level 2.
        // Always load them so the required balloon count matches the win condition.
        balloons[i].x = balloonX[i];
        balloons[i].y = balloonY[i];
        balloons[i].active = 1;
    }

    for (i = LEVEL2_BALLOON_COUNT; i < MAX_BALLOONS; i++) {
        balloons[i].active = 0;
        balloons[i].width = 24;
        balloons[i].height = 24;
    }
}

static void initStarsLevel2(void) {
    int i;

    // Spread stars along the traversal route.
    // Each one gets its own vertical patrol range.
    static const int baseX[MAX_STARS]  = { 76, 132, 210, 286, 330, 388, 452, 486 };
    static const int baseY[MAX_STARS]  = { 56, 114, 70, 172, 84, 60, 126, 186 };
    static const int rangeY[MAX_STARS] = { 14, 18, 12, 16, 22, 18, 20, 14 };
    
    for (i = 0; i < MAX_STARS; i++) {
        stars[i].x = baseX[i];
        stars[i].y = baseY[i];
        stars[i].oldY = stars[i].y;

        // Built as a simple 8x8 sprite tile
        stars[i].width = 8;
        stars[i].height = 8;

        stars[i].yVel = (i & 1) ? -1 : 1;
        stars[i].minY = baseY[i] - rangeY[i];
        stars[i].maxY = baseY[i] + rangeY[i];
        stars[i].active = 1;

        starXVel[i] = 0;
        starMinX[i] = baseX[i];
        starMaxX[i] = baseX[i];
    }
}

void drawLevel2(void) {
    drawHUD();
    drawSpritesLevel2();
}

void drawSpritesLevel2(void) {
    int oamIndex = 0;
    int i;

    // Player
    drawPlayerSprite(player.x - level2HOff, player.y - level2VOff);
    oamIndex = 1;

    // Player bullets
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (playerBullets[i].active) {
            drawBulletSprite(oamIndex, playerBullets[i].x - level2HOff,
                             playerBullets[i].y - level2VOff, 0);
            oamIndex++;
        }
    }

    // Balloons
    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active) {
            drawBalloonSprite(oamIndex, balloons[i].x - level2HOff,
                              balloons[i].y - level2VOff, balloons[i].spriteVariant);
            oamIndex++;
        }
    }

    // Stars
    for (i = 0; i < MAX_STARS; i++) {
        if (stars[i].active) {
            int screenX = stars[i].x - level2HOff;
            int screenY = stars[i].y - level2VOff;

            if (screenX > -8 && screenX < SCREENWIDTH &&
                screenY > -8 && screenY < SCREENHEIGHT) {
                drawStarSprite(oamIndex, screenX, screenY);
                oamIndex++;
            }
        }
    }

    // Hide all remaining sprites
    hideUnusedSpritesFrom(oamIndex);
}