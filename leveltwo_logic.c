#include "game.h"
#include "leveltwo_logic.h"

void initLevel2(void) {
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

void buildLevel2Map(void) {
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

void updateLevel2(void) {
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

void updatePlayerLevel2(void) {
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

void updateBalloonsLevel2(void) {
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

void updateStars(void) {
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

void drawLevel2(void) {
    drawHUD();
    drawSpritesLevel2();
}

void drawSpritesLevel2(void) {
    int oamIndex = 0;
    int i;

    // Player
    drawPlayerSprite(player.x - level2HOff, player.y);
    oamIndex = 1;

    // Player bullets
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (playerBullets[i].active) {
            drawBulletSprite(oamIndex, playerBullets[i].x - level2HOff, playerBullets[i].y, 0);
            oamIndex++;
        }
    }

    // Balloons
    for (i = 0; i < MAX_BALLOONS; i++) {
        if (balloons[i].active) {
            drawBalloonSprite(oamIndex, balloons[i].x - level2HOff, balloons[i].y);
            oamIndex++;
        }
    }

    // Stars
    for (i = 0; i < MAX_STARS; i++) {
        if (stars[i].active) {
            drawStarSprite(oamIndex, stars[i].x - level2HOff, stars[i].y);
            oamIndex++;
        }
    }

    // Hide all remaining sprites
    hideUnusedSpritesFrom(oamIndex);

    // Copy shadow OAM to hardware OAM
    DMANow(3, shadowOAM, OAM, 512);
}