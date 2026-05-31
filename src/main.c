#include <genesis.h>
#include "resources.h"

#define PLAYER_FOOT_OFFSET_X 32
#define PLAYER_FOOT_OFFSET_Y 76
#define ARENA_MIN_X 40
#define ARENA_MAX_X 280
#define ARENA_MIN_Y 136
#define ARENA_MAX_Y 196
#define PLAYER_SPEED_X 1
#define PLAYER_SPEED_Y 1
#define PLAYER_ATTACK_DURATION_FRAMES 32
#define IDLE_FRAME_COUNT 6
#define WALK_FRAME_COUNT 6
#define ATTACK_FRAME_COUNT 6
#define IDLE_FRAME_DELAY 20
#define WALK_FRAME_DELAY 6

typedef enum { PLAYER_STATE_IDLE = 0, PLAYER_STATE_WALK, PLAYER_STATE_ATTACK } PlayerState;
typedef enum { FACING_LEFT = 0, FACING_RIGHT } FacingDirection;
typedef enum { PLAYER_ANIM_IDLE = 0, PLAYER_ANIM_WALK = 1, PLAYER_ANIM_ATTACK = 2, PLAYER_ANIM_JUMP = 3, PLAYER_ANIM_SPECIAL = 4 } PlayerAnim;

typedef struct {
    s16 x; s16 y; s16 velocityX; s16 velocityY; s16 renderOffsetX;
    PlayerState state; FacingDirection facing; u16 attackTimer; u16 animTimer; u16 animFrame;
    Sprite* sprite; PlayerAnim currentAnim;
} Player;

static Player player; static u16 previousJoy;
static const char* getPlayerStateText(PlayerState state) { switch (state) { case PLAYER_STATE_IDLE: return "IDLE  "; case PLAYER_STATE_WALK: return "WALK  "; case PLAYER_STATE_ATTACK: return "ATTACK"; default: return "UNKNOWN"; } }
static const char* getFacingText(FacingDirection facing) { switch (facing) { case FACING_LEFT: return "LEFT "; case FACING_RIGHT: return "RIGHT"; default: return "UNKNOWN"; } }
static void drawNumber(s16 value, u16 x, u16 y) { char text[8]; intToStr(value, text, 1); VDP_drawText("     ", x, y); VDP_drawText(text, x, y); }
static void clearTextLine(u16 y) { VDP_drawText("                                        ", 0, y); }
static void drawArena() {
    VDP_clearPlane(BG_A, TRUE); VDP_clearPlane(BG_B, TRUE);
    VDP_drawText("========================================", 0, 0);
    VDP_drawText("=        MD BEATEMUP - BUILD 024       =", 0, 1);
    VDP_drawText("=      FULL-CELL CLEAN SHEET FIX       =", 0, 2);
    VDP_drawText("========================================", 0, 3);
    VDP_drawText("D-PAD: MOVE", 2, 5); VDP_drawText("B: PUNCH TEST", 17, 5);
    VDP_drawText("FIXED: NO SIDE/BOTTOM BLEED", 5, 6);
    VDP_drawText("+--------------------------------------+", 0, 10);
    for (u16 y = 11; y <= 22; y++) VDP_drawText("|                                      |", 0, y);
    VDP_drawText("+--------------------------------------+", 0, 23);
}
static void setPlayerAnimation(PlayerAnim anim) { if (player.currentAnim == anim) return; player.currentAnim = anim; player.animTimer = 0; player.animFrame = 0; SPR_setAnim(player.sprite, anim); SPR_setAutoAnimation(player.sprite, FALSE); SPR_setFrame(player.sprite, 0); }
static void setPlayerFrame(u16 frame) { if (player.animFrame == frame) return; player.animFrame = frame; SPR_setFrame(player.sprite, frame); }
static s16 getFacingSign() { return (player.facing == FACING_RIGHT) ? 1 : -1; }
static void syncPlayerSprite() { s16 spriteX = player.x - PLAYER_FOOT_OFFSET_X + (player.renderOffsetX * getFacingSign()); s16 spriteY = player.y - PLAYER_FOOT_OFFSET_Y; SPR_setPosition(player.sprite, spriteX, spriteY); SPR_setHFlip(player.sprite, player.facing == FACING_LEFT); }
static void clampPlayerToArena() { if (player.x < ARENA_MIN_X) player.x = ARENA_MIN_X; if (player.x > ARENA_MAX_X) player.x = ARENA_MAX_X; if (player.y < ARENA_MIN_Y) player.y = ARENA_MIN_Y; if (player.y > ARENA_MAX_Y) player.y = ARENA_MAX_Y; }
static void startPlayerAttack() { player.state = PLAYER_STATE_ATTACK; player.velocityX = 0; player.velocityY = 0; player.attackTimer = 0; player.renderOffsetX = 0; setPlayerAnimation(PLAYER_ANIM_ATTACK); }
static void initPlayer() {
    player.x = 160; player.y = 184; player.velocityX = 0; player.velocityY = 0; player.renderOffsetX = 0; player.state = PLAYER_STATE_IDLE; player.facing = FACING_RIGHT; player.attackTimer = 0; player.animTimer = 0; player.animFrame = 0;
    player.sprite = SPR_addSprite(&player_sprite, player.x - PLAYER_FOOT_OFFSET_X, player.y - PLAYER_FOOT_OFFSET_Y, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    SPR_setAutoAnimation(player.sprite, FALSE); player.currentAnim = PLAYER_ANIM_IDLE; SPR_setAnim(player.sprite, PLAYER_ANIM_IDLE); SPR_setFrame(player.sprite, 0); syncPlayerSprite();
}
static void updatePlayerInput() {
    u16 joy = JOY_readJoypad(JOY_1); bool isBPressedNow = (joy & BUTTON_B) != 0; bool wasBPressedBefore = (previousJoy & BUTTON_B) != 0; player.velocityX = 0; player.velocityY = 0;
    if (player.state == PLAYER_STATE_ATTACK) { previousJoy = joy; return; }
    if (isBPressedNow && !wasBPressedBefore) { startPlayerAttack(); previousJoy = joy; return; }
    if (joy & BUTTON_LEFT) { player.velocityX = -PLAYER_SPEED_X; player.facing = FACING_LEFT; } else if (joy & BUTTON_RIGHT) { player.velocityX = PLAYER_SPEED_X; player.facing = FACING_RIGHT; }
    if (joy & BUTTON_UP) player.velocityY = -PLAYER_SPEED_Y; else if (joy & BUTTON_DOWN) player.velocityY = PLAYER_SPEED_Y;
    if ((player.velocityX != 0) || (player.velocityY != 0)) { player.state = PLAYER_STATE_WALK; player.renderOffsetX = 0; setPlayerAnimation(PLAYER_ANIM_WALK); } else { player.state = PLAYER_STATE_IDLE; player.renderOffsetX = 0; setPlayerAnimation(PLAYER_ANIM_IDLE); }
    previousJoy = joy;
}
static void updatePlayerAttackAnimation() {
    player.attackTimer++;
    // Feet are now uniformly anchored in the recut sheet, so no per-frame X compensation is needed.
    if (player.attackTimer < 5) { setPlayerFrame(0); }
    else if (player.attackTimer < 9) { setPlayerFrame(1); }
    else if (player.attackTimer < 14) { setPlayerFrame(2); }
    else if (player.attackTimer < 18) { setPlayerFrame(3); }
    else if (player.attackTimer < 24) { setPlayerFrame(4); }
    else { setPlayerFrame(5); }
    player.renderOffsetX = 0;
    if (player.attackTimer >= PLAYER_ATTACK_DURATION_FRAMES) { player.attackTimer = 0; player.renderOffsetX = 0; player.state = PLAYER_STATE_IDLE; setPlayerAnimation(PLAYER_ANIM_IDLE); }
}
static void updatePlayerWalkAnimation() { if (player.animTimer >= WALK_FRAME_DELAY) { player.animTimer = 0; setPlayerFrame((player.animFrame + 1) % WALK_FRAME_COUNT); } }
static void updatePlayerIdleAnimation() { if (player.animTimer >= IDLE_FRAME_DELAY) { player.animTimer = 0; setPlayerFrame((player.animFrame + 1) % IDLE_FRAME_COUNT); } }
static void updatePlayerAnimation() { player.animTimer++; if (player.state == PLAYER_STATE_ATTACK) { updatePlayerAttackAnimation(); return; } if (player.state == PLAYER_STATE_WALK) { updatePlayerWalkAnimation(); return; } updatePlayerIdleAnimation(); }
static void updatePlayer() { updatePlayerInput(); player.x += player.velocityX; player.y += player.velocityY; clampPlayerToArena(); updatePlayerAnimation(); syncPlayerSprite(); }
static void drawDebugHud() {
    clearTextLine(25); clearTextLine(26); clearTextLine(27);
    VDP_drawText("STATE:", 0, 25); VDP_drawText(getPlayerStateText(player.state), 7, 25);
    VDP_drawText("FACE:", 17, 25); VDP_drawText(getFacingText(player.facing), 23, 25);
    VDP_drawText("FRAME:", 31, 25); drawNumber(player.animFrame, 37, 25);
    VDP_drawText("TIMER:", 0, 26); drawNumber(player.animTimer, 7, 26);
    VDP_drawText("OFF:", 15, 26); drawNumber(player.renderOffsetX, 20, 26);
    VDP_drawText("ATK:", 25, 26); drawNumber(player.attackTimer, 30, 26);
    VDP_drawText("BUILD 024 / FULLCELL PACK", 4, 27);
}
int main(bool hard) {
    JOY_init(); VDP_setScreenWidth320(); SPR_init(); PAL_setPalette(PAL1, player_sprite.palette->data, DMA);
    previousJoy = 0; drawArena(); initPlayer(); drawDebugHud();
    while (TRUE) { updatePlayer(); drawDebugHud(); SPR_update(); SYS_doVBlankProcess(); }
    return 0;
}
