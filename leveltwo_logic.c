#include "game.h"
#include "leveltwo_logic.h"
#include "leveltwo.h"

#include "tileset.h"

void initLevel2(void) {
    int i;

    initMode0();

    setupBackground(0, BG_PRIORITY(0) | BG_CHARBLOCK(HUD_CHARBLOCK) | BG_SCREENBLOCK(HUD_SCREENBLOCK) | BG_4BPP | BG_SIZE_SMALL);
    setupBackground(1, BG_PRIORITY(1) | BG_CHARBLOCK(LEVEL_CHARBLOCK) | BG_SCREENBLOCK(LEVEL_SCREENBLOCK) | BG_4BPP | BG_SIZE_WIDE);

    loadBgPalette(tilesetPal, tilesetPalLen / 2);
    BG_PALETTE[240] = BLACK;
    BG_PALETTE[241] = WHITE;
    loadTilesToCharblock(LEVEL_CHARBLOCK, tilesetTiles, tilesetTilesLen / 2);

    level2HOff = 0;
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);

    buildLevel2Map();

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

    for (i = 0; i < MAX_BALLOONS; i++) {
        balloons[i].active = 0;
        balloons[i].width = 8;
        balloons[i].height = 16;
        balloons[i].spriteVariant = nextRandomSpriteVariant();
    }

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

    resetPlayerForCurrentLevel();
    player.x = 16;
    player.y = 70;
}

void buildLevel2Map(void) {
    // Copy the artist-made 64x32 level 2 map into the two horizontal screenblocks.
    DMANow(3, (void*) leveltwoMap, SCREENBLOCK[LEVEL_SCREENBLOCK].tilemap, 1024);
    DMANow(3, (void*) (leveltwoMap + 1024), SCREENBLOCK[LEVEL_SCREENBLOCK + 1].tilemap, 1024);
}

void updateLevel2(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        pausedState = STATE_LEVEL2;
        state = STATE_PAUSE;
        menuNeedsRedraw = 1;
        return;
    }

    if (player.invincibleTimer > 0) {
        player.invincibleTimer--;
    }

    updatePlayerLevel2();
    updatePlayerBullets();
    updateStars();
    updateBalloonsLevel2();
    updatePlayerAnimation();

    level2HOff = CLAMP(player.x - 120, 0, LEVEL2_PIXEL_W - SCREENWIDTH);
    setBackgroundOffset(1, level2HOff, 0);

    if (level2BalloonsRemaining <= 0) {
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
    int floorY;
    int floatStrength;

    // Save old position
    player.oldX = player.x;
    player.oldY = player.y;

    // Default movement state
    player.isMoving = 0;

    // Horizontal movement
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

    // Bottom of the screen acts as the ground in level 2
    floorY = SCREENHEIGHT - player.height;

    // Check whether the player is standing on the ground
    if (player.y >= floorY) {
        player.y = floorY;
        player.grounded = 1;
    } else {
        player.grounded = 0;
    }

    // A held = fly upward.
    //
    // Same behavior as level 1:
    // more lives means faster upward flight.
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
        // Normal falling behavior when A is not held.
        if (!player.grounded) {
            player.yVel += PLAYER_GRAVITY;

            if (player.yVel > PLAYER_MAX_FALL_SPEED) {
                player.yVel = PLAYER_MAX_FALL_SPEED;
            }
        }
    }

    // If grounded, do not keep downward falling speed.
    if (player.grounded && player.yVel > 0) {
        player.yVel = 0;
    }

    // No boost timer needed with hold-to-fly behavior.
    player.floatBoostTimer = 0;

    // Apply vertical movement one pixel at a time
    while (player.yVel < 0) {
        if (player.y > 16) {
            player.y--;
            player.yVel++;
        } else {
            player.y = 16;
            player.yVel = 0;
            break;
        }
    }

    while (player.yVel > 0) {
        if (player.y < floorY) {
            player.y++;
            player.yVel--;
        } else {
            player.y = floorY;
            player.yVel = 0;
            break;
        }
    }

    // Recompute grounded after moving
    if (player.y >= floorY) {
        player.y = floorY;
        player.grounded = 1;
        if (player.yVel > 0) {
            player.yVel = 0;
        }
    } else {
        player.grounded = 0;
    }

    // Shooting is still allowed
    if (BUTTON_PRESSED(BUTTON_B)) {
        firePlayerBullet();
    }

    // Keep player inside level 2 bounds
    player.x = CLAMP(player.x, 0, LEVEL2_PIXEL_W - player.width);
    player.y = CLAMP(player.y, 16, floorY);

    // Treat vertical drifting/floating as movement for animation
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
            drawBalloonSprite(oamIndex, balloons[i].x - level2HOff, balloons[i].y, balloons[i].spriteVariant);
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