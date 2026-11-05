/* Ninja: Stickman.gd (hooks do lutador) + scripts de estados em states_ninja.c */
#include "ninja.h"
#include "states.h"
#include <string.h>

_Static_assert(sizeof(ninja_t) <= FIGHTER_CH_SIZE, "ninja_t excede a area privada do lutador");

#define RELEASE_MODIFIER FXC(1.175)
#define MAX_MOMENTUM_UPWARD_SPEED FXC(-10.0)
#define SKULL_SHAKER_BLEED_DAMAGE 2
#define BOOST_REDUCTION_PER_FRAME FXC(0.1)
#define MIN_BOOST_REDUCTION_PER_FRAME FXC(0.05)
#define UPPERCUT_HITSTUN_CANCEL_DELAY 2

static inline fx fxi(int v) { return FX_INT(v); }

static void n_init(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    memset(n, 0, sizeof *n);
    n->can_summon_kunai = n->can_summon_kick = n->can_divekick_hop = true;
    n->sticky_bombs_left = 3;
    n->stored_speed_1 = fxi(11);
    n->bomb_uid = n->hook_uid = -1;
    n->cancel_countdown = -1;
    if (f->infinite_resources) n->momentum_stores = 1;
}

void ninja_detach(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    obj_t *hook = n->hook_uid >= 0 ? game_obj_by_uid(eng_game_get(), n->hook_uid) : NULL;
    if (hook) proj_disable(hook);
}

void ninja_explode_sticky_bomb(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    obj_t *b = n->bomb_uid >= 0 ? game_obj_by_uid(eng_game_get(), n->bomb_uid) : NULL;
    if (n->bomb_thrown && b) proj_explode(b);
}

static void n_process_extra(fighter_t *f, const action_extra_t *x)
{
    ninja_t *n = NINJA(f);
    fxv vel = fxv_make(f->o.chara.vx, f->o.chara.vy);
    n->current_momentum = fx_vec_len(vel.x, vel.y);
    if (!f->has_extra) return;
    if ((x->cx_has & NX_EXPLODE) && (x->cx_val & NX_EXPLODE)) ninja_explode_sticky_bomb(f);
    if ((x->cx_has & NX_PULL) && !f->busy_interrupt) n->pulling = (x->cx_val & NX_PULL) != 0;
    if ((x->cx_has & NX_DETACH) && (x->cx_val & NX_DETACH)) {
        if (n->pulling) n->detach_delay = 5; else ninja_detach(f);
    }
    if ((x->cx_has & NX_RELEASE) && (x->cx_val & NX_RELEASE) && (x->cx_has & NX_RELEASE_DIR)) {
        n->will_release_momentum = true;
        fxv imp = obj_xy_to_dir(x->cx_dir.x, x->cx_dir.y, fx_mul(RELEASE_MODIFIER, n->stored_speed_1));
        n->stored_momentum_x = imp.x; n->stored_momentum_y = imp.y;
        if (n->stored_momentum_y < 0 && vel.y + n->stored_momentum_y < MAX_MOMENTUM_UPWARD_SPEED) {
            f->o.chara.vy = 0;
            n->stored_momentum_y = MAX_MOMENTUM_UPWARD_SPEED;
        }
        fighter_use_super_bar(f);
    }
    if (x->cx_has & NX_STORE) n->will_store_momentum = (x->cx_val & NX_STORE) != 0;
}

static bool n_apply_forces(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    if (n->released_this_turn || n->pulling) { phys_apply_forces_no_limit(&f->o.chara); return true; }
    return false;
}

static bool n_apply_grav(fighter_t *f)
{
    state_t *prev = obj_prev_state(&f->o);
    if (prev && strcmp(prev->d.name, "AirDash") == 0 && !state_is_hurt(f->o.sm.cur) && f->o.sm.cur->current_tick <= 7) return true;
    return false;
}

static void release_momentum(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    phys_force(&f->o.chara, n->stored_momentum_x, f->o.chara.grounded ? 0 : n->stored_momentum_y);
    if (!f->infinite_resources) n->momentum_stores = 0;
    if (n->momentum_stores < 0) n->momentum_stores = 0;
    n->released_this_turn = true;
    n->will_release_momentum = false;
    if (f->combo_count > 0) { n->boosted_during_combo = true; f->combo_count += 1; }
    f->colliding_with_opponent = false;
    n->stored_momentum_x = 0; n->stored_momentum_y = 0;
}

static void n_on_state_interruptable(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    if (n->cancel_pending && n->cancel_countdown < 0) n->cancel_countdown = UPPERCUT_HITSTUN_CANCEL_DELAY;
}

__attribute__((weak)) void ninja_hook_tick(fighter_t *f) { NINJA(f)->pulling = false; }   /* sobrescrito em proj_ninja.c */

static void n_tick(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    fighter_t *opp = f->opponent;
    if (n->cancel_countdown >= 0) {
        if (n->cancel_countdown == 0) {
            n->cancel_pending = false; n->cancel_countdown = -1;
            if (opp && fighter_is_in_hurt_state(opp, false)) {
                opp->combo_count = 0; opp->o.hitlag_ticks = 0;
                cs_enable_interrupt(opp, opp->o.sm.cur, false, true);
            }
        } else n->cancel_countdown--;
    }
    if (n->cancel_pending && fighter_is_in_hurt_state(f, false) && opp && fighter_is_in_hurt_state(opp, false)) {
        state_t *a = f->o.sm.cur, *b = opp->o.sm.cur;
        int target = a->hu.hitstun + UPPERCUT_HITSTUN_CANCEL_DELAY;
        if (b->hu.hitstun < target) b->hu.hitstun = target;
    }
    if (f->turn_frames <= 1 && n->boost_frames_left <= 0) n->released_this_turn = false;
    if (fighter_is_in_hurt_state(f, true)) n->pulling = false;
    ninja_hook_tick(f);
    if (n->detach_delay > 0 && --n->detach_delay <= 0) ninja_detach(f);

    if (n->skull_shaker_bleed_ticks > 0) {
        n->skull_shaker_bleed_ticks--;
        if (n->skull_shaker_bleed_ticks % 8 == 0)
            fighter_take_damage(opp, SKULL_SHAKER_BLEED_DAMAGE, 0, FXC(0.25), 0, FX_ONE, false, false);
    }
    if (n->will_release_momentum && f->turn_frames == 1) release_momentum(f);

    if (n->momentum_stores > 0 && f->combo_count <= 0) {
        fx reduction = fx_mul(BOOST_REDUCTION_PER_FRAME, fx_div(n->stored_speed_1, fxi(20)));
        if (n->stored_speed_1 < fxi(8)) reduction = 0;
        else {
            if (reduction < MIN_BOOST_REDUCTION_PER_FRAME) reduction = MIN_BOOST_REDUCTION_PER_FRAME;
            if (n->stored_speed_1 < fxi(10)) reduction = fx_mul(reduction, FX_HALF);
        }
        n->stored_speed_1 -= reduction;
        fxv sd = fxv_norm_times(fxv_make(n->stored_momentum_x, n->stored_momentum_y), n->stored_speed_1);
        n->stored_momentum_x = sd.x; n->stored_momentum_y = sd.y;
        if (n->stored_speed_1 < 0) { n->momentum_stores--; n->stored_speed_1 = 0; }
    }
    if (f->o.chara.grounded) { n->used_grappling_hook = false; n->can_divekick_hop = true; }
}

static void n_on_got_parried(fighter_t *f) { if (NINJA(f)->released_this_turn) f->o.hitlag_ticks += 10; }

static void n_on_got_hit(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    if (n->bomb_uid >= 0 || n->bomb_thrown) {
        n->bomb_thrown = false;
        obj_t *b = n->bomb_uid >= 0 ? game_obj_by_uid(eng_game_get(), n->bomb_uid) : NULL;
        if (b) proj_disable(b);
        n->bomb_uid = -1;
    }
}

static void n_reset_combo(fighter_t *f) { NINJA(f)->boosted_during_combo = false; }

const fighter_vt_t vt_ninja = {
    .init = n_init, .tick = n_tick, .process_extra = n_process_extra, .apply_forces = n_apply_forces, .apply_grav = n_apply_grav,
    .on_state_interruptable = n_on_state_interruptable, .on_got_parried = n_on_got_parried, .on_got_hit = n_on_got_hit,
    .reset_combo = n_reset_combo,
};
