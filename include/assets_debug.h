//NOTE: SHOULD NOT BE INCLUDED MANUALLY
//ONLY INCLUDE assets.h

#ifndef ASSET_IMPL
#define ASSET_IMPL

#include <assert.h>
#include "raylib.h"
#include "assets.h"

const char* asset_map[ASSET_COUNT] = {
    [DEFAULT_FONT] = "assets/fonts/default.ttf",
    [FLOOR_TEXTURE] = "assets/sprites/floor.png",
    [SIMPLE_BORDER] = "assets/sprites/ui/button.png",
    [BOX] = "assets/sprites/ui/box.png",
    [SPELL_BOX] = "assets/sprites/ui/spell_box.png",
    [SPELL_BOX_SELECT] = "assets/sprites/ui/spell_box_select.png",
    [GAME_SLOT] = "assets/sprites/ui/game_slot.png",
    [LIFE_BAR_BG] = "assets/sprites/ui/life_bar_bg.png",
    [WALL_TEXTURE] = "assets/sprites/walls/wall.png",
    [PLAYER_TEXTURE] = "assets/sprites/wizzard_idle.png",
    [WALL_TORCH] = "assets/sprites/wall_torch.png",
    [SLASH_ATTACK] = "assets/sprites/attacks/slash.png",
    [HEAL_ATTACK] = "assets/sprites/attacks/heal.png",
    [ICONS] = "assets/sprites/icons/icons.png",
    [UI_BUTTON_CLICKED] = "assets/sounds/10_UI_Menu_SFX/013_Confirm_03.wav",
    [UI_TAB_SWITCH] = "assets/sounds/10_UI_Menu_SFX/029_Decline_09.wav",
    [MOVE_SOUND] = "assets/sounds/16_human_walk_stone_2.wav",
    [ATTACK_SOUND] = "assets/sounds/07_human_atk_sword_1.wav",
    [STUN_SOUND] = "assets/sounds/8_Buffs_Heals_SFX/44_Sleep_01.wav",
    [BURN_SOUND] = "assets/sounds/8_Atk_Magic_SFX/04_Fire_explosion_04_medium.wav",
    [HEAL_SOUND] = "assets/sounds/8_Buffs_Heals_SFX/02_Heal_02.wav",
    [DEATH_SOUND] = "assets/sounds/10_Battle_SFX/69_Enemy_death_01.wav",
    [WIN_ROUND_SOUND] = "assets/sounds/8_Buffs_Heals_SFX/16_Atk_buff_04.wav",
    [LOSE_ROUND_SOUND] = "assets/sounds/8_Buffs_Heals_SFX/17_Def_buff_01.wav",
};

const char* ASSET(asset a) {
    assert(a >= 0 && a < ASSET_COUNT);
    return asset_map[a];
}

Font load_font(asset type) {
    return LoadFont(ASSET(type));
}

Sound load_sound(asset type) {
    return LoadSound(ASSET(type));
}

Texture2D load_texture(asset type) {
    return LoadTexture(ASSET(type));
}

#endif
