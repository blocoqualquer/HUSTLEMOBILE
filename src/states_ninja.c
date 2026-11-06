/* Scripts de estados do Ninja (characters/stickman/states). */
#include "ninja.h"
#include "states.h"
#include <string.h>
#include <stdlib.h>

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

/* ---------- QuickSlash (SuperMove) ustate: 0 started_in_neutral, 1 attack ---------- */
#define QS_DIST 120
#define QS_SNAP 4
enum { QS_NEUTRAL = 0, QS_ATTACK = 1 };
static sid qs_next_attack(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (!s->ustate[QS_NEUTRAL] && !s->hit_anything) return SID_NONE;
    bool gr = phys_y(&o->chara) > -QS_SNAP;
    switch (s->ustate[QS_ATTACK]) {
    case 1: return by_name(f, gr ? "GroundedPunchQS" : "AirUpwardPunchQS");
    case 2: return by_name(f, gr ? "GroundedSweepQS" : "AirAttackQS");
    case 3: return by_name(f, gr ? "NunChukHeavyQS" : "NunChukSpinQS");
    }
    return SID_NONE;
}
static void qs_followup(obj_t *o, state_t *s)
{
    phys_t *c = &o->chara;
    fx vx = c->vx, vy = c->vy; (void)vy;
    c->vx = fx_mul(c->vx, FXC(0.25)); c->vy = fx_mul(c->vy, FXC(0.5));
    sm_queue_state(o, qs_next_attack(o, s), NULL);
    if (phys_y(c) > -QS_SNAP) {
        c->vx = vx; c->vy = 0;
        phys_move(c, 0, fxi(QS_SNAP));
        c->grounded = true;
        c->vx = fx_mul(vx, FXC(0.35)); c->vy = 0;
    }
}
static sid qs_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    const qs_data_t *d = (const qs_data_t *)s->data;
    fxv dir = d ? obj_xy_to_dir(fxi(d->dir.x), fxi(d->dir.y), FX_ONE) : fxv_make(fxi(o->chara.facing), 0);
    s->ustate[QS_ATTACK] = d ? d->attack : 0;
    for (int i = 0; i < s->d.nhb; i++) { s->hbr[i].box.x = 0; s->hbr[i].box.y = 0; }
    phys_reset_momentum(&o->chara);
    phys_force(&o->chara, fx_mul(dir.x, fxi(20)), fx_div(fx_mul(dir.y, fxi(20)), fxi(2)));
    NINJA(f)->qs_dir_x = dir.x; NINJA(f)->qs_dir_y = dir.y;
    return SID_NONE;
}
static sid qs_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o); ninja_t *n = NINJA(f);
    phys_t *c = &o->chara;
    if (t == 0) {
        s->ustate[QS_NEUTRAL] = f->combo_count <= 0;
        c->grounded = false;
        n->qs_start_x = phys_x(c); n->qs_start_y = phys_y(c);
        if (qs_next_attack(o, s) != SID_NONE && s->ustate[QS_NEUTRAL]) s->current_tick++;
    } else if (t == 4) {
        if (f->initiative) o->invulnerable = true;
    } else if (t == 5) {
        fxv mv = fxv_norm_times(fxv_make(n->qs_dir_x, n->qs_dir_y), fxi(QS_DIST));
        phys_move(c, mv.x, mv.y);
    } else if (t == 6) {
        int ex = phys_x(c), ey = phys_y(c), nh = s->d.nhb;
        for (int i = 0; i < nh; i++) {
            fx ratio = fx_div(fxi(i), fxi(nh));
            s->hbr[i].box.x = (int)fx_round(fx_lerp(fxi(n->qs_start_x), fxi(ex), ratio) - fxi(ex)) * c->facing;
            s->hbr[i].box.y = (int)fx_round(fx_lerp(fxi(n->qs_start_y), fxi(ey), ratio) - fxi(ey)) - 16;
        }
        if (qs_next_attack(o, s) == SID_NONE) {
            phys_reset_momentum(c);
            phys_force(c, n->qs_dir_x, n->qs_dir_y);
            phys_force(c, 0, fxi(-1));
        } else if (s->ustate[QS_NEUTRAL]) qs_followup(o, s);
        o->invulnerable = false;
    }
    return SID_NONE;
}
static void qs_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)victim; (void)hb;
    if (qs_next_attack(o, s) != SID_NONE && !s->ustate[QS_NEUTRAL]) qs_followup(o, s);
}
static sid qs_tick(obj_t *o, state_t *s)
{
    if (qs_next_attack(o, s) != SID_NONE && s->current_tick == 2) s->current_tick = 3;
    bool no_next = qs_next_attack(o, s) == SID_NONE;
    if (s->current_tick > 6 && o->chara.grounded && no_next)
        sm_queue_state(o, by_name(F(o), "Landing"), (void *)(intptr_t)2);
    obj_apply_grav(o);
    phys_apply_forces_no_limit(&o->chara);
    return SID_NONE;
}
static void qs_parried(obj_t *o, state_t *s) { (void)o; (void)s; }
const state_vt_t vt_characters_stickman_states_QuickSlash = { .enter = qs_enter, .frame = qs_frame, .tick = qs_tick, .hit_something = qs_hit, .got_parried = qs_parried };

/* ---------- SwapPlaces ---------- */
#define SP_MAXD 300
#include "projectile.h"
static int sp_usable(fighter_t *f, obj_t **out, int max)
{
    game_t *g = eng_game_get(); int n = 0;
    int mx, my; obj_hurt_center(&f->opponent->o, &mx, &my);
    ninja_t *nj = NINJA(f);
    for (int i = 0; i < g->nobjs && n < max; i++) {
        obj_t *ob = g->objs[i];
        if (ob->is_fighter || ob->id != f->o.id || ob->disabled) continue;
        if (ninja_obj_is_bomb(ob) && PROJ(ob)->attached) continue;
        if (ninja_obj_is_hook(ob) && PROJ(ob)->attached_to >= 0) continue;
        bool sub = false;
        for (int k = 0; k < nj->nsubstituted; k++) if (nj->substituted[k] == ob->uid) sub = true;
        if (sub) continue;
        if (abs(phys_x(&ob->chara) - mx) > SP_MAXD || abs(phys_y(&ob->chara) - my) > SP_MAXD) continue;
        out[n++] = ob;
    }
    return n;
}
static bool sp_is_usable(fighter_t *f, state_t *s, bool base) { (void)s; obj_t *b[GAME_MAX_OBJS]; return base && sp_usable(f, b, GAME_MAX_OBJS) > 0; }
static sid sp_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o); ninja_t *n = NINJA(f);
    if (t == 0) {
        s->ustate[0] = 0; s->ustate[1] = -1;
        if (f->combo_count <= 0) { s->d.anim_length = 13; s->cs.iasa_at = 12; }
        else { s->d.anim_length = 8; s->cs.iasa_at = 7; }
        s->cs.interruptible_on_opponent_turn = false;
    } else if (t == 6) {
        obj_t *list[GAME_MAX_OBJS];
        int cnt = sp_usable(f, list, GAME_MAX_OBJS);
        int start_dir = fighter_opponent_dir(f);
        if (cnt > 0) {
            obj_t *ob = list[cnt - 1];
            int ox = phys_x(&ob->chara), oy = phys_y(&ob->chara);
            int mx, my; obj_hurt_center(o, &mx, &my);
            if (f->combo_count <= 0) {
                int dist = abs(ox - mx);
                int sgn = ox - mx > 0 ? 1 : (ox - mx < 0 ? -1 : 0);
                if (sgn != fighter_opponent_dir(f)) {
                    int pen = (int)fx_round(fx_mul(FXC(0.25), fxi(dist)));
                    pen = pen < 5 ? 5 : (pen > 15 ? 15 : pen);
                    fighter_add_penalty(f, pen, true);
                }
            }
            oy += 18; if (oy > -16) oy = 0;
            phys_set_pos(&o->chara, fxi(ox), fxi(oy));
            phys_set_pos(&ob->chara, fxi(mx), fxi(my));
            ob->chara.facing = o->chara.facing;
            if (f->reverse_state) ob->chara.vx = -ob->chara.vx;
            s->ustate[1] = ob->uid;
            if (n->nsubstituted < 8) n->substituted[n->nsubstituted++] = ob->uid;
            ninja_detach(f);
            if (start_dir != fighter_opponent_dir(f)) fighter_add_penalty(f, 5, true);
        }
        if (f->combo_count == 0) o->hitlag_ticks += 2;
        s->cs.interruptible_on_opponent_turn = true;
    } else if (t == 7 && s->ustate[1] >= 0) {
        obj_t *ob = game_obj_by_uid(eng_game_get(), s->ustate[1]);
        if (ob) {
            state_t *cs = ob->sm.cur;
            for (int i = 0; cs && i < cs->d.nhb; i++) hitbox_reset_hit_objects(&cs->hbr[i]);
            if (ninja_obj_is_hook(ob)) ninja_hook_unlock(ob);
        }
    }
    return SID_NONE;
}
static sid sp_tick(obj_t *o, state_t *s)
{
    (void)o;
    if (s->current_tick > 6 && s->ustate[0] > 0) { s->ustate[0]--; s->current_tick = 6; }
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_SwapPlaces = { .frame = sp_frame, .tick = sp_tick, .usable = sp_is_usable };

/* ---------- DiveKick / DiveKick2 (grounded = xparam) ustate: 0 moving, 1 move_x_amount, 2 move_y_amount, 3 mxm, 4 mym, 5 data_y ---------- */
static sid dk_enter(obj_t *o, state_t *s)
{
    (void)o;
    const xy_data_t *d = (const xy_data_t *)s->data;
    s->ustate[5] = d ? d->y : 1;                    /* data = {x:1,y:1} se nao for dicionario */
    return SID_NONE;
}
static sid dk_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    const xy_data_t *d = (const xy_data_t *)s->data;
    int dx = d ? d->x : 1, dy = d ? d->y : 1;
    bool grounded = xp_int(&s->d, "grounded", 0);
    int move_x = xp_int(&s->d, "move_x", 3), move_y = xp_int(&s->d, "move_y", 3);
    int xm = xp_int(&s->d, "x_modifier_amount", 2), ym = xp_int(&s->d, "y_modifier_amount", 2);
    if (t == 0) {
        if (!grounded) { o->chara.vx = fx_mul(o->chara.vx, FXC(0.75)); o->chara.vy = 0; }
        else if (s->hbr) {
            hitbox_t *hb = &s->hbr[0];
            hb->box.y = -9; hb->hits_vs_grounded = 1; hb->block_cancel_allowed = 0; hb->dir_y = FX_ONE;
            if (dy < 0) { hb->hits_vs_grounded = 0; hb->block_cancel_allowed = 1; hb->dir_y = -FX_ONE; hb->box.y -= 16; }
        }
        s->ustate[0] = 0;
        s->ustate[3] = abs(dx) * xm; s->ustate[4] = dy * ym;
        int mxa = move_x + s->ustate[3];
        int mya = (dy < 0 ? -1 : 1) * move_y + s->ustate[4];
        s->ustate[1] = mxa; s->ustate[2] = mya;
    } else if (t == 11) {
        phys_reset_momentum(&o->chara);
        phys_force_rel(&o->chara, fxi(move_x + s->ustate[3]), fxi(move_y + s->ustate[4]));
    } else if (t == 12) s->ustate[0] = 1;
    (void)f;
    return SID_NONE;
}
static void dk_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)hb;
    fighter_t *f = F(o); ninja_t *n = NINJA(f);
    bool grounded = xp_int(&s->d, "grounded", 0);
    int dy = s->ustate[5];
    if (n->can_divekick_hop && !victim->is_fighter && victim->id == o->id) {
        sm_queue_state(o, by_name(f, "DiveKickHop"), NULL);
        f->feinting = false; o->hitlag_ticks = 0; n->can_divekick_hop = false;
    }
    if (grounded) {
        if (dy >= 0) o->chara.vy = 0;
        else o->chara.vy = fx_mul(fxi((dy > 0 ? 1 : -1) * xp_int(&s->d, "move_y", 3) + s->ustate[4]), FXC(0.6));
        s->ustate[0] = 0;
        if (victim->is_fighter) sm_queue_state(o, by_name(f, "Fall"), NULL);
    }
}
static void dk_perfect_parried(obj_t *o, state_t *s) { if (s->ustate[5] < 0) o->hitlag_ticks += 8; }
static void dk_exit_shared(obj_t *o, state_t *s)
{
    bool pass = s->ustate[5] < 0 && s->current_tick > 20 && s->feinting;
    cs_exit_shared(o, s);
    if (pass) F(o)->feinting = true;
}
static sid dk_tick(obj_t *o, state_t *s)
{
    bool grounded = xp_int(&s->d, "grounded", 0);
    fighter_t *f = F(o);
    if (!grounded && s->current_tick == 3) s->current_tick = f->initiative ? 9 : 6;
    if (grounded && s->ustate[5] < 0 && s->current_tick == 4) s->current_tick = 8;
    if (s->ustate[5] < 0 && s->current_tick > 20) return by_name(f, "DiveKickHopLong");
    if (s->ustate[0]) phys_move_rel(&o->chara, fxi(s->ustate[1]), fxi(s->ustate[2]));
    else obj_apply_forces(o);
    if (o->chara.grounded && s->current_tick > (grounded ? 10 : 5)) {
        phys_reset_momentum(&o->chara);
        phys_force_rel(&o->chara, fxi((xp_int(&s->d, "move_x", 3) + s->ustate[3]) / 2), 0);
        return by_name(f, "Landing");
    }
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_states_DiveKick = { .enter = dk_enter, .frame = dk_frame, .tick = dk_tick, .hit_something = dk_hit, .got_perfect_parried = dk_perfect_parried, .exit_shared = dk_exit_shared };

/* ---------- NunChukLight / LightHigh (extends NunChuk) ustate[0] = charged ---------- */
static sid ncl_enter(obj_t *o, state_t *s)
{
    state_t *prev = obj_prev_state(o);
    const char *pn = prev ? prev->d.name : "";
    bool cur_light = strcmp(s->d.name, "NunChukLight") == 0 || strcmp(s->d.name, "NunChukLightHigh") == 0;
    bool prev_light = strcmp(pn, "NunChukLight") == 0 || strcmp(pn, "NunChukLightHigh") == 0;
    if (strstr(pn, "NunChukHeavy") && cur_light) s->ustate[0] = 1;
    else if (prev_light && prev->ustate[0]) s->ustate[0] = 1;
    else s->ustate[0] = 0;
    const xy_data_t *d = (const xy_data_t *)s->data;
    if (d && d->y != 0 && strcmp(s->d.name, "NunChukLight") == 0) return by_name(F(o), "NunChukLightHigh");
    return SID_NONE;
}
static sid ncl_frame(obj_t *o, state_t *s, int t)
{
    if (t == 4 && s->ustate[0] && s->d.projectile_scene) eng_spawn_exported_projectile(o, s);
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_NunChukLight = { .enter = ncl_enter, .frame = ncl_frame, .got_perfect_parried = nc_parried };

/* ---------- WhipWaveParriable ---------- */
static void wwp_process(obj_t *o, state_t *s, obj_t *pr) { (void)o; (void)s; PROJ(pr)->whip_parriable = true; }
const state_vt_t vt_characters_stickman_states_WhipWaveParriable = { .process_projectile = wwp_process };
