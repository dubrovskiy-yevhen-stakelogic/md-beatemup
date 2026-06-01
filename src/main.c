#include <genesis.h>
#include "resources.h"

#define PLAYER_FRAME_W 48
#define PLAYER_FRAME_H 96

#define PLAYER_FOOT_OFFSET_X 24
#define PLAYER_FOOT_OFFSET_Y 87

#define ARENA_MIN_X 32
#define ARENA_MAX_X 288
#define ARENA_MIN_Y 136
#define ARENA_MAX_Y 196

#define PLAYER_SPEED_X 1
#define PLAYER_SPEED_Y 1

#define IDLE_FIRST_FRAME 0
#define IDLE_FRAME_COUNT 3

#define WALK_FIRST_FRAME 3
#define WALK_FRAME_COUNT 6

#define IDLE_FRAME_DELAY 14
#define WALK_FRAME_DELAY 7

typedef enum
{
    PLAYER_STATE_IDLE = 0,
    PLAYER_STATE_WALK
} PlayerState;

typedef enum
{
    FACING_LEFT = 0,
    FACING_RIGHT
} FacingDirection;

typedef struct
{
    s16 x;
    s16 y;

    s16 velocityX;
    s16 velocityY;

    PlayerState state;
    FacingDirection facing;

    u16 animTimer;
    u16 animFrame;

    Sprite* sprite;
} Player;

static Player player;
static u16 previousJoy;

static const char* getPlayerStateText(PlayerState state)
{
    switch (state)
    {
        case PLAYER_STATE_IDLE:
            return "IDLE";
        case PLAYER_STATE_WALK:
            return "WALK";
        default:
            return "UNKNOWN";
    }
}

static const char* getFacingText(FacingDirection facing)
{
    switch (facing)
    {
        case FACING_LEFT:
            return "LEFT ";
        case FACING_RIGHT:
            return "RIGHT";
        default:
            return "UNKNOWN";
    }
}

static void drawNumber(s16 value, u16 x, u16 y)
{
    char text[8];

    intToStr(value, text, 1);
    VDP_drawText("     ", x, y);
    VDP_drawText(text, x, y);
}

static void clearTextLine(u16 y)
{
    VDP_drawText("                                        ", 0, y);
}

static void drawArena()
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    VDP_drawText("========================================", 0, 0);
    VDP_drawText("=        MD BEATEMUP - BUILD 028       =", 0, 1);
    VDP_drawText("=       USER RAW SPRITE TEST           =", 0, 2);
    VDP_drawText("========================================", 0, 3);

    VDP_drawText("D-PAD: MOVE", 2, 5);
    VDP_drawText("NO ATTACK YET", 17, 5);

    VDP_drawText("USING YOUR PNG AS-IS", 8, 6);
    VDP_drawText("9 FRAMES: 0-2 IDLE, 3-8 WALK", 4, 7);

    VDP_drawText("+--------------------------------------+", 0, 10);
    for (u16 y = 11; y <= 22; y++)
    {
        VDP_drawText("|                                      |", 0, y);
    }
    VDP_drawText("+--------------------------------------+", 0, 23);
}

static void setPlayerFrame(u16 frame)
{
    if (player.animFrame == frame)
    {
        return;
    }

    player.animFrame = frame;
    SPR_setFrame(player.sprite, frame);
}

static void syncPlayerSprite()
{
    SPR_setPosition(
        player.sprite,
        player.x - PLAYER_FOOT_OFFSET_X,
        player.y - PLAYER_FOOT_OFFSET_Y
    );

    SPR_setHFlip(player.sprite, player.facing == FACING_LEFT);
}

static void clampPlayerToArena()
{
    if (player.x < ARENA_MIN_X)
    {
        player.x = ARENA_MIN_X;
    }

    if (player.x > ARENA_MAX_X)
    {
        player.x = ARENA_MAX_X;
    }

    if (player.y < ARENA_MIN_Y)
    {
        player.y = ARENA_MIN_Y;
    }

    if (player.y > ARENA_MAX_Y)
    {
        player.y = ARENA_MAX_Y;
    }
}

static void initPlayer()
{
    player.x = 160;
    player.y = 184;

    player.velocityX = 0;
    player.velocityY = 0;

    player.state = PLAYER_STATE_IDLE;
    player.facing = FACING_RIGHT;

    player.animTimer = 0;
    player.animFrame = IDLE_FIRST_FRAME;

    player.sprite = SPR_addSprite(
        &player_sprite,
        player.x - PLAYER_FOOT_OFFSET_X,
        player.y - PLAYER_FOOT_OFFSET_Y,
        TILE_ATTR(PAL1, TRUE, FALSE, FALSE)
    );

    /*
        One-row sprite sheet:
        frames 0-2 = idle
        frames 3-8 = walk

        We do NOT switch animation rows.
        We only manually set frame indexes inside animation 0.
    */
    SPR_setAutoAnimation(player.sprite, FALSE);
    SPR_setAnim(player.sprite, 0);
    SPR_setFrame(player.sprite, IDLE_FIRST_FRAME);

    syncPlayerSprite();
}

static void updatePlayerInput()
{
    u16 joy = JOY_readJoypad(JOY_1);

    player.velocityX = 0;
    player.velocityY = 0;

    if (joy & BUTTON_LEFT)
    {
        player.velocityX = -PLAYER_SPEED_X;
        player.facing = FACING_LEFT;
    }
    else if (joy & BUTTON_RIGHT)
    {
        player.velocityX = PLAYER_SPEED_X;
        player.facing = FACING_RIGHT;
    }

    if (joy & BUTTON_UP)
    {
        player.velocityY = -PLAYER_SPEED_Y;
    }
    else if (joy & BUTTON_DOWN)
    {
        player.velocityY = PLAYER_SPEED_Y;
    }

    if ((player.velocityX != 0) || (player.velocityY != 0))
    {
        if (player.state != PLAYER_STATE_WALK)
        {
            player.state = PLAYER_STATE_WALK;
            player.animTimer = 0;
            setPlayerFrame(WALK_FIRST_FRAME);
        }
    }
    else
    {
        if (player.state != PLAYER_STATE_IDLE)
        {
            player.state = PLAYER_STATE_IDLE;
            player.animTimer = 0;
            setPlayerFrame(IDLE_FIRST_FRAME);
        }
    }

    previousJoy = joy;
}

static void updatePlayerAnimation()
{
    u16 firstFrame;
    u16 frameCount;
    u16 delay;
    u16 relativeFrame;

    player.animTimer++;

    if (player.state == PLAYER_STATE_WALK)
    {
        firstFrame = WALK_FIRST_FRAME;
        frameCount = WALK_FRAME_COUNT;
        delay = WALK_FRAME_DELAY;
    }
    else
    {
        firstFrame = IDLE_FIRST_FRAME;
        frameCount = IDLE_FRAME_COUNT;
        delay = IDLE_FRAME_DELAY;
    }

    if (player.animTimer >= delay)
    {
        player.animTimer = 0;

        relativeFrame = player.animFrame - firstFrame;
        relativeFrame = (relativeFrame + 1) % frameCount;

        setPlayerFrame(firstFrame + relativeFrame);
    }
}

static void updatePlayer()
{
    updatePlayerInput();

    player.x += player.velocityX;
    player.y += player.velocityY;

    clampPlayerToArena();

    updatePlayerAnimation();
    syncPlayerSprite();
}

static void drawDebugHud()
{
    clearTextLine(25);
    clearTextLine(26);
    clearTextLine(27);

    VDP_drawText("STATE:", 0, 25);
    VDP_drawText(getPlayerStateText(player.state), 7, 25);

    VDP_drawText("FACE:", 17, 25);
    VDP_drawText(getFacingText(player.facing), 23, 25);

    VDP_drawText("FRAME:", 31, 25);
    drawNumber(player.animFrame, 37, 25);

    VDP_drawText("TIMER:", 0, 26);
    drawNumber(player.animTimer, 7, 26);

    VDP_drawText("SIZE: 48x96", 15, 26);

    VDP_drawText("BUILD 028 / RAW USER SPRITE", 5, 27);
}

int main(bool hard)
{
    JOY_init();

    VDP_setScreenWidth320();

    SPR_init();

    PAL_setPalette(PAL1, player_sprite.palette->data, DMA);

    previousJoy = 0;

    drawArena();
    initPlayer();
    drawDebugHud();

    while (TRUE)
    {
        updatePlayer();
        drawDebugHud();

        SPR_update();
        SYS_doVBlankProcess();
    }

    return 0;
}
