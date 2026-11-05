/* Estados comuns: Burst, OffensiveBurst, CounterAttack, InstantCancel, SuperFall, ChargeDash, AirDash, RollStartup, Grab, ThrowState, Frozen, Forfeit. */
#include "fighter.h"
#include <string.h>

static inline fx fxi(int v) { return FX_INT(v); }
static inline int rnd(fx v) { return (int)fx_round(v); }
static inline int isign(int v) { return (v > 0) - (v < 0); }
#define F(o) ((o)->fighter)
enum { AIR_GROUNDED, AIR_AERIAL };

static sid by_name(fighter_t *f, const char *n) { return fighter_name_id(f, n); }
static fxv xy_div(fx x, fx y, fx mul, fx div) { return fxv_make(fx_mul(fx_div(x, div), mul), fx_mul(fx_div(y, div), mul)); }

static void use_burst(fighter_t *f)
{
    if (--f->bursts_available < 0) f->bursts_available = 0;
    fighter_refresh_air_movements(f);
}
static void use_burst_meter(fighter_t *f, int amount)
{
    if (f->bursts_available > 0) { f->bursts_available = 0; f->burst_meter = MAX_BURST_METER; }
    f->burst_meter -= amount;
}

/* ---------- Burst / OffensiveBurst ---------- */

static sid burst_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    o->invulnerable = true; o->projectile_invulnerable = true;
    s->cs.interruptible_on_opponent_turn = 0;
    fighter_reset_combo(f->opponent);
    s->ustate[0] = 0;
    use_burst(f);
    return SID_NONE;
}
static sid ob_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    use_burst(f);
    o->projectile_invulnerable = true;
    phys_update_grounded(&o->chara);
    s->ustate[0] = 0;
    o->invulnerable = true;
    return SID_NONE;
}
static sid burst_frame(obj_t *o, state_t *s, int t)
{
    if (t == 15) { o->invulnerable = false; s->ustate[0] = 1; }
    return SID_NONE;
}
static sid burst_tick(obj_t *o, state_t *s)
{
    if (s->ustate[0] && o->gravity_enabled) obj_apply_grav(o);
    obj_apply_forces(o);
    o->invulnerable = !(s->current_tick > 15);
    return SID_NONE;
}
static void burst_exit(obj_t *o, state_t *s) { (void)s; o->projectile_invulnerable = false; }
const state_vt_t vt_characters_states_Burst = { .enter = burst_enter, .frame = burst_frame, .tick = burst_tick, .exit = burst_exit };
const state_vt_t vt_characters_states_OffensiveBurst = { .enter = ob_enter, .frame = burst_frame, .tick = burst_tick, .exit = burst_exit };

/* ---------- CounterAttack ---------- */

static sid counter_enter(obj_t *o, state_t *s)
{
    s->ustate[0] = 1;       /* bracing */
    use_burst_meter(F(o), rnd(fx_mul(fxi(MAX_BURST_METER), 8192)));
    return SID_NONE;
}
static sid counter_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    bool low = xp_int(&s->d, "counter_type", 0) == 1;
    fighter_set_anim(f, s, (o->chara.grounded && !low) ? "ParryHigh" : "ParryLow");
    if (o->chara.grounded) { o->chara.py = 0; phys_x_fric(&o->chara, 3277); }
    else { phys_x_fric(&o->chara, 983); phys_grav_custom(&o->chara, 16384, 983040); }
    phys_apply_forces_no_limit(&o->chara);
    return SID_NONE;
}
const state_vt_t vt_characters_states_CounterAttack = { .enter = counter_enter, .tick = counter_tick };

/* ---------- InstantCancel / SuperFall / ChargeDash / Forfeit ---------- */

static sid ic_enter_whiff(obj_t *o, state_t *s) { (void)s; use_burst_meter(F(o), rnd(fx_mul(fxi(MAX_BURST_METER), 49152))); return SID_NONE; }
static sid ic_tick(obj_t *o, state_t *s) { (void)s; phys_fric(&o->chara); if (o->gravity_enabled) obj_apply_grav(o); obj_apply_forces(o); return SID_NONE; }
const state_vt_t vt_characters_states_InstantCancel = { .tick = ic_tick };
const state_vt_t vt_characters_states_WhiffInstantCancel = { .enter = ic_enter_whiff, .tick = ic_tick };

static sid sfs_frame(obj_t *o, state_t *s, int t) { if (t == 5) sm_queue_state(o, s->d.fallback_state, NULL); return SID_NONE; }
const state_vt_t vt_characters_states_SuperFallStartup = { .frame = sfs_frame };
static sid sf_frame(obj_t *o, state_t *s, int t)
{
    if (t == 1) return SID_NONE;
    return SID_NONE; (void)o; (void)s;
}
static sid sf_tick(obj_t *o, state_t *s)
{
    fx sp = xp_fx(&s->d, "move_speed", 10 << 16);
    phys_move(&o->chara, 0, sp);
    return SID_NONE;
}
const state_vt_t vt_characters_states_SuperFall = { .frame = sf_frame, .tick = sf_tick };

static xy_data_t g_charged_dash = { 100, 0, false, true, true, false, false, false };
static sid cd_frame(obj_t *o, state_t *s, int t)
{
    if (t != 6) return SID_NONE;
    if (s->d.fallback_state == by_name(F(o), "ChargeDashForward")) sm_queue_state(o, s->d.fallback_state, &g_charged_dash);
    else sm_queue_state(o, s->d.fallback_state, NULL);
    return SID_NONE;
}
const state_vt_t vt_characters_states_ChargeDash = { .frame = cd_frame };

static sid forfeit_enter(obj_t *o, state_t *s) { (void)s; F(o)->will_forfeit = true; return SID_NONE; }
const state_vt_t vt_characters_states_Forfeit = { .enter = forfeit_enter };
static sid fe_frame(obj_t *o, state_t *s, int t) { (void)s; if (t == 0) F(o)->hp = 0; return SID_NONE; }
const state_vt_t vt_characters_states_ForfeitExplosion = { .frame = fe_frame };

/* ---------- AirDash ---------- */

#define AD_BACK_LAG 4
#define AD_Y_MOD 39322          /* 0.60 */
#define AD_MAX_IASA 14
#define AD_COMBO_IASA 7
#define AD_NEUTRAL_MIN 9
static sid ad_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    if (t != 0) return SID_NONE;
    const xy_data_t *d = (const xy_data_t *)s->data;
    int dx = d ? d->x : 0, dy = d ? d->y : 0;
    fx spd = xp_fx(&s->d, "speed", 2 << 16);
    fxv force = xy_div(fxi(dx), fxi(dy), spd, fxi(100));
    fxv dir = xy_div(fxi(dx), fxi(dy), FX_ONE, fxi(100));
    int iasa;
    if (f->combo_count > 0) iasa = AD_COMBO_IASA;
    else { iasa = rnd(fx_mul(fx_vec_len(dir.x, dir.y), fxi(AD_MAX_IASA))); if (iasa < AD_NEUTRAL_MIN) iasa = AD_NEUTRAL_MIN; }
    s->cs.iasa_at = iasa;
    bool back = false;
    bool neg = force.x < 0;
    if (neg) { if (o->chara.facing > 0 && dx != 0) back = true; }
    else if (o->chara.facing < 0 && dx != 0) back = true;
    fighter_set_anim(f, s, back ? "DashBackward" : "DashForward");
    if (back && f->combo_count <= 0) {
        s->cs.backdash_iasa = 1; s->cs.beats_backdash = 0;
        o->hitlag_ticks += AD_BACK_LAG;
        fighter_add_penalty(f, 5, false);
    } else { s->cs.backdash_iasa = 0; s->cs.beats_backdash = 1; }
    phys_force(&o->chara, force.x, force.y < 0 ? fx_mul(force.y, AD_Y_MOD) : force.y);
    return SID_NONE;
}
static sid ad_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    phys_x_fric(&o->chara, xp_fx(&s->d, "fric", 3277));
    phys_apply_forces_no_limit(&o->chara);
    if (o->chara.grounded && f->combo_count <= 0 && fighter_opponent_dir(f) != fx_sign(o->chara.vx))
        o->chara.vx = fx_mul(o->chara.vx, 39322);       /* 0.6 */
    return SID_NONE;
}
const state_vt_t vt_characters_states_AirDash = { .frame = ad_frame, .tick = ad_tick };

/* ---------- RollStartup (RollDodge) ---------- */

#define R_GROUND_FRIC 7537      /* 0.115 */
#define R_AIR_FRIC 6554         /* 0.1 */
#define R_SPEED 557056          /* 8.5 */
#define R_ACCEL 45875           /* 0.7 */
static sid roll_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    const xy_data_t *d = (const xy_data_t *)s->data;
    int dx = d ? d->x : 1;
    s->ustate[0] = 0;
    f->used_air_dodge = true;
    fx fx_ = fx_mul(fxi(dx), R_SPEED);
    s->ufx[0] = fx_mul(fxi(dx), R_ACCEL);          /* accel */
    bool back = false;
    if (fx_ < 0) { if (o->chara.facing > 0) { back = true; fx_ = fx_mul(fx_, 19661); fighter_set_anim(f, s, "RollBackward"); } else fighter_set_anim(f, s, "RollForward"); }
    else { if (o->chara.facing < 0) { back = true; fx_ = fx_mul(fx_, 19661); fighter_set_anim(f, s, "RollBackward"); } else fighter_set_anim(f, s, "RollForward"); }
    s->d.anim_length = back ? 20 : 19; s->cs.iasa_at = back ? 20 : 19;
    phys_force(&o->chara, fx_, 0);
    o->throw_invulnerable = true; o->projectile_invulnerable = true; o->roll_projectile_invulnerable = true;
    return SID_NONE;
}
static sid roll_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    const xy_data_t *d = (const xy_data_t *)s->data;
    switch (t) {
    case 0:
        if (xp_int(&s->d, "tech", 0)) { if (!(d && d->no_invuln)) o->invulnerable = true; f->colliding_with_opponent = false; }
        if (!o->chara.grounded) { f->air_movements_left--; if (f->air_movements_left < 0) f->air_movements_left = 0; }
        break;
    case 1: f->colliding_with_opponent = false; break;
    case 2: if (!(d && d->no_invuln)) o->invulnerable = true; break;
    case 10: o->invulnerable = false; o->throw_invulnerable = false; o->projectile_invulnerable = false; o->roll_projectile_invulnerable = false; break;
    }
    return SID_NONE;
}
static sid roll_tick(obj_t *o, state_t *s)
{
    s->d.custom_x_fric = o->chara.grounded ? R_GROUND_FRIC : R_AIR_FRIC;
    F(o)->colliding_with_opponent = false;
    if (s->ufx[0]) phys_force(&o->chara, s->ufx[0], 0);
    return SID_NONE;
}
const state_vt_t vt_characters_states_RollStartup = { .enter = roll_enter, .frame = roll_frame, .tick = roll_tick };

/* ---------- Grab ---------- */

#define G_DASH_LAG 4
#define G_DASH_SPEED (14 << 16)
#define G_JUMP_HEIGHT (-(10 << 16))
static sid grab_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    s->ustate[0] = 0; s->ustate[1] = 0;            /* dash_lag, dash_momentum_applied */
    if (!s->data) {
        state_t *p = obj_prev_state(o);
        if (p && p->d.name && (strstr(p->d.name, "Grab") != NULL) && p->data) s->data = p->data;
    }
    const xy_data_t *d = (const xy_data_t *)s->data;
    if (d) {
        if (d->dash && !d->jump) s->ustate[0] = G_DASH_LAG;
        if (d->jump && s->cs.air_type == AIR_GROUNDED) return by_name(f, "JumpGrab");
        for (int i = 0; i < s->d.nhb; i++) {
            if (!s->d.hb[i].is_throwbox) continue;
            int st = -1;
            if (d->x == 0 && d->y == 1) st = xp_int(&s->d, "down_throw_state", -1);
            else if (d->x == o->chara.facing && d->y == 0) st = xp_int(&s->d, "forward_throw_state", -1);
            else if (d->x == -o->chara.facing && d->y == 0) st = xp_int(&s->d, "back_throw_state", -1);
            if (st >= 0) s->hbr[i].throw_state = st;
        }
    }
    if (xp_int(&s->d, "jump_grab", 0)) phys_force_rel(&o->chara, G_DASH_SPEED, G_JUMP_HEIGHT);
    return SID_NONE;
}
static sid grab_frame(obj_t *o, state_t *s, int t)
{
    (void)o;
    if (t == 0) s->cs.throw_techable = 1;
    if (t == 9) s->cs.throw_techable = 0;
    return SID_NONE;
}
static sid grab_tick(obj_t *o, state_t *s)
{
    phys_fric(&o->chara); if (o->gravity_enabled) obj_apply_grav(o); obj_apply_forces(o);
    if (s->ustate[0] > 0) { s->current_tick = 0; s->ustate[0]--; }
    const xy_data_t *d = (const xy_data_t *)s->data;
    if (s->current_tick == 4 && d && d->dash) { for (int i = 0; i < s->d.nhb; i++) if (s->d.hb[i].is_throwbox) hitbox_deactivate(&s->hbr[i]); }
    if (s->current_tick == 0 && d) {
        if (d->dash && !d->jump && !s->ustate[1]) phys_force_rel(&o->chara, G_DASH_SPEED, 0);
        if (!s->ustate[1]) s->ustate[1] = 1;
    }
    return SID_NONE;
}
const state_vt_t vt_characters_states_Grab = { .enter = grab_enter, .frame = grab_frame, .tick = grab_tick };

/* ---------- ThrowState ---------- */

static void throw_set_pos(fighter_t *f, int px, int py)
{
    f->throw_pos_x = px; f->throw_pos_y = py;
}
static void throw_opp_to_pos(fighter_t *f)
{
    obj_t *oo = &f->opponent->o;
    int x = phys_x(&f->o.chara) + f->throw_pos_x * f->o.chara.facing, y = phys_y(&f->o.chara) + f->throw_pos_y;
    oo->chara.px = fxi(x); oo->chara.py = fxi(y);
    obj_update_boxes(oo);
}
static void throw_update_pos(fighter_t *f, state_t *s)
{
    /* throw_positions por frame de sprite: TODO (gerador ainda nao exporta; vazio nos personagens atuais) */
    (void)f; (void)s;
}
static void throw_release(fighter_t *f, state_t *s)
{
    s->ustate[1] = 0;      /* throw = false */
    if (xp_int(&s->d, "use_release_throw_pos", 1)) throw_set_pos(f, xp_int(&s->d, "release_throw_pos_x", 0), xp_int(&s->d, "release_throw_pos_y", 0));
    else throw_update_pos(f, s);
    throw_opp_to_pos(f);
    fighter_update_facing(f->opponent);
    hitdata_t h; memset(&h, 0, sizeof h);
    h.hit_height = xp_int(&s->d, "hit_height", HEIGHT_MID);
    h.hitstun_ticks = xp_int(&s->d, "hitstun_ticks", 0);
    h.facing_int = f->o.chara.facing; h.box_facing = f->o.chara.facing; h.id = f->o.id;
    h.knockback = xp_fx(&s->d, "knockback", FX_ONE);
    h.damage = xp_int(&s->d, "damage", 10);
    h.dir_x = xp_fx(&s->d, "dir_x", FX_ONE); h.dir_y = xp_fx(&s->d, "dir_y", 0);
    h.disable_collision = xp_int(&s->d, "disable_collision", 1);
    h.knockdown = xp_int(&s->d, "knockdown", 1);
    h.aerial_hit_state = xp_int(&s->d, "aerial_hit_state", f->st_hurt_aerial);
    h.grounded_hit_state = xp_int(&s->d, "grounded_hit_state", f->st_hurt_grounded);
    h.ground_bounce = xp_int(&s->d, "ground_bounce", 1);
    h.hard_knockdown = xp_int(&s->d, "hard_knockdown", 0); h.force_grounded = xp_int(&s->d, "force_grounded", 0);
    h.wall_slam = xp_int(&s->d, "wall_slam", 0);
    h.pos_x = phys_x(&f->o.chara); h.pos_y = phys_y(&f->o.chara);
    h.knockdown_extends_hitstun = xp_int(&s->d, "knockdown_extends_hitstun", 1);
    h.hits_otg = xp_int(&s->d, "hits_otg", 0);
    h.combo_count = f->combo_count;
    h.screenshake_amount = xp_int(&s->d, "screenshake_amount", 0); h.screenshake_frames = xp_int(&s->d, "screenshake_frames", 0);
    h.sdi_modifier = FX_ONE; h.di_modifier = xp_fx(&s->d, "di_modifier", FX_ONE);
    h.meter_gain_modifier = FX_ONE; h.chip_damage_modifier = FX_ONE; h.block_pushback_modifier = FX_ONE;
    h.ground_bounce_knockback_modifier = FX_ONE; h.block_reverse_pushback_modifier = FX_ONE;
    h.increment_combo = xp_int(&s->d, "increment_combo", 1);
    h.damage_proration = xp_int(&s->d, "damage_proration", 0);
    h.parry_meter_gain = -1;
    h.damage_in_combo = xp_int(&s->d, "damage_in_combo", -1);
    h.hits_vs_dizzy = true; h.scale_combo = true; h.combo_scaling_amount = 1; h.hits_vs_standing = true;
    h.minimum_grounded_frames = xp_int(&s->d, "minimum_grounded_frames", -1);
    h.air_ground_bounce = xp_int(&s->d, "air_ground_bounce", 0);
    h.hits_projectiles = true; h.block_cancel_allowed = true; h.allowed_to_hit_own_team = true; h.parriable = true;
    h.host = &f->o; h.followup_state = -1;
    fighter_hit_by(f->opponent, &h, false);
}

static sid th_frame_shared(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    cs_frame_shared(o, s, t);
    if (t == 0) {
        sm_change_state(&f->opponent->o, f->opponent->st_grabbed, NULL, true, true);
        if (xp_int(&s->d, "use_start_throw_pos", 1)) throw_set_pos(f, xp_int(&s->d, "start_throw_pos_x", 0), xp_int(&s->d, "start_throw_pos_y", 0));
        else throw_update_pos(f, s);
        throw_opp_to_pos(f);
    }
    return SID_NONE;
}
static sid th_tick_shared(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (s->current_tick == 0) {
        s->ustate[1] = 1;
        if (xp_int(&s->d, "reverse", 0) && !s->d.force_same_direction_as_previous_state) { f->reverse_state = false; fighter_set_facing(f, -o->chara.facing, false); }
        o->invulnerable = true;
        s->ustate[0] = 0;       /* released */
    }
    sid r = cs_tick_shared(o, s);
    if (!s->ustate[0] && xp_int(&s->d, "release", 0) && s->current_tick + 1 == xp_int(&s->d, "release_frame", -1)) { throw_release(f, s); s->ustate[0] = 1; }
    if (!s->ustate[0]) { f->opponent->colliding_with_opponent = false; f->colliding_with_opponent = false; }
    throw_update_pos(f, s);
    return r;
}
static sid th_tick_after(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    cs_tick_after(o, s);
    if (!s->ustate[0]) throw_opp_to_pos(f);
    return SID_NONE;
}
static sid th_enter(obj_t *o, state_t *s) { (void)o; s->ustate[0] = 0; return SID_NONE; }
static void th_exit(obj_t *o, state_t *s) { (void)o; s->ustate[0] = 0; }
const state_vt_t vt_characters_states_ThrowState = {
    .enter = th_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit,
};

/* ---------- Frozen (sem material/shader) ---------- */

static sid fz_enter(obj_t *o, state_t *s)
{
    s->ustate[0] = 60; s->ustate[1] = 0; s->ustate[2] = 0;       /* duration, freeze_forces, can_act */
    const hitdata_t *h = (const hitdata_t *)s->data;
    if (h) { s->ustate[0] = h->hitstun_ticks; /* misc_data "FreezeForces": TODO exportar misc_data */ }
    if (s->ustate[1]) { o->chara.vx = 0; o->chara.vy = 0; }
    else { o->chara.vx = fx_mul(o->chara.vx, FX_HALF); o->chara.vy = fx_mul(o->chara.vy, FX_HALF); }
    return SID_NONE;
}
static sid fz_frame(obj_t *o, state_t *s, int t) { (void)s; if (t == 0) fighter_release_opponent(F(o)); return SID_NONE; }
static sid fz_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (s->ustate[1]) { o->chara.vx = 0; o->chara.vy = 0; }
    else { if (o->gravity_enabled) obj_apply_grav(o); phys_fric(&o->chara); obj_apply_forces(o); }
    phys_update_grounded(&o->chara);
    if (s->current_tick >= s->ustate[0]) {
        if (s->ustate[2]) return s->d.fallback_state;
        cs_enable_interrupt(f, s, true, false);
        s->ustate[2] = 1;
    }
    return SID_NONE;
}
const state_vt_t vt_characters_states_Frozen = { .enter = fz_enter, .frame = fz_frame, .tick = fz_tick };
