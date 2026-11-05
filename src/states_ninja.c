/* Scripts de estados do Ninja (characters/stickman/states). */
#include "ninja.h"
#include "states.h"
#include <string.h>

#define F(o) ((o)->fighter)
static inline fx fxi(int v) { return FX_INT(v); }
static sid by_name(fighter_t *f, const char *n) { return fighter_name_id(f, n); }
static int moves_used(fighter_t *f, const char *name)
{
    int id = by_name(f, name);
    for (int i = 0; i < f->nmoves_used; i++) if (f->moves_used[i].name_id == id) return f->moves_used[i].count;
    return -1;
}

/* ---------- BackSway ---------- */
static sid bs_enter(obj_t *o, state_t *s) { (void)s; if (o->chara.grounded) phys_reset_momentum(&o->chara); return SID_NONE; }
static sid bs_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t == 2 && F(o)->initiative) o->invulnerable = true;
    if (t == 9) o->invulnerable = false;
    return SID_NONE;
}
static sid bs_tick(obj_t *o, state_t *s) { (void)s; phys_force_rel(&o->chara, FXC(1.2), 0); return SID_NONE; }
const state_vt_t vt_characters_stickman_states_BackSway = { .enter = bs_enter, .frame = bs_frame, .tick = bs_tick };

/* ---------- FlyingRoundhouse (ustate: 0 left_ground, 1 started_on_ground, 2 had_invuln) ---------- */
enum { FR_LEFT = 0, FR_START_GROUND = 1, FR_INVULN = 2 };
static sid fr_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    switch (t) {
    case 0: s->ustate[FR_LEFT] = 0; s->ustate[FR_START_GROUND] = o->chara.grounded; break;
    case 1:
        s->ustate[FR_INVULN] = 0;
        if (f->initiative) { o->projectile_invulnerable = true; s->ustate[FR_INVULN] = 1; }
        break;
    case 7: if (!s->started_in_air) s->ustate[FR_LEFT] = 1; break;
    }
    return SID_NONE;
}
static sid fr_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (s->current_tick <= 6) s->ustate[FR_LEFT] = 0;
    else if (s->started_in_air && o->chara.grounded) {
        if (s->current_tick < 15) {
            o->chara.vy = 0;
            fxv force = fxv_norm_times(fxv_make(FXC(1.1), FXC(-0.25)), fxi(11));
            phys_force_rel(&o->chara, force.x, force.y);
        } else s->ustate[FR_LEFT] = 1;
    }
    if (s->current_tick >= 24) o->projectile_invulnerable = false;
    if (s->hbr) s->hbr[0].block_cancel_allowed = !s->ustate[FR_START_GROUND] && !f->opponent->o.chara.grounded;
    phys_fric(&o->chara); obj_apply_grav(o); obj_apply_forces(o);
    if (s->ustate[FR_LEFT] && o->chara.grounded) sm_queue_state(o, by_name(f, "JumpKickLanding"), (void *)(intptr_t)8);
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_FlyingRoundhouse = { .frame = fr_frame, .tick = fr_tick };

static sid jkl_enter(obj_t *o, state_t *s)
{
    (void)s;
    state_t *prev = obj_prev_state(o);
    if (prev && strcmp(prev->d.name, "FlyingRoundhouse") == 0 && prev->ustate[FR_INVULN]) o->projectile_invulnerable = true;
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_JumpKickLanding = { .enter = jkl_enter, VT_LANDING };

/* ---------- GroundedSweep (extends Idle) ---------- */
static void gs_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)s; (void)hb;
    if (victim->is_fighter) phys_move_rel(&o->chara, fxi(16), 0);
}
const state_vt_t vt_characters_stickman_states_GroundedSweep = { VT_IDLE, .hit_something = gs_hit };

/* ---------- HitGrab / HitGrabFollowup ---------- */
static sid hg_frame(obj_t *o, state_t *s, int t)
{
    if (t != 0) return SID_NONE;
    const xy_data_t *d = (const xy_data_t *)s->data;
    fx amount = fx_div(fxi(d ? d->x : 0), fxi(100));
    phys_force_rel(&o->chara, fx_lerp(fxi(3), fxi(14), amount), 0);
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_HitGrab = { .frame = hg_frame };

static void hgf_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)s; (void)hb;
    fighter_t *f = F(o);
    if (victim == &f->opponent->o) {
        int c = moves_used(f, "HitGrab");
        if (c >= 0 && c <= 1) NINJA(f)->skull_shaker_bleed_ticks = 80;
    }
}
const state_vt_t vt_characters_stickman_states_HitGrabFollowup = { VT_THROWSTATE, .hit_something = hgf_hit };

/* ---------- NunChuk / NunChukJump / PalmStrike / SlideKick / Stomp / Taunt ---------- */
static void nc_parried(obj_t *o, state_t *s) { if (xp_int(&s->d, "heavy", 0)) o->hitlag_ticks += 4; }
const state_vt_t vt_characters_stickman_states_NunChuk = { .got_perfect_parried = nc_parried };

static sid ncj_frame(obj_t *o, state_t *s, int t)
{
    if (t != 1) return SID_NONE;
    const xy_data_t *d = (const xy_data_t *)s->data;
    if (d) s->ustate[0] = fx_round(fx_mul(fx_div(fxi(d->x), fxi(100)) - FX_HALF, fxi(10)));
    phys_force_rel(&o->chara, fxi(s->ustate[0]), 0);
    return SID_NONE;
}
static sid ncj_tick(obj_t *o, state_t *s)
{
    if (s->current_tick > 2 && o->chara.grounded) return by_name(F(o), "Landing");
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_NunChukJump = { .frame = ncj_frame, .tick = ncj_tick };

static sid sk_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    if (t == 0) s->d.force_tick = f->initiative ? 1 : 3;
    else if (t == 2) {
        if (f->initiative) o->projectile_invulnerable = true;
        if (f->reverse_state && f->combo_count <= 0) fighter_add_penalty(f, 25, false);
    } else if (t == 13) o->projectile_invulnerable = false;
    return SID_NONE;
}
static sid sk_tick(obj_t *o, state_t *s)
{
    (void)s;
    phys_apply_forces_no_limit(&o->chara);
    phys_x_fric(&o->chara, FXC(0.06));
    fx len = fx_vec_len(o->chara.vx, o->chara.vy);
    if (len > fxi(22)) { fxv nv = fxv_norm_times(fxv_make(o->chara.vx, o->chara.vy), fxi(22)); o->chara.vx = nv.x; o->chara.vy = nv.y; }
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_SlideKick = { .frame = sk_frame, .tick = sk_tick };

static sid stomp_tick(obj_t *o, state_t *s) { (void)s; phys_fric(&o->chara); obj_apply_forces(o); return SID_NONE; }
const state_vt_t vt_characters_stickman_states_Stomp = { .tick = stomp_tick };

const state_vt_t vt_characters_stickman_states_Taunt = { .frame = taunt_frame };

/* ---------- StoreMomentum ---------- */
static sid stm_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t != 1) return SID_NONE;
    ninja_t *n = NINJA(F(o));
    n->stored_momentum_x += fx_mul(fx_abs(o->chara.vx), FXC(0.90));
    n->stored_momentum_y += fx_mul(fx_abs(o->chara.vy), FXC(0.90));
    fx speed = fx_vec_len(n->stored_momentum_x, n->stored_momentum_y);
    phys_reset_momentum(&o->chara);
    n->momentum_stores = 1;
    n->stored_speed_1 = speed;
    return SID_NONE;
}
static sid stm_tick(obj_t *o, state_t *s) { (void)s; phys_fric(&o->chara); obj_apply_grav(o); obj_apply_forces(o); return SID_NONE; }
static bool stm_usable(fighter_t *f, state_t *s, bool base) { (void)s; return base && (NINJA(f)->momentum_stores < 3 || f->infinite_resources); }
const state_vt_t vt_characters_stickman_states_StoreMomentum = { .frame = stm_frame, .tick = stm_tick, .usable = stm_usable };

/* ---------- Uppercut (ustate[0] = was_neutral_hit) ---------- */
static sid uc_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    switch (t) {
    case 0: {
        int air = xp_int(&s->d, "air", 0);
        s->ustate[0] = !air && f->opponent && !fighter_is_in_hurt_state(f->opponent, false);
        if (f->initiative && o->chara.grounded) o->invulnerable = true;
        if (s->cs.air_type != AIR_AERIAL) {
            if (fx_sign(o->chara.vx) != o->chara.facing) phys_reset_momentum(&o->chara);
            else { fx vy = o->chara.vy; phys_reset_momentum(&o->chara); o->chara.vx = fx_div(o->chara.vx, fxi(3)); o->chara.vy = vy; }
        }
        if (s->d.nhb > 1) s->hbr[1].block_punishable = false;
        break; }
    case 4: o->invulnerable = false; break;
    case 8: o->throw_invulnerable = false; break;
    case 9: o->invulnerable = false; break;
    }
    return SID_NONE;
}
static void uc_got_blocked(obj_t *o, state_t *s) { (void)o; if (s->d.nhb > 1) s->hbr[1].block_punishable = true; }
static void uc_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)victim; (void)hb;
    if (!xp_int(&s->d, "air", 0)) {
        phys_force(&o->chara, 0, fxi(6));
        if (s->ustate[0]) { NINJA(F(o))->cancel_pending = true; NINJA(F(o))->cancel_countdown = -1; }
    }
}
static sid uc_tick(obj_t *o, state_t *s)
{
    obj_apply_grav(o); obj_apply_forces(o);
    if (o->chara.grounded && s->current_tick > s->d.force_tick) return by_name(F(o), "UppercutLanding");
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_Uppercut = { .frame = uc_frame, .tick = uc_tick, .hit_something = uc_hit, .got_blocked = uc_got_blocked };

/* ====================== estados que criam projeteis ====================== */
extern const chardata_t chardata_ninja_Caltrops, chardata_ninja_StickyBomb, chardata_ninja_StickmanSummonKunai, chardata_ninja_StickmanSummonSlide;

/* ---------- Fireball / AirFireball (ustate: 0 sm_has, 1 projectile_spawned, 2 startup_lag; ufx[0] speed_modifier) ---------- */
static sid fb_enter(obj_t *o, state_t *s) { (void)o; s->ustate[2] = 3; return SID_NONE; }
static sid fb_frame(obj_t *o, state_t *s, int t)
{
    const xy_data_t *d = (const xy_data_t *)s->data;
    if (t == 0) {
        o->chara.vx = fx_mul(o->chara.vx, FXC(0.75)); o->chara.vy = fx_mul(o->chara.vy, FXC(0.75));
        if (d) {
            fxv dir = obj_xy_to_dir(fxi(d->x), fxi(d->y), FX_ONE);
            fxv vec = fxv_mul(dir, xp_fx(&s->d, "speed_modifier_amount", FXC(2.0)));
            s->ufx[0] = fx_vec_len(vec.x, vec.y); s->ustate[0] = 1;
        }
        s->ustate[1] = 0;
    } else if (t == 5) {
        fighter_t *f = F(o);
        s->ustate[1] = 1;
        proj_speed_data_t sd = { s->ustate[0] != 0, s->ufx[0] };
        proj_t *p = proj_spawn_from(o, (const chardata_t *)xp_ptr(&s->d, "projectile"), xp_int(&s->d, "projectile_x", 16), xp_int(&s->d, "projectile_y", -16), true, &sd, sizeof sd);
        if (!p || !d) return SID_NONE;
        fxv dir = obj_xy_to_dir(fxi(d->x), fxi(d->y), FX_ONE);
        dir.x = fx_mul(dir.x, fxi(o->chara.facing));
        if (s->cs.air_type == AIR_GROUNDED) phys_force_rel(&o->chara, xp_fx(&s->d, "push_back_amount", FXC(-2.0)), 0);
        p->dir_x = fx_mul(dir.x, FXC(7.5)); p->dir_y = fx_mul(dir.y, FXC(7.5));
        (void)f;
    }
    return SID_NONE;
}
static sid fb_tick(obj_t *o, state_t *s)
{
    if (s->ustate[2] > 0 && s->current_tick == 3) { s->ustate[2]--; s->current_tick = 2; }
    phys_fric(&o->chara); obj_apply_forces(o);
    if (s->cs.air_type == AIR_AERIAL && s->ustate[1]) {
        obj_apply_grav(o);
        if (o->chara.grounded) return by_name(F(o), "Landing");
    }
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_Fireball = { .enter = fb_enter, .frame = fb_frame, .tick = fb_tick };

/* ---------- Caltrops ---------- */
static sid cal_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t != 6) return SID_NONE;
    static const int horiz[3] = { 5, 9, 14 };
    for (int i = 0; i < 3; i++) {
        proj_t *p = proj_spawn_from(o, &chardata_ninja_Caltrops, 5, -16, true, NULL, 0);
        if (p) phys_force_rel(&p->o.chara, fxi(horiz[i]), fxi(-3));
    }
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_Caltrops = { .frame = cal_frame };

/* ---------- GrapplingHook (ustate[0] = projectile_spawned) ---------- */
static sid gh_frame(obj_t *o, state_t *s, int t) { if (t == 0) { s->ustate[0] = 0; NINJA(F(o))->used_grappling_hook = true; } return SID_NONE; }
static void gh_process(obj_t *o, state_t *s, obj_t *pr)
{
    s->ustate[0] = 1;
    pr->chara.grounded = false;
    const xy_data_t *d = (const xy_data_t *)s->data;
    fxv force = obj_xy_to_dir(fxi(d ? d->x : 0), fxi(d ? d->y : 0), fxi(15));
    phys_force(&pr->chara, o->chara.vx + force.x, o->chara.vy + force.y);
    NINJA(F(o))->hook_uid = pr->uid;
    PROJ(pr)->start_y = phys_y(&o->chara);
}
static sid gh_tick(obj_t *o, state_t *s)
{
    phys_fric(&o->chara); obj_apply_forces(o);
    if (s->cs.air_type == AIR_AERIAL && s->ustate[0]) {
        if (F(o)->combo_count > 0) obj_apply_grav(o);
        if (o->chara.grounded) return by_name(F(o), "Landing");
    }
    return SID_NONE;
}
static bool gh_usable(fighter_t *f, state_t *s, bool base) { (void)s; return NINJA(f)->hook_uid < 0 && !NINJA(f)->used_grappling_hook && base; }
const state_vt_t vt_characters_stickman_states_GrapplingHook = { .frame = gh_frame, .tick = gh_tick, .process_projectile = gh_process, .usable = gh_usable };

/* ---------- StickyBomb / StickyBombThrow ---------- */
static sid sb_frame(obj_t *o, state_t *s, int t) { (void)s; if (t == 0) F(o)->colliding_with_opponent = false; return SID_NONE; }
static sid sb_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (s->current_tick == 15) {
        proj_t *p = proj_spawn_from(o, &chardata_ninja_StickyBomb, 20, 0, true, NULL, 0);
        ninja_t *n = NINJA(f);
        n->bomb_thrown = true;
        if (p) {
            n->bomb_uid = p->o.uid;
            fxv force = fxv_norm_times(fxv_make(0, FXC(1.15)), fxi(15));
            phys_force(&p->o.chara, force.x, force.y);
        }
    }
    if (s->current_tick > 10 && o->chara.grounded) return by_name(f, "Landing");
    if (s->data) phys_grav_custom(&o->chara, FXC(0.35), fxi(5)); else obj_apply_grav(o);
    return SID_NONE;
}
static bool sb_usable(fighter_t *f, state_t *s, bool base) { (void)s; return base && !NINJA(f)->bomb_thrown && NINJA(f)->sticky_bombs_left > 0; }
const state_vt_t vt_characters_stickman_states_StickyBomb = { .frame = sb_frame, .tick = sb_tick, .usable = sb_usable };

static sid sbt_enter(obj_t *o, state_t *s)
{
    th_enter(o, s);
    phys_reset_momentum(&o->chara);
    phys_reset_momentum(&F(o)->opponent->o.chara);
    s->ustate[1] = phys_x(&o->chara); s->ustate[2] = phys_y(&o->chara);
    return SID_NONE;
}
static sid sbt_frame(obj_t *o, state_t *s, int t)
{
    if (t == 30) {
        proj_t *p = proj_spawn_from(o, &chardata_ninja_StickyBomb, 0, -16, true, NULL, 0);
        NINJA(F(o))->bomb_thrown = true;
        if (p) { NINJA(F(o))->bomb_uid = p->o.uid; p->attached = true; }
    }
    (void)s;
    return SID_NONE;
}
static sid sbt_tick(obj_t *o, state_t *s)
{
    if (s->current_tick > 29) {
        s->cs.interruptible_on_opponent_turn = 1;
        obj_apply_grav(o); phys_fric(&o->chara); obj_apply_forces(o);
    } else s->cs.interruptible_on_opponent_turn = 0;
    if (s->current_tick < 28) phys_set_pos(&o->chara, fxi(s->ustate[1]), fxi(s->ustate[2]));
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_StickyBombThrow = { .enter = sbt_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit, .frame = sbt_frame, .tick = sbt_tick };

/* ---------- Summon ---------- */
static sid su_tick(obj_t *o, state_t *s) { (void)s; phys_fric(&o->chara); obj_apply_forces(o); return SID_NONE; }
static sid su_frame(obj_t *o, state_t *s, int t)
{
    if (t != 4) return SID_NONE;
    const xy_data_t *d = (const xy_data_t *)s->data;
    if (!d) return SID_NONE;
    if (d->x != 0) {
        int off = d->y != 0 ? 0 : -1;
        proj_t *p = proj_spawn_from(o, &chardata_ninja_StickmanSummonKunai, 2, -16 + off, true, d, sizeof *d);
        if (p) p->o.chara.facing = o->chara.facing;
        NINJA(F(o))->can_summon_kunai = false;
    } else {
        proj_t *p = proj_spawn_from(o, &chardata_ninja_StickmanSummonSlide, 2, -16, true, NULL, 0);
        if (p) p->o.chara.facing = o->chara.facing;
        NINJA(F(o))->can_summon_kick = false;
    }
    return SID_NONE;
}
static bool su_usable(fighter_t *f, state_t *s, bool base) { (void)s; return (base && NINJA(f)->can_summon_kunai) || NINJA(f)->can_summon_kick; }
const state_vt_t vt_characters_stickman_states_Summon = { .frame = su_frame, .tick = su_tick, .usable = su_usable };
