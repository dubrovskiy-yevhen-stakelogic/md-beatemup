#include <genesis.h>

#define TEXT_CELL_SIZE 8

#define ARENA_MIN_X 16
#define ARENA_MAX_X 288

#define ARENA_MIN_Y 88
#define ARENA_MAX_Y 168

#define PLAYER_SPEED_X 2
#define PLAYER_SPEED_Y 1
#define PLAYER_START_HP 5
#define PLAYER_HIT_STUN_FRAMES 24
#define PLAYER_KNOCKBACK_SPEED 2

#define PLAYER_MAX_COMBO_STEP 3
#define PLAYER_ATTACK_BUFFER_START_FRAME 10

#define PLAYER_BODY_W 16
#define PLAYER_BODY_H 16

#define PLAYER_ATTACK_HITBOX_H 14
#define PLAYER_ATTACK_HITBOX_OFFSET_Y 0

#define MAX_ENEMIES 3

#define ENEMY_BODY_W 16
#define ENEMY_BODY_H 16
#define ENEMY_START_HP 3
#define ENEMY_SPEED_X 1
#define ENEMY_SPEED_Y 1
#define ENEMY_HIT_STUN_FRAMES 20
#define ENEMY_KNOCKBACK_SPEED 2

#define ENEMY_ATTACK_RANGE_X 24
#define ENEMY_ATTACK_RANGE_Y 10
#define ENEMY_ATTACK_DURATION_FRAMES 34
#define ENEMY_ATTACK_ACTIVE_START_FRAME 14
#define ENEMY_ATTACK_ACTIVE_END_FRAME 20
#define ENEMY_ATTACK_COOLDOWN_FRAMES 46

#define ENEMY_ATTACK_HITBOX_W 20
#define ENEMY_ATTACK_HITBOX_H 14
#define ENEMY_ATTACK_HITBOX_OFFSET_X 14
#define ENEMY_ATTACK_HITBOX_OFFSET_Y 0

typedef enum
{
    GAME_STATE_PLAYING = 0,
    GAME_STATE_WIN,
    GAME_STATE_LOSE
} GameState;

typedef enum
{
    PLAYER_STATE_IDLE = 0,
    PLAYER_STATE_WALK,
    PLAYER_STATE_ATTACK,
    PLAYER_STATE_HIT,
    PLAYER_STATE_DEAD
} PlayerState;

typedef enum
{
    FACING_LEFT = 0,
    FACING_RIGHT
} FacingDirection;

typedef enum
{
    ENEMY_STATE_IDLE = 0,
    ENEMY_STATE_WALK,
    ENEMY_STATE_ATTACK,
    ENEMY_STATE_HIT,
    ENEMY_STATE_DEAD
} EnemyState;

typedef struct
{
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} Rect;

typedef struct
{
    s16 x;
    s16 y;

    s16 prevX;
    s16 prevY;

    s16 tileX;
    s16 tileY;

    s16 prevTileX;
    s16 prevTileY;

    s16 velocityX;
    s16 velocityY;

    s16 hp;

    PlayerState state;
    FacingDirection facing;

    u16 hitStunTimer;
    u16 animTimer;
    u16 animFrame;

    u16 attackTimer;
    u16 comboStep;
    bool queuedNextAttack;
} Player;

typedef struct
{
    s16 x;
    s16 y;

    s16 prevX;
    s16 prevY;

    s16 tileX;
    s16 tileY;

    s16 prevTileX;
    s16 prevTileY;

    s16 velocityX;
    s16 velocityY;

    s16 hp;

    EnemyState state;
    FacingDirection facing;

    FacingDirection knockbackDirection;
    s16 knockbackSpeed;

    u16 hitStunTimer;
    u16 attackTimer;
    u16 attackCooldownTimer;
    bool hasHitPlayerThisAttack;

    u16 animTimer;
    u16 animFrame;
} Enemy;

typedef struct
{
    bool visible;
    s16 tileX;
    s16 tileY;
} DebugHitbox;

static GameState gameState;

static Player player;
static Enemy enemies[MAX_ENEMIES];

static bool playerHitEnemyThisAttack[MAX_ENEMIES];

static DebugHitbox playerAttackHitbox;
static DebugHitbox prevPlayerAttackHitbox;

static DebugHitbox enemyAttackHitboxes[MAX_ENEMIES];
static DebugHitbox prevEnemyAttackHitboxes[MAX_ENEMIES];

static u16 previousJoy;
static u16 playerHitCounter;
static u16 enemyHitCounter;
static u16 enemiesDefeated;
static u16 hitPauseTimer;

static const char* getPlayerStateText(PlayerState state)
{
    switch (state)
    {
        case PLAYER_STATE_IDLE:
            return "IDLE  ";
        case PLAYER_STATE_WALK:
            return "WALK  ";
        case PLAYER_STATE_ATTACK:
            return "ATTACK";
        case PLAYER_STATE_HIT:
            return "HIT   ";
        case PLAYER_STATE_DEAD:
            return "DEAD  ";
        default:
            return "UNKNOWN";
    }
}

static const char* getEnemyStateText(EnemyState state)
{
    switch (state)
    {
        case ENEMY_STATE_IDLE:
            return "IDLE ";
        case ENEMY_STATE_WALK:
            return "WALK ";
        case ENEMY_STATE_ATTACK:
            return "ATK  ";
        case ENEMY_STATE_HIT:
            return "HIT  ";
        case ENEMY_STATE_DEAD:
            return "DEAD ";
        default:
            return "UNKN ";
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

static s16 absS16(s16 value)
{
    return value < 0 ? -value : value;
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

static bool rectsOverlap(Rect a, Rect b)
{
    return a.x < (b.x + b.w)
        && (a.x + a.w) > b.x
        && a.y < (b.y + b.h)
        && (a.y + a.h) > b.y;
}

static u16 getPlayerAttackDuration()
{
    switch (player.comboStep)
    {
        case 1:
            return 20;
        case 2:
            return 22;
        case 3:
            return 30;
        default:
            return 20;
    }
}

static u16 getPlayerAttackActiveStart()
{
    switch (player.comboStep)
    {
        case 1:
            return 6;
        case 2:
            return 7;
        case 3:
            return 10;
        default:
            return 6;
    }
}

static u16 getPlayerAttackActiveEnd()
{
    switch (player.comboStep)
    {
        case 1:
            return 11;
        case 2:
            return 14;
        case 3:
            return 20;
        default:
            return 11;
    }
}

static s16 getPlayerAttackHitboxWidth()
{
    switch (player.comboStep)
    {
        case 1:
            return 24;
        case 2:
            return 28;
        case 3:
            return 36;
        default:
            return 24;
    }
}

static s16 getPlayerAttackHitboxOffsetX()
{
    switch (player.comboStep)
    {
        case 1:
            return 18;
        case 2:
            return 20;
        case 3:
            return 24;
        default:
            return 18;
    }
}

static s16 getPlayerAttackDamage()
{
    switch (player.comboStep)
    {
        case 3:
            return 2;
        default:
            return 1;
    }
}

static s16 getPlayerAttackKnockbackSpeed()
{
    switch (player.comboStep)
    {
        case 1:
            return 2;
        case 2:
            return 3;
        case 3:
            return 5;
        default:
            return 2;
    }
}

static u16 getPlayerAttackHitPause()
{
    switch (player.comboStep)
    {
        case 3:
            return 6;
        default:
            return 3;
    }
}

static const char* getPlayerAttackHitboxText()
{
    switch (player.comboStep)
    {
        case 1:
            return "1#";
        case 2:
            return "2#";
        case 3:
            return "3#";
        default:
            return "##";
    }
}

static bool isPlayerAttacking()
{
    return player.state == PLAYER_STATE_ATTACK;
}

static bool isPlayerAttackActive()
{
    return isPlayerAttacking()
        && player.attackTimer >= getPlayerAttackActiveStart()
        && player.attackTimer <= getPlayerAttackActiveEnd();
}

static bool isEnemyAttacking(Enemy* enemy)
{
    return enemy->state == ENEMY_STATE_ATTACK;
}

static bool isEnemyAttackActive(Enemy* enemy)
{
    return isEnemyAttacking(enemy)
        && enemy->attackTimer >= ENEMY_ATTACK_ACTIVE_START_FRAME
        && enemy->attackTimer <= ENEMY_ATTACK_ACTIVE_END_FRAME;
}

static Rect getPlayerHurtbox()
{
    Rect rect;

    rect.x = player.x;
    rect.y = player.y;
    rect.w = PLAYER_BODY_W;
    rect.h = PLAYER_BODY_H;

    return rect;
}

static Rect getEnemyHurtbox(Enemy* enemy)
{
    Rect rect;

    rect.x = enemy->x;
    rect.y = enemy->y;
    rect.w = ENEMY_BODY_W;
    rect.h = ENEMY_BODY_H;

    return rect;
}

static Rect getPlayerAttackRect()
{
    Rect rect;
    s16 attackWidth = getPlayerAttackHitboxWidth();
    s16 attackOffsetX = getPlayerAttackHitboxOffsetX();

    rect.y = player.y + PLAYER_ATTACK_HITBOX_OFFSET_Y;
    rect.w = attackWidth;
    rect.h = PLAYER_ATTACK_HITBOX_H;

    if (player.facing == FACING_RIGHT)
    {
        rect.x = player.x + attackOffsetX;
    }
    else
    {
        rect.x = player.x - attackOffsetX - attackWidth + PLAYER_BODY_W;
    }

    return rect;
}

static Rect getEnemyAttackRect(Enemy* enemy)
{
    Rect rect;

    rect.y = enemy->y + ENEMY_ATTACK_HITBOX_OFFSET_Y;
    rect.w = ENEMY_ATTACK_HITBOX_W;
    rect.h = ENEMY_ATTACK_HITBOX_H;

    if (enemy->facing == FACING_RIGHT)
    {
        rect.x = enemy->x + ENEMY_ATTACK_HITBOX_OFFSET_X;
    }
    else
    {
        rect.x = enemy->x - ENEMY_ATTACK_HITBOX_OFFSET_X - ENEMY_ATTACK_HITBOX_W + ENEMY_BODY_W;
    }

    return rect;
}

static void drawArena()
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    VDP_drawText("========================================", 0, 0);
    VDP_drawText("=        MD BEATEMUP - BUILD 009       =", 0, 1);
    VDP_drawText("=       STAGE 6: 3 HIT COMBO           =", 0, 2);
    VDP_drawText("========================================", 0, 3);

    VDP_drawText("D-PAD: MOVE", 2, 5);
    VDP_drawText("B: PUNCH / TAP FOR 1-2-3 COMBO", 2, 6);
    VDP_drawText("START: RESTART AFTER WIN/LOSE", 2, 7);

    VDP_drawText("+--------------------------------------+", 0, 10);
    VDP_drawText("|                                      |", 0, 11);
    VDP_drawText("|                                      |", 0, 12);
    VDP_drawText("|                                      |", 0, 13);
    VDP_drawText("|                                      |", 0, 14);
    VDP_drawText("|                                      |", 0, 15);
    VDP_drawText("|                                      |", 0, 16);
    VDP_drawText("|                                      |", 0, 17);
    VDP_drawText("|                                      |", 0, 18);
    VDP_drawText("|                                      |", 0, 19);
    VDP_drawText("|                                      |", 0, 20);
    VDP_drawText("+--------------------------------------+", 0, 21);

    VDP_drawText("1#/2#/3# = COMBO HITBOX, 3RD IS HEAVY", 1, 23);
}

static void resetPlayerHitFlags()
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        playerHitEnemyThisAttack[i] = FALSE;
    }
}

static void initPlayer()
{
    player.x = 90;
    player.y = 136;

    player.prevX = player.x;
    player.prevY = player.y;

    player.tileX = player.x / TEXT_CELL_SIZE;
    player.tileY = player.y / TEXT_CELL_SIZE;

    player.prevTileX = player.tileX;
    player.prevTileY = player.tileY;

    player.velocityX = 0;
    player.velocityY = 0;

    player.hp = PLAYER_START_HP;

    player.state = PLAYER_STATE_IDLE;
    player.facing = FACING_RIGHT;

    player.hitStunTimer = 0;
    player.animTimer = 0;
    player.animFrame = 0;

    player.attackTimer = 0;
    player.comboStep = 0;
    player.queuedNextAttack = FALSE;

    resetPlayerHitFlags();
}

static void initEnemy(u16 index, s16 x, s16 y)
{
    Enemy* enemy = &enemies[index];

    enemy->x = x;
    enemy->y = y;

    enemy->prevX = enemy->x;
    enemy->prevY = enemy->y;

    enemy->tileX = enemy->x / TEXT_CELL_SIZE;
    enemy->tileY = enemy->y / TEXT_CELL_SIZE;

    enemy->prevTileX = enemy->tileX;
    enemy->prevTileY = enemy->tileY;

    enemy->velocityX = 0;
    enemy->velocityY = 0;

    enemy->hp = ENEMY_START_HP;

    enemy->state = ENEMY_STATE_IDLE;
    enemy->facing = FACING_LEFT;
    enemy->knockbackDirection = FACING_RIGHT;
    enemy->knockbackSpeed = ENEMY_KNOCKBACK_SPEED;

    enemy->hitStunTimer = 0;
    enemy->attackTimer = 0;
    enemy->attackCooldownTimer = 20 + (index * 18);
    enemy->hasHitPlayerThisAttack = FALSE;

    enemy->animTimer = 0;
    enemy->animFrame = 0;
}

static void initEnemies()
{
    initEnemy(0, 210, 128);
    initEnemy(1, 240, 152);
    initEnemy(2, 270, 112);
}

static void initHitboxes()
{
    u16 i;

    playerAttackHitbox.visible = FALSE;
    playerAttackHitbox.tileX = 0;
    playerAttackHitbox.tileY = 0;

    prevPlayerAttackHitbox.visible = FALSE;
    prevPlayerAttackHitbox.tileX = 0;
    prevPlayerAttackHitbox.tileY = 0;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        enemyAttackHitboxes[i].visible = FALSE;
        enemyAttackHitboxes[i].tileX = 0;
        enemyAttackHitboxes[i].tileY = 0;

        prevEnemyAttackHitboxes[i].visible = FALSE;
        prevEnemyAttackHitboxes[i].tileX = 0;
        prevEnemyAttackHitboxes[i].tileY = 0;
    }
}

static void resetGame()
{
    previousJoy = 0;
    playerHitCounter = 0;
    enemyHitCounter = 0;
    enemiesDefeated = 0;
    hitPauseTimer = 0;

    gameState = GAME_STATE_PLAYING;

    drawArena();
    initPlayer();
    initEnemies();
    initHitboxes();
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

static void clampEnemyToArena(Enemy* enemy)
{
    if (enemy->x < ARENA_MIN_X)
    {
        enemy->x = ARENA_MIN_X;
    }

    if (enemy->x > ARENA_MAX_X)
    {
        enemy->x = ARENA_MAX_X;
    }

    if (enemy->y < ARENA_MIN_Y)
    {
        enemy->y = ARENA_MIN_Y;
    }

    if (enemy->y > ARENA_MAX_Y)
    {
        enemy->y = ARENA_MAX_Y;
    }
}

static void startPlayerAttack(u16 comboStep)
{
    player.state = PLAYER_STATE_ATTACK;
    player.velocityX = 0;
    player.velocityY = 0;
    player.attackTimer = 0;
    player.comboStep = comboStep;
    player.queuedNextAttack = FALSE;
    player.animTimer = 0;
    player.animFrame = 0;

    resetPlayerHitFlags();
}

static void startEnemyAttack(Enemy* enemy)
{
    enemy->state = ENEMY_STATE_ATTACK;
    enemy->velocityX = 0;
    enemy->velocityY = 0;
    enemy->attackTimer = 0;
    enemy->animTimer = 0;
    enemy->animFrame = 0;
    enemy->hasHitPlayerThisAttack = FALSE;
}

static void updatePlayerInput()
{
    u16 joy = JOY_readJoypad(JOY_1);
    bool isBPressedNow = (joy & BUTTON_B) != 0;
    bool wasBPressedBefore = (previousJoy & BUTTON_B) != 0;

    player.velocityX = 0;
    player.velocityY = 0;

    if (player.state == PLAYER_STATE_DEAD || player.state == PLAYER_STATE_HIT)
    {
        previousJoy = joy;
        return;
    }

    if (isPlayerAttacking())
    {
        if (isBPressedNow
            && !wasBPressedBefore
            && player.attackTimer >= PLAYER_ATTACK_BUFFER_START_FRAME
            && player.comboStep < PLAYER_MAX_COMBO_STEP)
        {
            player.queuedNextAttack = TRUE;
        }

        previousJoy = joy;
        return;
    }

    if (isBPressedNow && !wasBPressedBefore)
    {
        startPlayerAttack(1);
        previousJoy = joy;
        return;
    }

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
        player.state = PLAYER_STATE_WALK;
        player.comboStep = 0;
    }
    else
    {
        player.state = PLAYER_STATE_IDLE;
    }

    previousJoy = joy;
}

static void finishPlayerAttack()
{
    if (player.queuedNextAttack && player.comboStep < PLAYER_MAX_COMBO_STEP)
    {
        startPlayerAttack(player.comboStep + 1);
        return;
    }

    player.state = PLAYER_STATE_IDLE;
    player.attackTimer = 0;
    player.comboStep = 0;
    player.queuedNextAttack = FALSE;
    player.animTimer = 0;
    player.animFrame = 0;
    resetPlayerHitFlags();
}

static void updatePlayerAnimation()
{
    player.animTimer++;

    if (player.state == PLAYER_STATE_DEAD)
    {
        player.velocityX = 0;
        player.velocityY = 0;
        return;
    }

    if (player.state == PLAYER_STATE_HIT)
    {
        if (player.hitStunTimer > 0)
        {
            player.hitStunTimer--;
        }

        if (player.hitStunTimer == 0)
        {
            player.state = PLAYER_STATE_IDLE;
            player.velocityX = 0;
            player.velocityY = 0;
            player.comboStep = 0;
            player.queuedNextAttack = FALSE;
        }

        return;
    }

    if (player.state == PLAYER_STATE_ATTACK)
    {
        player.attackTimer++;

        if (player.comboStep == 3)
        {
            if (player.attackTimer < 10)
            {
                player.animFrame = 0;
            }
            else if (player.attackTimer < 21)
            {
                player.animFrame = 1;
            }
            else
            {
                player.animFrame = 2;
            }
        }
        else
        {
            if (player.attackTimer < 6)
            {
                player.animFrame = 0;
            }
            else if (player.attackTimer < 15)
            {
                player.animFrame = 1;
            }
            else
            {
                player.animFrame = 2;
            }
        }

        if (player.attackTimer >= getPlayerAttackDuration())
        {
            finishPlayerAttack();
        }

        return;
    }

    if (player.state == PLAYER_STATE_IDLE)
    {
        if (player.animTimer >= 30)
        {
            player.animTimer = 0;
            player.animFrame++;

            if (player.animFrame > 1)
            {
                player.animFrame = 0;
            }
        }
    }
    else if (player.state == PLAYER_STATE_WALK)
    {
        if (player.animTimer >= 8)
        {
            player.animTimer = 0;
            player.animFrame++;

            if (player.animFrame > 1)
            {
                player.animFrame = 0;
            }
        }
    }
}

static void updatePlayerAttackHitbox()
{
    Rect attackRect;

    prevPlayerAttackHitbox = playerAttackHitbox;

    playerAttackHitbox.visible = FALSE;
    playerAttackHitbox.tileX = 0;
    playerAttackHitbox.tileY = 0;

    if (!isPlayerAttackActive())
    {
        return;
    }

    attackRect = getPlayerAttackRect();

    playerAttackHitbox.visible = TRUE;
    playerAttackHitbox.tileX = attackRect.x / TEXT_CELL_SIZE;
    playerAttackHitbox.tileY = player.tileY;

    if (playerAttackHitbox.tileX < 1)
    {
        playerAttackHitbox.tileX = 1;
    }

    if (playerAttackHitbox.tileX > 36)
    {
        playerAttackHitbox.tileX = 36;
    }
}

static void updateEnemyAttackHitbox(u16 index)
{
    Rect attackRect;
    Enemy* enemy = &enemies[index];

    prevEnemyAttackHitboxes[index] = enemyAttackHitboxes[index];

    enemyAttackHitboxes[index].visible = FALSE;
    enemyAttackHitboxes[index].tileX = 0;
    enemyAttackHitboxes[index].tileY = 0;

    if (!isEnemyAttackActive(enemy))
    {
        return;
    }

    attackRect = getEnemyAttackRect(enemy);

    enemyAttackHitboxes[index].visible = TRUE;
    enemyAttackHitboxes[index].tileX = attackRect.x / TEXT_CELL_SIZE;
    enemyAttackHitboxes[index].tileY = enemy->tileY;

    if (enemyAttackHitboxes[index].tileX < 1)
    {
        enemyAttackHitboxes[index].tileX = 1;
    }

    if (enemyAttackHitboxes[index].tileX > 36)
    {
        enemyAttackHitboxes[index].tileX = 36;
    }
}

static void damageEnemy(u16 index)
{
    Enemy* enemy = &enemies[index];
    s16 damage = getPlayerAttackDamage();

    if (enemy->state == ENEMY_STATE_DEAD)
    {
        return;
    }

    enemy->hp -= damage;
    enemyHitCounter++;
    hitPauseTimer = getPlayerAttackHitPause();

    if (enemy->hp <= 0)
    {
        enemy->hp = 0;
        enemy->state = ENEMY_STATE_DEAD;
        enemy->velocityX = 0;
        enemy->velocityY = 0;
        enemy->hitStunTimer = 0;
        enemiesDefeated++;

        if (enemiesDefeated >= MAX_ENEMIES)
        {
            gameState = GAME_STATE_WIN;
        }

        return;
    }

    enemy->state = ENEMY_STATE_HIT;
    enemy->velocityX = 0;
    enemy->velocityY = 0;
    enemy->knockbackDirection = player.facing;
    enemy->knockbackSpeed = getPlayerAttackKnockbackSpeed();
    enemy->hitStunTimer = ENEMY_HIT_STUN_FRAMES + (player.comboStep == 3 ? 8 : 0);
    enemy->attackTimer = 0;
    enemy->attackCooldownTimer = ENEMY_ATTACK_COOLDOWN_FRAMES;
    enemy->hasHitPlayerThisAttack = FALSE;
    enemy->animTimer = 0;
    enemy->animFrame = 0;
}

static void damagePlayer(Enemy* enemy)
{
    if (player.state == PLAYER_STATE_DEAD || gameState != GAME_STATE_PLAYING)
    {
        return;
    }

    player.hp--;
    playerHitCounter++;
    hitPauseTimer = 4;

    if (player.hp <= 0)
    {
        player.hp = 0;
        player.state = PLAYER_STATE_DEAD;
        player.velocityX = 0;
        player.velocityY = 0;
        player.hitStunTimer = 0;
        gameState = GAME_STATE_LOSE;
        return;
    }

    player.state = PLAYER_STATE_HIT;
    player.velocityX = 0;
    player.velocityY = 0;
    player.hitStunTimer = PLAYER_HIT_STUN_FRAMES;
    player.attackTimer = 0;
    player.comboStep = 0;
    player.queuedNextAttack = FALSE;
    player.animTimer = 0;
    player.animFrame = 0;
    resetPlayerHitFlags();

    if (enemy->facing == FACING_RIGHT)
    {
        player.x += PLAYER_KNOCKBACK_SPEED;
    }
    else
    {
        player.x -= PLAYER_KNOCKBACK_SPEED;
    }

    clampPlayerToArena();
}

static void checkPlayerAttackVsEnemies()
{
    Rect attackRect;
    Rect enemyHurtbox;
    u16 i;

    if (!isPlayerAttackActive())
    {
        return;
    }

    attackRect = getPlayerAttackRect();

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].state == ENEMY_STATE_DEAD)
        {
            continue;
        }

        if (playerHitEnemyThisAttack[i])
        {
            continue;
        }

        enemyHurtbox = getEnemyHurtbox(&enemies[i]);

        if (rectsOverlap(attackRect, enemyHurtbox))
        {
            playerHitEnemyThisAttack[i] = TRUE;
            damageEnemy(i);
        }
    }
}

static void checkEnemyAttackVsPlayer(u16 index)
{
    Rect attackRect;
    Rect playerHurtbox;
    Enemy* enemy = &enemies[index];

    if (!isEnemyAttackActive(enemy))
    {
        return;
    }

    if (enemy->hasHitPlayerThisAttack)
    {
        return;
    }

    if (player.state == PLAYER_STATE_DEAD || player.state == PLAYER_STATE_HIT)
    {
        return;
    }

    attackRect = getEnemyAttackRect(enemy);
    playerHurtbox = getPlayerHurtbox();

    if (rectsOverlap(attackRect, playerHurtbox))
    {
        enemy->hasHitPlayerThisAttack = TRUE;
        damagePlayer(enemy);
    }
}

static void updatePlayer()
{
    player.prevX = player.x;
    player.prevY = player.y;

    player.prevTileX = player.tileX;
    player.prevTileY = player.tileY;

    updatePlayerInput();

    player.x += player.velocityX;
    player.y += player.velocityY;

    clampPlayerToArena();

    player.tileX = player.x / TEXT_CELL_SIZE;
    player.tileY = player.y / TEXT_CELL_SIZE;

    updatePlayerAnimation();

    clampPlayerToArena();

    player.tileX = player.x / TEXT_CELL_SIZE;
    player.tileY = player.y / TEXT_CELL_SIZE;

    updatePlayerAttackHitbox();
}

static bool isEnemyCloseEnoughToAttack(Enemy* enemy)
{
    return absS16(player.x - enemy->x) <= ENEMY_ATTACK_RANGE_X
        && absS16(player.y - enemy->y) <= ENEMY_ATTACK_RANGE_Y;
}

static void updateEnemyFacing(Enemy* enemy)
{
    if (enemy->state == ENEMY_STATE_DEAD)
    {
        return;
    }

    if (player.x < enemy->x)
    {
        enemy->facing = FACING_LEFT;
    }
    else
    {
        enemy->facing = FACING_RIGHT;
    }
}

static void updateEnemyAI(Enemy* enemy)
{
    enemy->velocityX = 0;
    enemy->velocityY = 0;

    if (enemy->state == ENEMY_STATE_DEAD || enemy->state == ENEMY_STATE_HIT || enemy->state == ENEMY_STATE_ATTACK)
    {
        return;
    }

    if (player.state == PLAYER_STATE_DEAD || gameState != GAME_STATE_PLAYING)
    {
        enemy->state = ENEMY_STATE_IDLE;
        return;
    }

    if (enemy->attackCooldownTimer > 0)
    {
        enemy->attackCooldownTimer--;
    }

    if (isEnemyCloseEnoughToAttack(enemy) && enemy->attackCooldownTimer == 0)
    {
        startEnemyAttack(enemy);
        return;
    }

    if (absS16(player.x - enemy->x) > ENEMY_ATTACK_RANGE_X - 4)
    {
        if (player.x < enemy->x)
        {
            enemy->velocityX = -ENEMY_SPEED_X;
        }
        else
        {
            enemy->velocityX = ENEMY_SPEED_X;
        }
    }

    if (absS16(player.y - enemy->y) > 2)
    {
        if (player.y < enemy->y)
        {
            enemy->velocityY = -ENEMY_SPEED_Y;
        }
        else
        {
            enemy->velocityY = ENEMY_SPEED_Y;
        }
    }

    if ((enemy->velocityX != 0) || (enemy->velocityY != 0))
    {
        enemy->state = ENEMY_STATE_WALK;
    }
    else
    {
        enemy->state = ENEMY_STATE_IDLE;
    }
}

static void updateEnemyAnimationAndState(Enemy* enemy)
{
    enemy->animTimer++;

    if (enemy->state == ENEMY_STATE_DEAD)
    {
        return;
    }

    if (enemy->state == ENEMY_STATE_HIT)
    {
        if (enemy->knockbackDirection == FACING_RIGHT)
        {
            enemy->x += enemy->knockbackSpeed;
        }
        else
        {
            enemy->x -= enemy->knockbackSpeed;
        }

        if (enemy->hitStunTimer > 0)
        {
            enemy->hitStunTimer--;
        }

        if (enemy->hitStunTimer == 0)
        {
            enemy->state = ENEMY_STATE_IDLE;
            enemy->knockbackSpeed = ENEMY_KNOCKBACK_SPEED;
        }

        return;
    }

    if (enemy->state == ENEMY_STATE_ATTACK)
    {
        enemy->attackTimer++;

        if (enemy->attackTimer < 12)
        {
            enemy->animFrame = 0;
        }
        else if (enemy->attackTimer < 22)
        {
            enemy->animFrame = 1;
        }
        else
        {
            enemy->animFrame = 2;
        }

        if (enemy->attackTimer >= ENEMY_ATTACK_DURATION_FRAMES)
        {
            enemy->state = ENEMY_STATE_IDLE;
            enemy->attackTimer = 0;
            enemy->attackCooldownTimer = ENEMY_ATTACK_COOLDOWN_FRAMES;
            enemy->hasHitPlayerThisAttack = FALSE;
            enemy->animFrame = 0;
            enemy->animTimer = 0;
        }

        return;
    }

    if (enemy->animTimer >= 12)
    {
        enemy->animTimer = 0;
        enemy->animFrame++;

        if (enemy->animFrame > 1)
        {
            enemy->animFrame = 0;
        }
    }
}

static void updateEnemy(u16 index)
{
    Enemy* enemy = &enemies[index];

    enemy->prevX = enemy->x;
    enemy->prevY = enemy->y;

    enemy->prevTileX = enemy->tileX;
    enemy->prevTileY = enemy->tileY;

    updateEnemyFacing(enemy);
    updateEnemyAI(enemy);

    enemy->x += enemy->velocityX;
    enemy->y += enemy->velocityY;

    clampEnemyToArena(enemy);

    enemy->tileX = enemy->x / TEXT_CELL_SIZE;
    enemy->tileY = enemy->y / TEXT_CELL_SIZE;

    updateEnemyAnimationAndState(enemy);

    clampEnemyToArena(enemy);

    enemy->tileX = enemy->x / TEXT_CELL_SIZE;
    enemy->tileY = enemy->y / TEXT_CELL_SIZE;

    updateEnemyAttackHitbox(index);
}

static void updateEnemies()
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        updateEnemy(i);
    }
}

static void checkEnemiesAttackVsPlayer()
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        checkEnemyAttackVsPlayer(i);
    }
}

static void handleRestartInput()
{
    u16 joy = JOY_readJoypad(JOY_1);
    bool isStartPressedNow = (joy & BUTTON_START) != 0;
    bool wasStartPressedBefore = (previousJoy & BUTTON_START) != 0;

    if (gameState != GAME_STATE_PLAYING && isStartPressedNow && !wasStartPressedBefore)
    {
        resetGame();
    }

    previousJoy = joy;
}

static const char* getPlayerDisplayText()
{
    if (player.state == PLAYER_STATE_DEAD)
    {
        return "KO";
    }

    if (player.state == PLAYER_STATE_HIT)
    {
        return "!!";
    }

    if (player.facing == FACING_RIGHT)
    {
        if (player.state == PLAYER_STATE_ATTACK)
        {
            if (player.comboStep == 3)
            {
                return player.animFrame == 1 ? "P}" : "P>";
            }

            if (player.comboStep == 2)
            {
                return player.animFrame == 1 ? "P]" : "P>";
            }

            return player.animFrame == 1 ? "P)" : "P>";
        }

        if (player.state == PLAYER_STATE_WALK)
        {
            return player.animFrame == 0 ? "P>" : "p>";
        }

        return "P>";
    }

    if (player.state == PLAYER_STATE_ATTACK)
    {
        if (player.comboStep == 3)
        {
            return player.animFrame == 1 ? "{P" : "<P";
        }

        if (player.comboStep == 2)
        {
            return player.animFrame == 1 ? "[P" : "<P";
        }

        return player.animFrame == 1 ? "(P" : "<P";
    }

    if (player.state == PLAYER_STATE_WALK)
    {
        return player.animFrame == 0 ? "<P" : "<p";
    }

    return "<P";
}

static const char* getEnemyDisplayText(Enemy* enemy)
{
    if (enemy->state == ENEMY_STATE_DEAD)
    {
        return "KO";
    }

    if (enemy->state == ENEMY_STATE_HIT)
    {
        return "!!";
    }

    if (enemy->facing == FACING_RIGHT)
    {
        if (enemy->state == ENEMY_STATE_ATTACK)
        {
            if (enemy->animFrame == 1)
            {
                return "E)";
            }

            return "E>";
        }

        if (enemy->state == ENEMY_STATE_WALK)
        {
            return enemy->animFrame == 0 ? "E>" : "e>";
        }

        return "E>";
    }

    if (enemy->state == ENEMY_STATE_ATTACK)
    {
        if (enemy->animFrame == 1)
        {
            return "(E";
        }

        return "<E";
    }

    if (enemy->state == ENEMY_STATE_WALK)
    {
        return enemy->animFrame == 0 ? "<E" : "<e";
    }

    return "<E";
}

static void clearPlayerAttackHitbox()
{
    if (prevPlayerAttackHitbox.visible)
    {
        VDP_drawText("  ", prevPlayerAttackHitbox.tileX, prevPlayerAttackHitbox.tileY);
    }
}

static void drawPlayerAttackHitbox()
{
    clearPlayerAttackHitbox();

    if (playerAttackHitbox.visible)
    {
        VDP_drawText(getPlayerAttackHitboxText(), playerAttackHitbox.tileX, playerAttackHitbox.tileY);
    }
}

static void clearEnemyAttackHitbox(u16 index)
{
    if (prevEnemyAttackHitboxes[index].visible)
    {
        VDP_drawText("  ", prevEnemyAttackHitboxes[index].tileX, prevEnemyAttackHitboxes[index].tileY);
    }
}

static void drawEnemyAttackHitbox(u16 index)
{
    clearEnemyAttackHitbox(index);

    if (enemyAttackHitboxes[index].visible)
    {
        VDP_drawText("@@", enemyAttackHitboxes[index].tileX, enemyAttackHitboxes[index].tileY);
    }
}

static void drawEnemyAttackHitboxes()
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        drawEnemyAttackHitbox(i);
    }
}

static void clearEnemy(u16 index)
{
    Enemy* enemy = &enemies[index];

    if ((enemy->prevTileX != enemy->tileX) || (enemy->prevTileY != enemy->tileY))
    {
        VDP_drawText("  ", enemy->prevTileX, enemy->prevTileY);
    }
}

static void drawEnemy(u16 index)
{
    Enemy* enemy = &enemies[index];

    clearEnemy(index);
    VDP_drawText(getEnemyDisplayText(enemy), enemy->tileX, enemy->tileY);
}

static void drawEnemies()
{
    u16 i;

    for (i = 0; i < MAX_ENEMIES; i++)
    {
        drawEnemy(i);
    }
}

static void clearPlayer()
{
    if ((player.prevTileX != player.tileX) || (player.prevTileY != player.tileY))
    {
        VDP_drawText("  ", player.prevTileX, player.prevTileY);
    }
}

static void drawPlayer()
{
    clearPlayer();
    VDP_drawText(getPlayerDisplayText(), player.tileX, player.tileY);
}

static void drawActors()
{
    /*
        Temporary simple draw order:
        enemies first, player last.
        Later, with real sprites, we will sort every actor by Y/depth.
    */
    drawEnemies();
    drawPlayer();
}

static void drawResultMessage()
{
    if (gameState == GAME_STATE_WIN)
    {
        VDP_drawText("*********** WAVE CLEAR! ***********", 3, 8);
        VDP_drawText("PRESS START TO RESTART", 9, 9);
    }
    else if (gameState == GAME_STATE_LOSE)
    {
        VDP_drawText("*********** YOU ARE KO! ***********", 3, 8);
        VDP_drawText("PRESS START TO RESTART", 9, 9);
    }
    else
    {
        clearTextLine(8);
        clearTextLine(9);
    }
}

static void drawDebugHud()
{
    clearTextLine(24);
    clearTextLine(25);
    clearTextLine(26);
    clearTextLine(27);

    VDP_drawText("P:", 0, 24);
    VDP_drawText(getPlayerStateText(player.state), 3, 24);

    VDP_drawText("HP:", 12, 24);
    drawNumber(player.hp, 16, 24);

    VDP_drawText("C:", 21, 24);
    drawNumber(player.comboStep, 23, 24);

    VDP_drawText("BUF:", 28, 24);
    VDP_drawText(player.queuedNextAttack ? "Y" : "N", 33, 24);

    VDP_drawText("E0:", 0, 25);
    VDP_drawText(getEnemyStateText(enemies[0].state), 4, 25);
    VDP_drawText("H", 9, 25);
    drawNumber(enemies[0].hp, 10, 25);

    VDP_drawText("E1:", 14, 25);
    VDP_drawText(getEnemyStateText(enemies[1].state), 18, 25);
    VDP_drawText("H", 23, 25);
    drawNumber(enemies[1].hp, 24, 25);

    VDP_drawText("E2:", 28, 25);
    VDP_drawText(getEnemyStateText(enemies[2].state), 32, 25);
    VDP_drawText("H", 37, 25);
    drawNumber(enemies[2].hp, 38, 25);

    VDP_drawText("PHITS:", 0, 26);
    drawNumber(playerHitCounter, 7, 26);

    VDP_drawText("EHITS:", 13, 26);
    drawNumber(enemyHitCounter, 20, 26);

    VDP_drawText("PAUSE:", 26, 26);
    drawNumber(hitPauseTimer, 33, 26);

    VDP_drawText("BUILD 009 / 3 HIT COMBO + HIT PAUSE", 1, 27);
}

static void drawFrame()
{
    drawPlayerAttackHitbox();
    drawEnemyAttackHitboxes();
    drawActors();
    drawResultMessage();
    drawDebugHud();
}

int main(bool hard)
{
    JOY_init();

    VDP_setScreenWidth320();

    resetGame();

    drawFrame();

    while (TRUE)
    {
        if (gameState == GAME_STATE_PLAYING)
        {
            if (hitPauseTimer > 0)
            {
                hitPauseTimer--;
            }
            else
            {
                updatePlayer();
                updateEnemies();

                checkPlayerAttackVsEnemies();
                checkEnemiesAttackVsPlayer();
            }
        }
        else
        {
            handleRestartInput();
        }

        drawFrame();

        SYS_doVBlankProcess();
    }

    return 0;
}
