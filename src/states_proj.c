/* Estados de projeteis: Default.gd (DefaultFireball), ShurikenDefault.gd, DirProjectileDefault.gd. */
#include "projectile.h"

static inline fx fxi(int v) { return FX_INT(v); }
static inline int isign(fx v) { return (v > 0) - (v < 0); }

static inline void move_directly(obj_t *o, fx x, fx y) { phys_move(&o->chara, x, y); }
static inline void move_directly_rel(obj_t *o, fx x, fx y) { move_directly(o, x * o->chara.facing, y); }

enum { U_HIT = 0, U_HIT_TICK = 1, U_NUM_HITS = 2 };

void proj_fizzle(obj_t *o, state_t *s)
{
    s->ustate[U_HIT] = 1;
    proj_disable(o);
    proj_terminate_hitboxes(o);
    s->ustate[U_HIT_TICK] = s->current_tick;
}

void proj_kill_hurtbox(obj_t *o) { o->hurtbox.w = 0; o->hurtbox.h = 0; }

sid proj_enter(obj_t *o, state_t *s) { (void)o; s->ustate[U_NUM_HITS] = xp_int(&s->d, "num_hits", 1); return SID_NONE; }

sid proj_frame0(obj_t *o, state_t *s, int t)
{
    if (t != 0) return SID_NONE;
    s->ustate[U_HIT] = 0; s->ustate[U_HIT_TICK] = 0;
    o->chara.grounded = false;
    return SID_NONE;
}

void proj_on_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)hb;
    bool clash = xp_int(&s->d, "clash", 1);
    if (clash || (xp_int(&s->d, "fizzle_on_hit_opponent", 0) && victim->is_fighter)) {
        if (obj_is_proj(victim) && !PROJ(victim)->pj.deletes_other_projectiles) return;
        if (--s->ustate[U_NUM_HITS] == 0) proj_fizzle(o, s);
    }
}

/* ---- Default.gd ---- */
void default_move(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    (void)p;
    fx mxs = xp_fx(&s->d, "move_x_string", 0), mys = xp_fx(&s->d, "move_y_string", 0);
    if (mxs > 0 || mys > 0) move_directly_rel(o, mxs, mys);
    else move_directly_rel(o, fxi(xp_int(&s->d, "move_x", 4)), fxi(xp_int(&s->d, "move_y", 0)));
}

sid default_tick_common(obj_t *o, state_t *s, void (*move)(obj_t *, state_t *))
{
    int x = phys_x(&o->chara);
    int sw = PROJ(o)->owner ? PROJ(o)->owner->stage_width : 1000;
    o->chara.grounded = o->chara.py >= 0;
    if (!s->ustate[U_HIT] && ((o->chara.grounded && xp_int(&s->d, "fizzle_on_ground", 1)) ||
                              (xp_int(&s->d, "fizzle_on_walls", 1) && (x <= -sw || x >= sw)))) {
        proj_fizzle(o, s); proj_kill_hurtbox(o);
    }
    if (s->current_tick >= xp_int(&s->d, "lifetime", 999999)) proj_fizzle(o, s);
    else if (!s->ustate[U_HIT]) move(o, s);
    if (xp_int(&s->d, "follow_creator", 0) && PROJ(o)->creator)
        phys_set_pos(&o->chara, PROJ(o)->creator->chara.px, PROJ(o)->creator->chara.py);
    return SID_NONE;
}
static sid default_tick(obj_t *o, state_t *s) { return default_tick_common(o, s, default_move); }
const state_vt_t vt_characters_stickman_projectiles_fireball_states_Default = { .enter = proj_enter, .frame = proj_frame0, .tick = default_tick, .hit_something = proj_on_hit };

/* ---- ShurikenDefault.gd: move() usa dir_x/dir_y * speed_modifier ---- */
static void shuriken_move(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    if (!p->sdata.speed.has) { default_move(o, s); return; }
    fxv d = fxv_mul(fxv_make(p->dir_x, p->dir_y), p->sdata.speed.sm);
    move_directly_rel(o, d.x, d.y);
}
void ninja_shuriken_stack(obj_t *host, state_t *s);
static sid shuriken_tick(obj_t *o, state_t *s) { default_tick_common(o, s, shuriken_move); ninja_shuriken_stack(o, s); return SID_NONE; }
const state_vt_t vt_characters_stickman_projectiles_fireball_states_ShurikenDefault = { .enter = proj_enter, .frame = proj_frame0, .tick = shuriken_tick, .hit_something = proj_on_hit };

/* ---- DirProjectileDefault.gd (sem homing por enquanto) ---- */
static sid dir_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    int x = phys_x(&o->chara);
    int sw = p->owner ? p->owner->stage_width : 1000;
    o->chara.grounded = o->chara.py >= 0;
    if ((xp_int(&s->d, "fizzle_on_ground", 1) && s->current_tick > 1 && !s->ustate[U_HIT] && o->chara.grounded) || x <= -sw || x >= sw) {
        proj_fizzle(o, s); proj_kill_hurtbox(o);
    }
    if (o->chara.vy != 0) s->ufx[0] = o->chara.vy;          /* last_y_vel */
    if (o->chara.grounded && xp_int(&s->d, "bounce_on_ground", 0)) {
        o->chara.grounded = false;
        move_directly(o, 0, fxi(-1));
        o->chara.vy = fx_mul(s->ufx[0] < 0 ? -s->ufx[0] : s->ufx[0], -(fx)49152 /* 0.75 */);
        if (--s->ustate[3] < 0) proj_fizzle(o, s);
    }
    if (s->current_tick > xp_int(&s->d, "lifetime", 99999)) { proj_fizzle(o, s); proj_kill_hurtbox(o); }
    else if (!s->ustate[U_HIT]) {
        fxv dir = p->sdata.dir.dir;
        fx dx = xp_int(&s->d, "relative_data_dir", 0) ? dir.x * o->chara.facing : dir.x;
        fxv mv = fxv_norm_times(fxv_make(dx, dir.y), xp_fx(&s->d, "move_speed", 15 * FX_ONE));
        move_directly(o, mv.x, mv.y);
        o->chara.facing = isign(dx) ? isign(dx) : 1;
    }
    return SID_NONE;
}
static sid dir_enter(obj_t *o, state_t *s) { proj_enter(o, s); s->ustate[3] = xp_int(&s->d, "num_bounces", 2); return SID_NONE; }
static sid dir_frame0(obj_t *o, state_t *s, int t)
{
    if (t != 0) return SID_NONE;
    proj_frame0(o, s, 0);
    proj_t *p = PROJ(o);
    if (xp_int(&s->d, "homing", 0) && xp_int(&s->d, "start_homing", 0)) {
        fxv mv = fxv_mul(p->sdata.dir.dir, xp_fx(&s->d, "move_speed", 15 * FX_ONE));
        phys_force(&o->chara, mv.x, mv.y);
    }
    return SID_NONE;
}
static void dir_got_parried(obj_t *o, state_t *s) { if (xp_int(&s->d, "homing", 0)) proj_fizzle(o, s); }
const state_vt_t vt_projectile_DirProjectileDefault = { .enter = dir_enter, .frame = dir_frame0, .tick = dir_tick, .hit_something = proj_on_hit, .got_parried = dir_got_parried };
