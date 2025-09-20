#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>
typedef enum {
    DEFAULT_FONT,
    SIMPLE_BORDER,
    BOX,
    SPELL_BOX,
    SPELL_BOX_SELECT,
    GAME_SLOT,
    LIFE_BAR_BG,
    FLOOR_TEXTURE,
    WALL_TEXTURE,
    TEST_WALL_TEXTURE,
    PLAYER_TEXTURE,
    WALL_TORCH,
    SLASH_ATTACK,
    HEAL_ATTACK,
    ICONS,
    EFFECTS,
    UI_BUTTON_CLICKED,
    UI_TAB_SWITCH,
    MOVE_SOUND,
    ATTACK_SOUND,
    STUN_SOUND,
    BURN_SOUND,
    HEAL_SOUND,
    DEATH_SOUND,
    WIN_ROUND_SOUND,
    LOSE_ROUND_SOUND,
    ERROR_SOUND,
    RAINDROP_SOUND,
    VINE,
    ASSET_COUNT,
} asset;

//TODO: Inlcude extension ?
typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t size;
    unsigned char* content;
} pak_entry;

#ifndef DEBUG
#include "assets_release.h"
#else
#include "assets_debug.h"
#endif

#define FLOOR_TEXTURE_COUNT 8
#define PLAYER_ANIMATION_COUNT 4
#define WALL_TORCH_ANIMATION_COUNT 8
#define SLASH_ANIMATION_COUNT 3
#define HEAL_ANIMATION_COUNT 4
#define WALL_ORIENTATION_COUNT 16
#define VINE_ANIMATION_COUNT 3

Sound load_sound(asset type);
Texture2D load_texture(asset type);

#endif
