/* Projeteis do Robo: RobotGrenade, Missile/MissileArc, LandingShockwave, GroundFire, OrbitalStrike. */
#include "robo.h"
#include "states.h"
#include <string.h>
#include <stdlib.h>

static inline fx fxi(int v) { return FX_INT(v); }
extern const chardata_t chardata_robo_NadeExplosion, chardata_robo_MissileExplosion, chardata_robo_GroundFire, chardata_robo_GroundFireLoic;
static sid cd_sid(obj_t *o, const char *n) { return fighter_name_id_cd(PROJ(o)->cd, n); }
static fx approach(fx v, fx target, fx amt) { if (v < target) { v += amt; if (v > target) v = target; } else if (v > target) { v -= amt; if (v < target) v = target; } return v; }
static void hb_hit_append(obj_t *o, hitbox_t *h, int uid) { (void)o; if (h) hitbox_save_hit(h, uid); }

/* ================= RobotGrenade ================= u: 0 ticks_left, 1 active, 2 hitbox_out, 3 hits_chained, 4 last_hit_by, 5 creator_hit_count, 6 grounded_frames
   uf: 0 last_vel_x, 1 last_vel_y, 2 base max_air_speed, 3 base max_fall_speed */
#define G_LIFETIME 900
#define G_ACTIVATE 30
enum { GU_TICKS, GU_ACTIVE, GU_OUT, GU_CHAIN, GU_LASTHIT, GU_CHC, GU_GROUNDED };
static const fx TIER_AIR[4] = { FXC(12), FXC(14.25), FXC(23.75), FXC(28.5) };
static const fx TIER_FALL[4] = { FXC(15), FXC(15), FXC(20), FXC(24) };

static void g_explode(obj_t *o)
{
    proj_disable(o);
    proj_spawn(eng_game_get(), &chardata_robo_NadeExplosion, o, 0, -8, true, NULL, 0, true);
}
void robo_grenade_activate(obj_t *o)
{
    proj_t *p = PROJ(o);
    if (p->u[GU_ACTIVE]) return;
    if (p->u[GU_TICKS] > G_ACTIVATE) p->u[GU_TICKS] = G_ACTIVATE;
    p->u[GU_ACTIVE] = 1;
    state_t *s = o->sm.cur;
    if (s->d.nhb > 0) s->hbr[0].sdi_modifier = 0;
    sid a = cd_sid(o, "Active");
    if (a >= 0 && o->sm.states[a].d.nhb > 0) o->sm.states[a].hbr[0].increment_combo = 0;
}
bool robo_grenade_active(obj_t *o) { return PROJ(o)->u[GU_ACTIVE]; }
void robo_grenade_refresh(obj_t *o)
{
    PROJ(o)->u[GU_OUT] = 0;
    sm_change_state(o, o->sm.cur->id, NULL, true, true);
}
static void g_init(obj_t *o)
{
    proj_t *p = PROJ(o);
    memset(p->u, 0, sizeof p->u);
    p->u[GU_TICKS] = G_LIFETIME; p->u[GU_LASTHIT] = -1;
    p->uf[2] = o->chara.max_aspeed; p->uf[3] = o->chara.max_fall;
}
static fx g_air_cap(proj_t *p)
{
    int tier = (p->u[GU_CHC] < 4 ? p->u[GU_CHC] : 4) - 1;
    return tier < 0 ? p->uf[2] : TIER_AIR[tier];
}
static void g_clear_boost(obj_t *o)
{
    proj_t *p = PROJ(o);
    p->u[GU_CHC] = 0; p->u[GU_GROUNDED] = 0;
    o->chara.max_aspeed = p->uf[2]; o->chara.max_fall = p->uf[3];
}
static void g_tick(obj_t *o)
{
    proj_t *p = PROJ(o);
    fighter_t *f = p->owner;
    if (o->hitlag_ticks <= 0) {
        p->u[GU_TICKS]--;
        if (p->u[GU_TICKS] == G_ACTIVATE - 3) p->pj.has_projectile_parry_window = false;
        bool grabbed = f && strcmp(f->opponent->o.sm.cur->d.name, "Grabbed") == 0;
        if (p->u[GU_TICKS] <= 0 && !grabbed) g_explode(o);
        else if (p->u[GU_TICKS] <= G_ACTIVATE) robo_grenade_activate(o);
    }
    if (p->u[GU_CHC] > 0) {
        if (o->chara.grounded) { if (++p->u[GU_GROUNDED] >= 60) g_clear_boost(o); }
        else p->u[GU_GROUNDED] = 0;
    }
    fx cap = g_air_cap(p);
    if (o->chara.vy < -cap) o->chara.vy = -cap;
}
static void g_hit_by(obj_t *o, hitbox_t *hb)
{
    proj_t *p = PROJ(o);
    phys_t *c = &o->chara;
    if (hb->def->is_throwbox) return;
    obj_t *host = hb->host;
    if (hb->def->hitbox_type == HB_FLIP) { c->vx = -c->vx; return; }
    phys_reset_momentum(c);
    fx dx = host ? hb->dir_x * host->chara.facing : hb->dir_x;
    bool by_creator = host && host->id == o->id;
    fxv dir = fxv_norm_times(fxv_make(dx, hb->dir_y), fx_mul(hb->knockback, FXC(1.5)));
    if (c->grounded && dir.y > 0) dir.y = -dir.y;
    sid act = cd_sid(o, "Active");
    if (act >= 0) sm_change_state(o, act, NULL, true, true);
    phys_force(c, dir.x, dir.y);
    fxv nudge = fxv_norm_times(fxv_make(dx, hb->dir_y), fxi(10));
    phys_move(c, nudge.x, nudge.y);
    hitbox_t *my = act >= 0 && o->sm.states[act].d.nhb > 0 ? &o->sm.states[act].hbr[0] : NULL;
    if (host) hb_hit_append(o, my, host->uid);
    if (by_creator) { if (p->u[GU_CHC] < 4) p->u[GU_CHC]++; p->u[GU_GROUNDED] = 0;
        int tier = p->u[GU_CHC] - 1; if (tier >= 0) { c->max_aspeed = TIER_AIR[tier]; c->max_fall = TIER_FALL[tier]; } }
    else if (p->u[GU_CHC] > 0) g_clear_boost(o);
    if (host) {
        fighter_t *pl = host->is_fighter ? host->fighter : (obj_is_proj(host) ? PROJ(host)->owner : NULL);
        if (pl) {
            if (p->u[GU_LASTHIT] != host->id || pl->combo_count > 0 || pl->opponent->combo_count > 0) { p->u[GU_LASTHIT] = host->id; p->u[GU_CHAIN] = 0; }
            else p->u[GU_CHAIN]++;
            p->pj.hit_cancel_on_hit = pl->combo_count > 0 && p->u[GU_CHAIN] == 0;
            if (host != &pl->o) hb_hit_append(o, my, pl->o.uid);
            else {
                fx infl = by_creator ? fxi(12) : fxi(5);
                fxv df = obj_xy_to_dir(pl->current_di.x, pl->current_di.y, infl);
                phys_force(c, fx_mul(df.x, FXC(0.85)), df.y);
            }
            if (p->u[GU_ACTIVE]) {
                if (host->id != o->id) { if (p->u[GU_TICKS] > 4) p->u[GU_TICKS] = 4; }
                else p->u[GU_TICKS] -= 5;
                if (p->u[GU_TICKS] < 0) p->u[GU_TICKS] = 0;
            }
        }
    }
}
static void g_on_got_blocked(obj_t *o, fighter_t *by)
{
    proj_t *p = PROJ(o); (void)by;
    if (p->u[GU_ACTIVE]) { if (p->u[GU_TICKS] > 4) p->u[GU_TICKS] = 4; }
    else o->chara.vx = fx_mul(o->chara.vx, FXC(-0.9));
    if (p->owner && ROBO(p->owner)->magnet_ticks_left > 0) { ROBO(p->owner)->magnetize_opponent = true; ROBO(p->owner)->magnetize_opponent_blocked = true; }
}
static void g_on_disable(obj_t *o)
{
    proj_t *p = PROJ(o);
    if (p->owner) { robo_t *r = ROBO(p->owner); r->grenade_uid = -1; r->magnetize_opponent = false; r->magnetize_opponent_blocked = false; }
}
const proj_vt_t vtp_characters_robo_projectiles_RobotGrenade = { .hit_by = g_hit_by, .init = g_init, .tick = g_tick, .on_got_blocked = g_on_got_blocked, .on_disable = g_on_disable };

static sid gd_frame(obj_t *o, state_t *s, int t)
{
    proj_t *p = PROJ(o);
    if (t == 1 && !p->u[GU_OUT] && s->d.nhb > 0) {
        hitbox_activate(&s->hbr[0]); p->u[GU_OUT] = 1;
        if (p->owner) hitbox_save_hit(&s->hbr[0], p->owner->o.uid);
    }
    return SID_NONE;
}
static sid gd_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o); phys_t *c = &o->chara;
    if (c->grounded && p->uf[1] > FXC(2.0)) { c->vy = fx_mul(p->uf[1], FXC(-0.7)); }
    fx speed = fx_vec_len(c->vx, c->vy);
    if (s->current_tick > 10 && speed < fxi(5) && strcmp(s->d.name, "Default") != 0) return cd_sid(o, "Default");
    int sw = p->owner ? p->owner->stage_width : 1000;
    int px = phys_x(c);
    if (abs(px) >= sw) c->vx = fx_mul(fx_abs(c->vx), fxi(-((px > 0) - (px < 0))));
    p->uf[0] = c->vx; p->uf[1] = c->vy;
    int sg = fx_sign(c->vx);
    c->facing = sg != 0 ? sg : c->facing;
    if (s->d.nhb > 0) { s->hbr[0].dir_x = fx_mul(c->vx, fxi(c->facing)); s->hbr[0].dir_y = c->vy; }
    return SID_NONE;
}
static void gd_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)s; (void)hb;
    proj_t *p = PROJ(o);
    if (victim->is_fighter && p->owner && ROBO(p->owner)->magnet_ticks_left > 0) { ROBO(p->owner)->magnetize_opponent = true; ROBO(p->owner)->magnetize_opponent_blocked = false; }
    if (p->u[GU_ACTIVE]) g_explode(o);
}
const state_vt_t vt_characters_robo_projectiles_states_GrenadeDefault = { .tick = gd_tick, .frame = gd_frame, .hit_something = gd_hit };

/* ================= NadeExplosion ================= */
static sid ne_frame(obj_t *o, state_t *s, int t)
{
    proj_t *p = PROJ(o);
    if (t == 0) {
        proj_frame0(o, s, t);
        if (p->owner && ROBO(p->owner)->nade_explosion_in_combo) {
            sid h = cd_sid(o, "HurtAerial");
            for (int i = 0; i < s->d.nhb && i < 2; i++) s->hbr[i].grounded_hit_state = h;
        }
    }
    return SID_NONE;
}
static void ne_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)s; (void)hb;
    proj_t *p = PROJ(o);
    if (victim->is_fighter && p->owner) ROBO(p->owner)->nade_explosion_in_combo = true;
}
static sid ne_tick(obj_t *o, state_t *s) { return default_tick_common(o, s, default_move); }
const state_vt_t vt_characters_robo_projectiles_states_NadeExplosionDefault = { .enter = proj_enter, .frame = ne_frame, .tick = ne_tick, .hit_something = ne_hit };

/* ================= Missile (root) ================= u: 0 arc_index, 1 is_up, 2 initial_facing */
static void ms_hit_by(obj_t *o, hitbox_t *hb)
{
    if (hb->def->is_throwbox) return;
    if (hb->host && hb->host->id == o->id) return;
    if (hb->host && hb->host->is_fighter) proj_disable(o);
}
static void ms_on_disable(obj_t *o)
{
    proj_t *p = PROJ(o);
    proj_t *e = proj_spawn(eng_game_get(), &chardata_robo_MissileExplosion, o, 0, 0, true, NULL, 0, true);
    if (e && p->creator) e->o.chara.facing = p->creator->chara.facing;
}
const proj_vt_t vtp_characters_robo_projectiles_Missile = { .hit_by = ms_hit_by, .on_disable = ms_on_disable };

/* MissileDefault (ufx 0 forward_force) */
static sid md_enter(obj_t *o, state_t *s) { proj_enter(o, s); s->ufx[0] = FXC(0.01); return SID_NONE; }
static sid md_tick(obj_t *o, state_t *s)
{
    default_tick_common(o, s, default_move);
    if (xp_int(&s->d, "flying", 0)) {
        phys_force_rel(&o->chara, s->ufx[0], o->chara.vy > FXC(-0.8) ? FXC(-0.3) : 0);
        s->ufx[0] += FXC(0.02);
    }
    return SID_NONE;
}
static void md_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    proj_on_hit(o, s, victim, hb);
    if (victim->is_fighter) proj_disable(o);
}
static void md_blocked(obj_t *o, state_t *s) { (void)s; proj_disable(o); }
const state_vt_t vt_characters_robo_projectiles_states_MissileDefault = { .enter = md_enter, .frame = proj_frame0, .tick = md_tick, .hit_something = md_hit, .got_blocked = md_blocked };

/* MissileArcDefault: ustate 4 phase, 5 ascent_ticks_total, 6 arc_ticks_total; ufx 0 ascent_angle, 1 descent_angle, 2 target_speed, 3 descent_force */
static void arc_set_angle(obj_t *o, state_t *s, fx angle)
{
    (void)o;
    if (s->d.nhb >= 2) {
        fxv f = fx_rotate_vec(fxv_make(fxi(5), 0), angle), b = fx_rotate_vec(fxv_make(fxi(-5), 0), angle);
        s->hbr[0].box.x = (int)fx_round(f.x); s->hbr[0].box.y = (int)fx_round(f.y);
        s->hbr[1].box.x = (int)fx_round(b.x); s->hbr[1].box.y = (int)fx_round(b.y);
    }
}
static void arc_update_rot(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    if (o->chara.vx == 0 && o->chara.vy == 0) return;
    fx lx = fx_mul(o->chara.vx, fxi(p->idata ? p->idata : 1));
    arc_set_angle(o, s, fx_vec_to_angle(lx, o->chara.vy));
}
static sid arc_frame(obj_t *o, state_t *s, int t)
{
    proj_t *p = PROJ(o);
    if (t != 0) return SID_NONE;
    proj_frame0(o, s, t);
    bool up = p->iy; int idx = p->ix;
    s->ufx[0] = up ? FXC(-1.371) + fx_mul(FXC(-0.2), fxi(idx)) : FXC(-1.4);
    s->ufx[1] = up ? FXC(1.5) : FXC(1.6) + fx_mul(FXC(-0.17), fxi(idx));
    s->ustate[5] = up ? 7 : 8; s->ustate[6] = up ? 10 : 2;
    s->ufx[2] = up ? fxi(10) : fxi(12);
    arc_set_angle(o, s, s->ufx[0]);
    if (!xp_int(&s->d, "flying", 0)) o->chara.grounded = false;
    return SID_NONE;
}
static void arc_before(obj_t *o, state_t *s) { if (xp_int(&s->d, "flying", 0) && s->ustate[4] == 1) obj_apply_grav(o); }
static sid arc_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    bool flying = xp_int(&s->d, "flying", 0), up = p->iy;
    default_tick_common(o, s, default_move);
    if (flying) {
        int ct = s->current_tick, face = p->idata ? p->idata : 1;
        if (s->ustate[4] == 0 && ct >= s->ustate[5]) s->ustate[4] = 1;
        else if (s->ustate[4] == 1 && ct >= s->ustate[5] + s->ustate[6]) { s->ustate[4] = 2; s->ufx[3] = FXC(0.05); }
        phys_t *c = &o->chara;
        if (s->ustate[4] == 0) {
            fxv d = fx_rotate_vec(fxv_make(FX_ONE, 0), s->ufx[0]);
            fx tx = fx_mul(fx_mul(d.x, s->ufx[2]), fxi(face)), ty = fx_mul(d.y, s->ufx[2]);
            c->vx = fx_lerp(c->vx, tx, FXC(0.35)); c->vy = fx_lerp(c->vy, ty, FXC(0.35));
        } else if (s->ustate[4] == 1) arc_update_rot(o, s);
        else {
            fxv d = fx_rotate_vec(fxv_make(FX_ONE, 0), s->ufx[1]);
            phys_force(c, fx_mul(fx_mul(d.x, s->ufx[3]), fxi(face)), fx_mul(d.y, s->ufx[3]));
            s->ufx[3] += FXC(0.05);
            arc_update_rot(o, s);
            if (c->vy > 0) p->pj.has_projectile_parry_window = false;
        }
        (void)up;
        if (c->py >= 0 && ct > 1) proj_disable(o);
    }
    return SID_NONE;
}
static void arc_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    proj_on_hit(o, s, victim, hb);
    if (victim->is_fighter) proj_disable(o);
}
const state_vt_t vt_characters_robo_projectiles_states_MissileArcDefault = { .enter = proj_enter, .frame = arc_frame, .tick_before = arc_before, .tick = arc_tick, .hit_something = arc_hit, .got_blocked = md_blocked };

/* ================= LandingShockwave: ix damage, force_x speed; ustate 4 start_x; ufx 0 move speed ================= */
static sid ls_enter(obj_t *o, state_t *s) { proj_enter(o, s); s->ustate[4] = phys_x(&o->chara); return SID_NONE; }
static void ls_move(obj_t *o, state_t *s) { phys_move_rel(&o->chara, s->ufx[0], 0); }
static sid ls_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    if (s->current_tick <= 1) { s->ufx[0] = fx_mul(p->force_x, FXC(1.25)); if (s->ufx[0] < fxi(6)) s->ufx[0] = fxi(6); }
    int dist = abs(phys_x(&o->chara) - s->ustate[4]);
    if (s->d.nhb > 0) {
        hitbox_t *h = &s->hbr[0];
        h->damage = p->ix - (int)fx_round(fx_mul(FXC(0.025), fxi(dist)));
        h->hitstun_ticks = 30 - (int)fx_round(fx_mul(FXC(0.05), fxi(dist)));
        if (h->hitstun_ticks < 10) h->hitstun_ticks = 10;
        if (h->damage < 0) proj_disable(o);
    }
    default_tick_common(o, s, ls_move);
    return SID_NONE;
}
static sid ls_frame(obj_t *o, state_t *s, int t) { if (t == 5) PROJ(o)->pj.has_projectile_parry_window = true; else proj_frame0(o, s, t); return SID_NONE; }
const state_vt_t vt_characters_robo_projectiles_states_LandingShockwaveDefault = { .enter = ls_enter, .frame = ls_frame, .tick = ls_tick, .hit_something = proj_on_hit };
const proj_vt_t vtp_characters_robo_projectiles_LandingShockwave = { 0 };

/* ================= GroundFire ================= */
extern const state_vt_t vt_characters_states_Jump, vt_characters_robo_SuperJump;
static bool is_jump_state(state_t *s) { return s->d.vt == &vt_characters_states_Jump || s->d.vt == &vt_characters_robo_SuperJump; }
static sid gf_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o); fighter_t *f = p->owner;
    bool loic = p->cd == &chardata_robo_GroundFireLoic;
    if (f && s->current_tick % 3 == 0) {
        robo_t *r = ROBO(f);
        fighter_t *op = f->opponent;
        int lx = abs(phys_x(&op->o.chara) - phys_x(&o->chara));
        if (lx < xp_int(&s->d, "width", 100)) {
            if (r->flame_touching_uid < 0) r->flame_touching_uid = o->uid;
            if (r->flame_touching_uid == o->uid && !op->o.invulnerable && op->o.chara.grounded && !is_jump_state(op->o.sm.cur))
                fighter_take_damage(op, 6, 0, FX_ONE, 0, FX_ONE, false, false);
        } else if (r->flame_touching_uid == o->uid) r->flame_touching_uid = -1;
    }
    if (s->current_tick > (loic ? 60 : 120)) {
        proj_disable(o);
        if (!loic && f) ROBO(f)->can_flamethrower = true;
    }
    o->chara.py = 0;
    return SID_NONE;
}
const state_vt_t vt_characters_robo_projectiles_states_GroundFire = { .tick = gf_tick };
static void gf_on_disable(obj_t *o) { proj_t *p = PROJ(o); if (p->owner && ROBO(p->owner)->flame_touching_uid == o->uid) ROBO(p->owner)->flame_touching_uid = -1; }
const proj_vt_t vtp_characters_robo_projectiles_GroundFire = { .on_disable = gf_on_disable };

/* ================= OrbitalStrike: ix aim_ticks, iy self_; u: 0 active, 1 deactivating ================= */
bool robo_orbital_active(obj_t *o) { return PROJ(o)->u[0]; }
bool robo_orbital_deactivating(obj_t *o) { return PROJ(o)->u[1]; }
bool robo_orbital_firing(obj_t *o) { return o->sm.cur && strcmp(o->sm.cur->d.name, "Fire") == 0; }
static void fire_deactivate(obj_t *o, state_t *s)
{
    s->current_tick = s->ustate[0];
    proj_terminate_hitboxes(o);
    PROJ(o)->u[1] = 1;
}
void robo_orbital_deactivate(obj_t *o)
{
    proj_t *p = PROJ(o);
    p->u[1] = 1;
    if (p->u[0]) fire_deactivate(o, o->sm.cur); else proj_disable(o);
}
static void os_tick(obj_t *o)
{
    state_t *s = o->sm.cur;
    if (strcmp(s->d.name, "Fire") == 0) {
        int w = 62 - (s->current_tick - s->ustate[0] > 0 ? s->current_tick - s->ustate[0] : 0) * 5;
        if (w <= 0) proj_disable(o);
    }
}
static void os_on_disable(obj_t *o)
{
    proj_t *p = PROJ(o);
    if (p->owner) { robo_t *r = ROBO(p->owner); r->loic_draining = false; r->orbital_uid = -1; r->orbital_strike_out = false; }
}
const proj_vt_t vtp_characters_robo_projectiles_OrbitalStrike = { .tick = os_tick, .on_disable = os_on_disable };

/* estado Default (orbital_strike_default): ustate 1 move_velocity */
static sid osd_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o); fighter_t *f = p->owner;
    if (f) {
        s->ustate[1] = (int)approach(fxi(s->ustate[1]), fxi(10 * ROBO(f)->loic_dir), fxi(1)) >> FX_SHIFT;
        phys_move(&o->chara, fxi(s->ustate[1]), 0);
        fx ratio = FX_ONE - fx_div(fxi(s->current_tick), fxi(p->ix > 0 ? p->ix : 1));
        ROBO(f)->loic_meter = (int)fx_round(fx_mul(fxi(1000), ratio));
    }
    if (s->current_tick > p->ix) return cd_sid(o, "Fire");
    return SID_NONE;
}
static sid osd_frame(obj_t *o, state_t *s, int t) { if (t == 0) { proj_frame0(o, s, t); phys_set_pos(&o->chara, fxi(phys_x(&o->chara)), 0); } return SID_NONE; }
const state_vt_t vt_characters_robo_projectiles_states_orbital_strike_default = { .enter = proj_enter, .frame = osd_frame, .tick = osd_tick };

/* estado Fire: ustate 0 active_time, 1 last_ground_fire, 2 move_velocity, 3 last_dir, 4 wiggle_penalty, 5 frames_this_dir */
static sid osf_enter(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    int at = 20 + ((p->ix - 14 > 0 ? p->ix - 14 : 0) * 6) / 7;
    s->ustate[0] = at;
    if (s->d.nhb > 0) s->hbr[0].active_ticks = at;
    return SID_NONE;
}
static sid osf_frame(obj_t *o, state_t *s, int t)
{
    proj_t *p = PROJ(o); fighter_t *f = p->owner;
    if (t == 0) {
        if (f) {
            robo_t *r = ROBO(f);
            r->loic_meter = 0; r->can_loic = false; r->loic_draining = false;
            int cx, cy; obj_hurt_center(&f->o, &cx, &cy);
            if (abs(cx - phys_x(&o->chara)) <= 30) robo_add_armor_pip(f);
        }
        s->ustate[1] = phys_x(&o->chara);
        p->u[0] = 1;
    } else if (t == 200) proj_disable(o);
    return SID_NONE;
}
static sid osf_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o); fighter_t *f = p->owner;
    bool active = true;
    int ct = s->current_tick;
    if (ct > 1 && ct < s->ustate[0]) {
        if (abs(phys_x(&o->chara) - s->ustate[1]) > 64) {
            const chardata_t *gf = &chardata_robo_GroundFire;
            proj_spawn(eng_game_get(), gf, o, 0, 0, true, NULL, 0, true);
            s->ustate[1] = phys_x(&o->chara);
        }
    } else active = false;
    if (ct >= s->ustate[0]) { p->u[1] = 1; proj_terminate_hitboxes(o); }
    if (f) {
        robo_t *r = ROBO(f);
        if (r->loic_dir != 0) {
            if (ct > 3 && r->loic_dir != s->ustate[3] && s->ustate[3] != 0) {
                s->ustate[0] -= 3 + s->ustate[4]; s->ustate[4] += 3; s->ustate[5] = 0;
            }
            s->ustate[3] = r->loic_dir;
        }
        s->ustate[5]++;
        int accel = s->ustate[5] / 5; if (accel > 16) accel = 16;
        if (!active) accel = 0;
        s->ustate[2] = (int)approach(fxi(s->ustate[2]), fxi((accel + 3) * r->loic_dir), fxi(1)) >> FX_SHIFT;
        phys_move(&o->chara, fxi(s->ustate[2]), 0);
        if (r->loic_dir == 0) { s->ustate[0] -= 1; if (ct % 2 == 0) s->ustate[0] -= 1; }
    }
    if (s->ustate[0] < 5) s->ustate[0] = 5;
    return SID_NONE;
}
static void osf_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb) { (void)hb; if (victim->is_fighter) fire_deactivate(o, s); }
static void osf_blocked(obj_t *o, state_t *s) { fire_deactivate(o, s); }
const state_vt_t vt_characters_robo_projectiles_states_OrbitalStrikeFire = { .enter = osf_enter, .frame = osf_frame, .tick = osf_tick, .hit_something = osf_hit, .got_blocked = osf_blocked };
