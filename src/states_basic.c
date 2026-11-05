/* Estados comuns basicos: Idle, Fall, Landing, Jump, Dash, DashStartup, Knockdown, Getup, WallSlam, Grabbed, ThrowTech, Taunt. */
#include "fighter.h"

static inline fx fxi(int v) { return FX_INT(v); }
static inline int rnd(fx v) { return (int)fx_round(v); }
static inline int isign(int v) { return (v > 0) - (v < 0); }
#define F(o) ((o)->fighter)
enum { AIR_GROUNDED, AIR_AERIAL, AIR_BOTH };
enum { BOUNCE_LEFT, BOUNCE_RIGHT, BOUNCE_NONE };

static sid by_name(fighter_t *f, const char *n) { return fighter_name_id(f, n); }
static const char *prev_name(obj_t *o) { state_t *p = obj_prev_state(o); return p ? p->d.name : ""; }

/* ---------- Idle / Fall / Landing ---------- */

static sid idle_enter(obj_t *o, state_t *s)
{
    if (xp_int(&s->d, "auto_fall", 1) && !o->chara.grounded) return by_name(F(o), "Fall");
    return SID_NONE;
}
static sid idle_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    phys_fric(&o->chara); obj_apply_forces(o);
    if (xp_int(&s->d, "auto_fall", 1) && !o->chara.grounded) return by_name(f, "Fall");
    if (f->hp <= 0) return f->st_knockdown;
    return SID_NONE;
}
const state_vt_t vt_characters_states_Idle = { .enter = idle_enter, .tick = idle_tick };

static sid fall_tick(obj_t *o, state_t *s)
{
    obj_apply_grav(o); obj_apply_forces(o);
    if ((s->current_tick > 3 || strcmp(prev_name(o), "Fall") == 0) && o->chara.grounded) return by_name(F(o), "Landing");
    return SID_NONE;
}
const state_vt_t vt_characters_states_Fall = { .tick = fall_tick };

#define LAND_DEFAULT_LAG 4
#define LAND_MAX_EXTRA 5
static sid land_frame(obj_t *o, state_t *s, int t)
{
    if (t != 0) return SID_NONE;
    fighter_t *f = F(o);
    int lag = LAND_DEFAULT_LAG;
    if (s->data) lag = (int)(intptr_t)s->data;
    state_t *prev = obj_prev_state(o);
    if (prev && f->combo_count <= 0 && f->opponent->combo_count <= 0) {
        int extra = LAND_MAX_EXTRA - prev->current_tick; if (extra < 0) extra = 0;
        lag += extra;
    }
    s->d.anim_length = lag; s->cs.iasa_at = lag - 1; s->ustate[0] = lag;
    return SID_NONE;
}
static sid land_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (s->current_tick > 4) phys_fric(&o->chara);
    obj_apply_forces(o);
    s->cs.interruptible_on_opponent_turn = f->opponent->combo_count <= 0 && s->ustate[0] <= LAND_DEFAULT_LAG;
    return SID_NONE;
}
const state_vt_t vt_characters_states_Landing = { .frame = land_frame, .tick = land_tick };

/* ---------- Knockdown / Getup / WallSlam ---------- */

static sid kd_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t != 0) return SID_NONE;
    fighter_t *f = F(o);
    o->chara.vy = 0; o->chara.grounded = true; o->chara.py = 0;
    f->on_the_ground = true; f->colliding_with_opponent = false;
    return SID_NONE;
}
static void kd_exit(obj_t *o, state_t *s) { (void)s; F(o)->on_the_ground = false; F(o)->colliding_with_opponent = true; }
static sid kd_tick(obj_t *o, state_t *s)
{
    phys_x_fric(&o->chara, 8192);       /* 0.125 */
    phys_apply_forces_no_limit(&o->chara);
    if (F(o)->hp <= 0) s->d.endless = 1;
    return SID_NONE;
}
const state_vt_t vt_characters_states_Knockdown = { .frame = kd_frame, .tick = kd_tick, .exit = kd_exit };

static sid getup_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t == 0) { o->invulnerable = true; F(o)->colliding_with_opponent = true; }
    return SID_NONE;
}
static void getup_exit(obj_t *o, state_t *s) { (void)s; F(o)->wakeup_throw_immunity_ticks = WAKEUP_THROW_IMMUNITY_TICKS; }
const state_vt_t vt_characters_states_Getup = { .frame = getup_frame, .exit = getup_exit };

#define WS_GRAV 16384          /* 0.25 */
#define WS_FALL (5 << 16)
#define WS_GRAV_PER 13107      /* 0.2 */
#define WS_FALL_PER 32768      /* 0.5 */
#define WS_MIN_DURATION 20
#define WS_MIN_HEIGHT (-30)
#define WS_DI_EFFECT (10 << 16)

static sid ws_enter(obj_t *o, state_t *s) { (void)s; F(o)->clipping_wall = true; F(o)->colliding_with_opponent = false; return SID_NONE; }
static sid ws_frame(obj_t *o, state_t *s, int t)
{
    if (t != 0) return SID_NONE;
    fighter_t *f = F(o);
    int dir = (int)(intptr_t)s->data == BOUNCE_LEFT ? 1 : -1;
    s->ustate[0] = dir; s->ustate[1] = 1;
    fxv di = fighter_get_scaled_di(f, f->current_di);
    int ypos = phys_y(&o->chara); if (ypos > WS_MIN_HEIGHT) ypos = WS_MIN_HEIGHT;
    ypos += rnd(fx_mul(di.y, WS_DI_EFFECT));
    if (f->wall_slams == 0) f->combo_proration += 1;
    f->wall_slams++;
    o->chara.px = fxi(f->stage_width * -dir); o->chara.py = fxi(ypos);
    obj_update_boxes(o);
    return SID_NONE;
}
static sid ws_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    int dir = s->ustate[0];
    if (s->current_tick > (WS_MIN_DURATION - (f->wall_slams - 1) * 8) + (!o->chara.grounded ? 5 : 0)) {
        if (o->chara.grounded) return f->st_knockdown;
        cs_enable_interrupt(f, s, true, false);
        sm_queue_state(o, by_name(f, "Fall"), NULL);
    }
    if (s->ustate[1]) { o->chara.px = fxi(f->stage_width * -dir); fighter_set_facing(f, dir, false); }
    fx grav = WS_GRAV + (fx)(f->wall_slams - 1) * WS_GRAV_PER;
    fx fall = WS_FALL + (fx)(f->wall_slams - 1) * WS_FALL_PER;
    phys_grav_custom(&o->chara, grav, fall);
    obj_apply_forces(o);
    if (s->current_tick > 30) { cs_enable_interrupt(f, s, true, false); return by_name(f, "Fall"); }
    return SID_NONE;
}
const state_vt_t vt_characters_states_WallSlam = { .enter = ws_enter, .frame = ws_frame, .tick = ws_tick };

/* ---------- Grabbed / ThrowTech / Taunt ---------- */

static sid grabbed_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    o->chara.snap = false;
    f->colliding_with_opponent = false; f->opponent->colliding_with_opponent = false;
    o->invulnerable = true;
    if (f->opponent->o.sm.cur->id == f->st_grabbed) {
        sid w = by_name(f, "Wait");
        sm_queue_state(o, w, NULL); sm_queue_state(&f->opponent->o, w, NULL);
    }
    (void)s;
    return SID_NONE;
}
static void grabbed_exit(obj_t *o, state_t *s) { (void)s; o->chara.snap = true; }
const state_vt_t vt_characters_states_Grabbed = { .enter = grabbed_enter, .exit = grabbed_exit };

static sid tech_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    fighter_t *f = F(o);
    if (t == 0) { f->reverse_state = false; o->invulnerable = true; fighter_set_facing(f, fighter_opponent_dir(f), false); }
    else if (t == 1) { phys_move(&o->chara, fxi(-10 * o->chara.facing), 0); phys_force_rel(&o->chara, -(8 << 16), 0); }
    return SID_NONE;
}
static sid tech_tick(obj_t *o, state_t *s) { (void)s; obj_apply_grav(o); phys_fric(&o->chara); obj_apply_forces(o); return SID_NONE; }
const state_vt_t vt_characters_states_ThrowTech = { .frame = tech_frame, .tick = tech_tick };

static sid taunt_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t == 44) fighter_gain_super_meter_raw(F(o), MAX_SUPER_METER);
    return SID_NONE;
}
const state_vt_t vt_characters_states_Taunt = { .frame = taunt_frame };

/* ---------- Jump ---------- */

#define J_FULL_HOP_LEN 45875        /* 0.7 */
#define J_SUPER_SPEED 1114112       /* 17.0 */
#define J_BASE_SPEED 32768          /* 0.5 */
#define J_GLOBAL 55706              /* 0.85 */
#define J_SH_IASA 9
#define J_SH_COMBO_IASA 7
#define J_FH_IASA 14
#define J_SUPER_FORCES_END 25
enum { JX, JY, JSQUAT, JTICK, JFSIGN, JQBD };   /* ustate */

static bool jsuper(const state_t *s) { return xp_int(&s->d, "super_jump", 0) != 0; }
static bool jback(const state_t *s, int facing) { return isign(s->ustate[JX]) != facing || s->ustate[JX] == 0; }

static bool jump_is_squat(fighter_t *f, const state_t *s)
{
    fx len = fx_vec_len(fx_div(fxi(s->ustate[JX]), fxi(100)), fx_div(fxi(s->ustate[JY]), fxi(100)));
    bool full = len > J_FULL_HOP_LEN;
    return jsuper(s) || (s->cs.air_type == AIR_GROUNDED && (jback(s, f->o.chara.facing) || f->combo_count <= 0) && full);
}

static void jump_homing(fighter_t *f, state_t *s)
{
    int dx = phys_x(&f->opponent->o.chara) - phys_x(&f->o.chara), dy = phys_y(&f->opponent->o.chara) - phys_y(&f->o.chara);
    fxv d = fxv_norm(fxv_make(fxi(dx), fxi(dy)));
    if (d.y > -22282) { d.y = -22282; d.x = fx_mul(fxi(f->o.chara.facing), 61604); d = fxv_norm(d); }
    else d = fxv_norm(d);
    s->ustate[JX] = rnd(fx_mul(d.x, fxi(100))); s->ustate[JY] = rnd(fx_mul(d.y, fxi(100)));
}

static void jump_load_data(fighter_t *f, state_t *s)
{
    if (!s->data) jump_homing(f, s);
    else { const xy_data_t *d = (const xy_data_t *)s->data; s->ustate[JX] = d->x; s->ustate[JY] = d->y; }
}

static void jump_do(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    o->throw_invulnerable = false;
    o->chara.grounded = false;
    o->chara.vx = fx_mul(o->chara.vx, xp_fx(&s->d, "x_speed_preserved", 16384));
    o->chara.vy = 0;
    fxv force = obj_xy_to_dir(fxi(s->ustate[JX]), fxi(s->ustate[JY]), FX_ONE);
    fx len = fx_vec_len(force.x, force.y);
    fx pw = fx_powu(len, 2);
    fxv fp = fxv_make(fx_mul(force.x, pw), fx_mul(force.y, pw));
    force = fxv_make(fx_div(fp.x + force.x, fxi(2)), fx_div(fp.y + force.y, fxi(2)));
    fx spd = jsuper(s) ? J_SUPER_SPEED : xp_fx(&s->d, "speed", 25 << 16) + J_BASE_SPEED;
    force = fxv_make(fx_mul(force.x, spd), fx_mul(force.y, spd));
    force.y = fx_mul(force.y, xp_fx(&s->d, "y_modifier", 98304));
    force.x = fx_mul(force.x, xp_fx(&s->d, "x_modifier", FX_ONE));
    if ((f->combo_count <= 0 || f->opponent->on_the_ground) && !jsuper(s)) force.y = fx_mul(force.y, J_GLOBAL);
    phys_force(&o->chara, force.x, force.y);
    s->ustate[JFSIGN] = fx_sign(force.x);
    s->cs.interruptible_on_opponent_turn = 1;
    s->cs.next_state_on_hold = 1; s->cs.next_state_on_hold_on_opponent_turn = 1;
}

static sid jump_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    fighter_set_anim(f, s, "Landing");
    jump_load_data(f, s);
    s->ustate[JSQUAT] = jump_is_squat(f, s);
    if (!s->ustate[JSQUAT]) o->throw_invulnerable = true;
    return SID_NONE;
}

static sid jump_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    if (t == 4) { if (s->ustate[JSQUAT] && !jsuper(s)) jump_do(o, s); return SID_NONE; }
    if (t == 7) { if (jsuper(s)) jump_do(o, s); return SID_NONE; }
    if (t == 1) {
        if (s->ustate[JQBD]) {
            s->ustate[JQBD] = 0;
            if (jback(s, o->chara.facing)) return SID_NONE;
            const state_t *os = f->opponent->o.sm.cur;
            if (os->d.nxp == 0 && !os->data) return SID_NONE;
            int ox = os->data ? ((const xy_data_t *)os->data)->x : 0;
            bool oback = isign(ox) == o->chara.facing || ox == 0;
            s->cs.beats_backdash = oback;
            if (!s->cs.beats_backdash) {
                fx len = fx_div(fx_vec_len(fxi(s->ustate[JX]), fxi(s->ustate[JY])), fxi(100));
                bool full = len > J_FULL_HOP_LEN;
                /* original usa (x,x) do oponente por engano; replicado: len = |ox|*sqrt2 */
                fx olen = fx_div(fx_vec_len(fxi(ox), fxi(ox)), fxi(100));
                bool ofull = olen > J_FULL_HOP_LEN;
                bool osuper = os->d.nxp ? xp_int(&os->d, "super_jump", 0) != 0 : false;
                s->cs.beats_backdash = (full == (ofull || !osuper));
            }
        }
        return SID_NONE;
    }
    if (t != 0) return SID_NONE;
    jump_load_data(f, s);
    if (!jsuper(s)) s->cs.interruptible_on_opponent_turn = 1;
    s->cs.next_state_on_hold = 0; s->cs.next_state_on_hold_on_opponent_turn = 0;
    s->ustate[JQBD] = 0;
    fx len = fx_vec_len(fx_div(fxi(s->ustate[JX]), fxi(100)), fx_div(fxi(s->ustate[JY]), fxi(100)));
    bool full = len > J_FULL_HOP_LEN;
    bool back = jback(s, o->chara.facing);
    bool squat = jump_is_squat(f, s);
    s->ustate[JSQUAT] = squat;
    if (squat && !back) s->current_tick = 3;
    if (back && f->combo_count <= 0) {
        fighter_add_penalty(f, full ? xp_int(&s->d, "back_full_hop_sadness", 10) : xp_int(&s->d, "back_short_hop_sadness", 5), false);
        s->cs.backdash_iasa = 1; s->cs.beats_backdash = 0;
    } else {
        s->cs.backdash_iasa = 0;
        const state_t *os = f->opponent->o.sm.cur;
        s->cs.beats_backdash = !(os->d.cs && os->cs.beats_backdash);
        const char *n = os->d.name;
        if (!strcmp(n, "Jump") || !strcmp(n, "DoubleJump") || !strcmp(n, "SuperJump")) s->ustate[JQBD] = 1;
    }
    if (!squat) { s->ustate[JTICK] = 1; jump_do(o, s); }
    else { s->ustate[JTICK] = jsuper(s) ? 7 : 4; fighter_set_anim(f, s, "Landing"); }
    if (!jsuper(s)) {
        int16_t *a = s->ifr;
        for (int i = 0; i < 2; i++) a[i] = (i < s->def->cs->ninterrupt_frames) ? s->def->cs->interrupt_frames[i] : 0;
        if (squat) { a[0] = 14; a[1] = back ? 25 : 24; if (!back) a[1] = 24; }
        else if (full) { a[0] = 10; a[1] = 21; }
        else { a[0] = f->combo_count <= 0 ? J_SH_IASA : J_SH_COMBO_IASA; a[1] = 18; }
        s->cs.interrupt_frames = a; s->cs.ninterrupt_frames = 2;
    }
    s->d.sfx_tick = s->ustate[JTICK];
    return SID_NONE;
}

static sid jump_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    int jt = s->ustate[JTICK];
    if (s->current_tick >= jt) {
        bool left = o->chara.facing < 0;
        bool jb;
        if (s->ustate[JFSIGN] < 0) jb = !left && s->ustate[JX] != 0; else jb = left && s->ustate[JX] != 0;
        if (jb) fighter_set_anim(f, s, "JumpBack"); else s->anim = s->def->sprite_anim;
        if (xp_int(&s->d, "fall_anim", 0) && o->chara.vy > xp_fx(&s->d, "fall_anim_speed", FX_ONE)) fighter_set_anim(f, s, "Fall");
        if (o->gravity_enabled) obj_apply_grav(o);
        if (!jsuper(s) || o->chara.vy > 0 || s->current_tick > J_SUPER_FORCES_END) obj_apply_forces(o);
        else phys_apply_forces_no_limit(&o->chara);
    }
    if (s->current_tick > jt && o->chara.grounded) return by_name(f, "Landing");
    return SID_NONE;
}
const state_vt_t vt_characters_states_Jump = { .enter = jump_enter, .frame = jump_frame, .tick = jump_tick };

/* ---------- DashStartup / Dash ---------- */

static sid ds_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    const xy_data_t *d = (const xy_data_t *)s->data;
    int x = d ? d->x : 0;
    return x * fighter_opponent_dir(f) < 0 ? by_name(f, "DashBackward") : by_name(f, "DashForward");
}
const state_vt_t vt_characters_states_DashStartup = { .enter = ds_enter };

#define D_MIN_IASA 7
#define D_MIN_NEUTRAL_IASA 9
#define D_MAX_IASA 14
#define D_MIN_RATIO 32768
enum { DUPD, DCHG, DAUTO };       /* ustate; ufx[0]=dist_ratio, ufx[1]=max_ratio */

static sid dash_enter(obj_t *o, state_t *s)
{
    (void)o;
    s->ustate[DUPD] = 0; s->ustate[DCHG] = 0;
    const xy_data_t *d = (const xy_data_t *)s->data;
    s->ustate[DAUTO] = d ? d->auto_correct : 0;
    s->ufx[0] = FX_ONE; s->ufx[1] = 81920;
    return SID_NONE;
}

static sid dash_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    int dir_x = xp_int(&s->d, "dir_x", 1);
    if (t == 0) { if (dir_x < 0) o->throw_invulnerable = true; return SID_NONE; }
    if (t == 7) { if (dir_x < 0) o->throw_invulnerable = false; return SID_NONE; }
    if (t != 1) return SID_NONE;
    xy_data_t *d = (xy_data_t *)s->data;
    if (dir_x < 0) {
        s->ufx[1] = FX_ONE;
        fighter_add_penalty(f, xp_int(&s->d, "back_penalty", 5), false);
        phys_reset_momentum(&o->chara);
    } else {
        s->ufx[1] = 81920;
        s->cs.beats_backdash = 1;
        s->ufx[0] = fx_div(fxi(d ? d->x : 100), fxi(100));
        int min_iasa = f->combo_count > 0 ? D_MIN_IASA : D_MIN_NEUTRAL_IASA;
        int iasa;
        if (!s->ustate[DCHG] && f->combo_count > 0) iasa = min_iasa;
        else { iasa = rnd(fx_mul(s->ufx[0], fxi(D_MAX_IASA - min_iasa)) + fxi(min_iasa)); if (iasa < 1) iasa = 1; }
        s->cs.iasa_at = iasa;
        if (s->def->cs) s->cs.velocity_forward_meter_gain_multiplier = fx_mul(s->def->cs->velocity_forward_meter_gain_multiplier, s->ufx[0]);
    }
    if (xp_int(&s->d, "startup_lag", 0) != 0) return SID_NONE;
    fx dsstr = xp_fx(&s->d, "dash_speed_string", 0);
    fx force = dsstr == 0 ? fxi(dir_x * xp_int(&s->d, "dash_speed", 100)) : fx_mul(fxi(dir_x), dsstr);
    if (strcmp(prev_name(o), "ChargeDash") == 0 || (d && d->has_charged)) {
        if (dir_x >= 0) {
            s->ustate[DCHG] = 1;
            if (d && d->has_charged) d->charged = true;
            force = fx_mul(force, fxi(2));
        }
    }
    fx ratio = fx_mul(s->ufx[0], s->ufx[1] - D_MIN_RATIO) + D_MIN_RATIO;
    phys_force_rel(&o->chara, fx_mul(force, ratio), 0);
    if (!xp_int(&s->d, "air", 0) && o->gravity_enabled) obj_apply_grav(o);
    return SID_NONE;
}

static sid dash_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    phys_x_fric(&o->chara, xp_fx(&s->d, "fric", 3277));
    if (!xp_int(&s->d, "air", 0) && o->gravity_enabled) obj_apply_grav(o);
    if (s->ustate[DCHG]) phys_apply_forces_no_limit(&o->chara); else obj_apply_forces(o);
    fx lim = xp_fx(&s->d, "speed_limit", 40 << 16);
    if (fx_vec_len(o->chara.vx, o->chara.vy) > lim) {
        fxv v = fxv_norm_times(fxv_make(o->chara.vx, o->chara.vy), lim);
        o->chara.vx = v.x; o->chara.vy = v.y;
    }
    fighter_t *op = f->opponent;
    if (s->ustate[DAUTO] && xp_int(&s->d, "dir_x", 1) > 0 && op->colliding_with_opponent && !fighter_is_in_hurt_state(op, true) && s->current_tick % 4 == 0) {
        if (o->chara.vx != 0 && fx_sign(o->chara.vx) != fighter_opponent_dir(f)) {
            fighter_update_facing(f);
            s->ustate[DUPD] = 1;
            o->chara.vx = fx_mul(fx_abs(o->chara.vx), fxi(fighter_opponent_dir(f)));
        }
    }
    return SID_NONE;
}
const state_vt_t vt_characters_states_Dash = { .enter = dash_enter, .frame = dash_frame, .tick = dash_tick };
