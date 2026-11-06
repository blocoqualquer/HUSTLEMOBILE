/* Ninja (Stickman.gd / NinjaGuy): estado privado do personagem. */
#ifndef NINJA_H
#define NINJA_H
#include "fighter.h"
#include "projectile.h"

typedef struct {
    bool can_summon_kunai, can_summon_kick, bomb_thrown, pulling, used_grappling_hook, whip_beam_charged;
    bool released_this_turn, will_release_momentum, will_store_momentum, boosted_during_combo, stackriken_out, can_divekick_hop;
    bool cancel_pending;
    int bomb_uid, hook_uid;                   /* uid do objeto ou -1 */
    int momentum_stores, sticky_bombs_left, skull_shaker_bleed_ticks, boost_frames_left, detach_delay, cancel_countdown;
    int qs_start_x, qs_start_y;
    fx stored_momentum_x, stored_momentum_y, stored_speed_1, current_momentum, qs_dir_x, qs_dir_y;
    int substituted[8]; int nsubstituted;     /* SwapPlaces: uids ja trocados */
} ninja_t;

#define NINJA(f) ((ninja_t *)(void *)(f)->ch)
extern const fighter_vt_t vt_ninja;

/* bits de extras (action_extra_t.cx_*) */
enum { NX_EXPLODE = 1 << 0, NX_PULL = 1 << 1, NX_DETACH = 1 << 2, NX_RELEASE = 1 << 3, NX_RELEASE_DIR = 1 << 4, NX_STORE = 1 << 5 };

void ninja_detach(fighter_t *f);
void ninja_explode_sticky_bomb(fighter_t *f);
void ninja_hook_unlock(obj_t *self);
bool ninja_obj_is_bomb(obj_t *o);
bool ninja_obj_is_hook(obj_t *o);
/* data do QuickSlash: {Direction:{x,y}, Attack:{id}} */
typedef struct { xy_data_t dir; int attack; } qs_data_t;
#endif
