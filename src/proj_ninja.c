/* Projeteis do Ninja: scripts raiz (Kunai, Caltrops, Stackriken, StickyBomb, grappling_hook, WhipWave...) e estados. */
#include "ninja.h"
#include "states.h"
#include <string.h>
#include <stdlib.h>

static inline fx fxi(int v) { return FX_INT(v); }
static inline int isgn(int v) { return (v > 0) - (v < 0); }
#define NJ(p) NINJA((p)->owner)
#define O_OF(uid) game_obj_by_uid(eng_game_get(), (uid))

/* ---------------- Kunai ---------------- */
static void kunai_hit_by(obj_t *self, hitbox_t *hb) { (void)self; if (hb->host && hb->host->is_fighter) proj_disable(self); }
const proj_vt_t vtp_characters_stickman_projectiles_Kunai = { .hit_by = kunai_hit_by };

/* ---------------- Caltrops ---------------- */
static void caltrops_hit_by(obj_t *self, hitbox_t *hb)
{
    proj_t *p = PROJ(self);
    obj_t *host = hb->host;
    if (hb->def->is_throwbox) return;
    if (host && host->is_fighter && host->id != self->id) {
        int misc = hb->def->misc;
        if (host->sm.cur) { int sm_ = xp_int(&host->sm.cur->d, "misc_data", 0); if (sm_) misc = sm_; }
        if (misc & MISC_IGNORE_CALTROPS) return;
        if (!(misc & MISC_LAUNCH_CALTROPS)) { proj_disable(self); return; }
    }
    if (host && host->is_fighter) fighter_projectile_free_cancel(host->fighter);
    fxv force = fxv_mul(fxv_make(hb->dir_x, hb->dir_y), hb->knockback);
    if (host && host->id == self->id && p->creator && p->creator->is_fighter) {
        fighter_t *c = p->creator->fighter;
        force.x += fxi(c->current_di.x / FX_ONE / 5);
        force.y += fxi(c->current_di.y / FX_ONE / 20);
    }
    phys_force(&self->chara, force.x, force.y);
}
const proj_vt_t vtp_characters_stickman_projectiles_Caltrops = { .hit_by = caltrops_hit_by };

static sid cal_tick(obj_t *o, state_t *s)
{
    default_tick_common(o, s, default_move);
    if (o->chara.grounded) { PROJ(o)->pj.has_projectile_parry_window = false; if (s->d.nhb) s->hbr[0].hit_height = HEIGHT_LOW; }
    else { PROJ(o)->pj.has_projectile_parry_window = true; if (s->d.nhb) s->hbr[0].hit_height = HEIGHT_MID; }
    return SID_NONE;
}
static void cal_parried(obj_t *o, state_t *s) { (void)s; proj_disable(o); }
static void cal_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    proj_on_hit(o, s, victim, hb);
    if (victim->is_fighter && victim->id != o->id) proj_disable(o);
}
const state_vt_t vt_characters_stickman_projectiles_CaltropsDefault = { .enter = proj_enter, .frame = proj_frame0, .tick = cal_tick, .hit_something = cal_hit, .got_parried = cal_parried };

/* ---------------- WhipWave ---------------- */
static sid ww_enter(obj_t *o, state_t *s) { proj_enter(o, s); if (PROJ(o)->whip_parriable) PROJ(o)->pj.has_projectile_parry_window = true; return SID_NONE; }
static sid ww_frame(obj_t *o, state_t *s, int t) { proj_frame0(o, s, t); if (t == 4) PROJ(o)->pj.has_projectile_parry_window = true; return SID_NONE; }
static sid ww_tick(obj_t *o, state_t *s) { return default_tick_common(o, s, default_move); }
const state_vt_t vt_characters_stickman_projectiles_WhipWaveDefault = { .enter = ww_enter, .frame = ww_frame, .tick = ww_tick, .hit_something = proj_on_hit };

/* ---------------- Stackriken ---------------- */
#define STACK_REFRESH 24
static void stack_hit_by(obj_t *self, hitbox_t *hb)
{
    proj_t *p = PROJ(self);
    if (hb->def->is_throwbox) return;
    obj_t *host = hb->host;
    if (host && host->is_fighter) {
        fx dx = hb->dir_x * host->chara.facing;                       /* get_hitbox_x_dir */
        fxv force = fxv_norm_times(fxv_make(dx, hb->dir_y), fx_mul(hb->knockback, FXC(0.85)));
        fighter_t *f = p->owner;
        if (f && self->id == host->id) { force.x += fxi(f->current_di.x / FX_ONE / 20); force.y += fxi(f->current_di.y / FX_ONE / 20); }
        self->chara.vx = force.x; self->chara.vy = force.y;
        state_t *s = self->sm.cur;
        s->current_tick -= p->refresh_amount;
        if (s->current_tick <= 0) s->current_tick = 0;
        p->refresh_amount -= 3;
        if (p->refresh_amount < 0) p->refresh_amount = 0;
    }
}
static void stack_init(obj_t *self)
{
    proj_t *p = PROJ(self);
    if (p->creator && p->owner) NJ(p)->stackriken_out = true;
    p->refresh_amount = STACK_REFRESH;
}
static void stack_on_disable(obj_t *self) { proj_t *p = PROJ(self); if (p->owner) NJ(p)->stackriken_out = false; }
const proj_vt_t vtp_characters_stickman_projectiles_Stackriken = { .hit_by = stack_hit_by, .init = stack_init, .on_disable = stack_on_disable };

void ninja_stackriken_refresh(obj_t *self)
{
    state_t *s = self->sm.cur;
    s->current_tick -= xp_int(&s->d, "lifetime", 0);
    if (s->current_tick <= 0) s->current_tick = 1;
}

/* estado: ustate[3] = block_hits; ufx[0..2] = hitbox_width/height (def) nao usado -> leitura do def */
static sid sr_enter(obj_t *o, state_t *s) { proj_enter(o, s); s->ustate[3] = 5; return SID_NONE; }
static sid sr_frame(obj_t *o, state_t *s, int t)
{
    proj_frame0(o, s, t);
    if (t != 1) return SID_NONE;
    proj_t *p = PROJ(o);
    if (p->refresh_amount < STACK_REFRESH) return SID_NONE;
    if (p->owner) { p->has_return = true; p->return_x = phys_x(&p->owner->o.chara); p->return_y = phys_y(&p->owner->o.chara); }
    phys_force(&o->chara, p->force_x, p->force_y);
    return SID_NONE;
}
static void sr_move(obj_t *o, state_t *s)
{
    (void)s;
    proj_t *p = PROJ(o);
    if (!p->has_return) return;
    fxv dir = fxv_norm(fxv_make(fxi(p->return_x - phys_x(&o->chara)), fxi(p->return_y - phys_y(&o->chara))));
    fxv force = fxv_mul(dir, FXC(0.25));
    phys_force(&o->chara, force.x, force.y);
}
static void sr_got_blocked(obj_t *o, state_t *s) { if (--s->ustate[3] == 0) proj_fizzle(o, s); }
static sid sr_tick(obj_t *o, state_t *s)
{
    default_tick_common(o, s, sr_move);
    fxv vel = fxv_make(o->chara.vx, o->chara.vy);
    fxv dir = fxv_norm(vel);
    int tick = s->current_tick - 20; if (tick < 0) tick = 0;
    int life = xp_int(&s->d, "lifetime", 999999);
    fx ratio = FXC(0.5) + fx_mul(FX_ONE - fx_div(fxi(tick), fxi(life - 20)), FX_ONE - FXC(0.5));
    if (ratio < FXC(0.5)) ratio = FXC(0.5);
    if (s->d.nhb) {
        hitbox_t *h = &s->hbr[0];
        h->dir_x = dir.x; h->dir_y = dir.y;
        h->box.w = (int)fx_round(fx_mul(fxi(h->def->w), ratio));
        h->box.h = (int)fx_round(fx_mul(fxi(h->def->h), ratio));
        h->knockback = fx_mul(fx_vec_len(vel.x, vel.y), FXC(0.5));
        h->damage = h->def->damage - tick / 5;
    }
    const chardata_t *cd = PROJ(o)->cd;
    o->hurtbox.w = (int)fx_round(fx_mul(fxi(cd->hurtbox.w), ratio));
    o->hurtbox.h = (int)fx_round(fx_mul(fxi(cd->hurtbox.h), ratio));
    phys_apply_forces_no_limit(&o->chara);
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_projectiles_StackrikenDefault = { .enter = sr_enter, .frame = sr_frame, .tick = sr_tick, .hit_something = proj_on_hit, .got_blocked = sr_got_blocked };

/* ---------------- Shuriken: empilhamento (ShurikenDefault._tick) ---------------- */
extern const chardata_t chardata_ninja_Stackriken;
extern const chardata_t chardata_ninja_StickmanFireball, chardata_ninja_StickmanAirFireball;
static bool is_shuriken(const obj_t *o) { return obj_is_proj(o) && (PROJ(o)->cd == &chardata_ninja_StickmanFireball || PROJ(o)->cd == &chardata_ninja_StickmanAirFireball); }

static fxv shuriken_dir(obj_t *o)
{
    proj_t *p = PROJ(o);
    return fxv_mul(fxv_make(fx_mul(p->dir_x, fxi(o->chara.facing)), p->dir_y), p->sdata.speed.sm);
}

void ninja_shuriken_stack(obj_t *host, state_t *s)
{
    proj_t *hp = PROJ(host);
    if (s->current_tick > 120) { obj_apply_grav(host); host->chara.vx = 0; obj_apply_forces(host); }
    if (!hp->can_stack) return;
    game_t *g = eng_game_get();
    bool out = NJ(hp)->stackriken_out;
    for (int i = 0; i < g->nobjs; i++) {
        obj_t *obj = g->objs[i];
        if (obj == host || obj->disabled || obj->id != host->id || !obj_hurtboxes_overlap(host, obj)) continue;
        proj_t *op = PROJ(obj);
        if (is_shuriken(obj)) {
            if (out) continue;
            if (!op->can_stack) continue;
            proj_t *sk = proj_spawn_from(host, &chardata_ninja_Stackriken, 0, 0, true, NULL, 0);
            if (!sk) continue;
            fxv d1 = shuriken_dir(host), d2 = shuriken_dir(obj);
            fxv force = fxv_mul(fxv_add(d1, d2), FXC(0.9));
            sk->force_x = force.x; sk->force_y = force.y;
            proj_disable(obj); proj_disable(host);
        }
        if (op->cd == &chardata_ninja_Stackriken) {
            proj_disable(host);
            if (obj->sm.cur) {
                ninja_stackriken_refresh(obj);
                fxv force = fxv_mul(shuriken_dir(host), FXC(0.5));
                phys_force(&obj->chara, force.x, force.y);
            }
        }
    }
}

/* ---------------- StickyBomb ---------------- */
void proj_explode(obj_t *o)
{
    proj_t *p = PROJ(o);
    p->detonating = true;
    if (p->owner) { NJ(p)->bomb_thrown = false; NJ(p)->bomb_uid = -1; }
}
static void big_explode(obj_t *o)
{
    proj_t *p = PROJ(o);
    sm_queue_state(o, fighter_name_id(p->owner, "BigExplode") >= 0 ? fighter_name_id(p->owner, "BigExplode") : 2, NULL);
    if (p->owner) { NJ(p)->bomb_thrown = false; NJ(p)->bomb_uid = -1; }
}
const proj_vt_t vtp_characters_stickman_projectiles_StickyBomb = { 0 };

static sid sb_frame(obj_t *o, state_t *s, int t) { (void)o; if (t == 0) { s->ustate[3] = 0; } return SID_NONE; }
static sid sb_enter(obj_t *o, state_t *s) { proj_enter(o, s); s->ustate[4] = 10; return SID_NONE; }
static sid sb_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    fighter_t *opp = p->owner->opponent;
    if (p->attached) {
        o->chara.facing = opp->o.chara.facing;
        int cx, cy; obj_hurt_center(&opp->o, &cx, &cy);
        phys_set_pos(&o->chara, fxi(cx), fxi(cy));
    } else {
        o->chara.grounded = o->chara.py >= 0;
        int cx, cy; obj_hurt_center(&opp->o, &cx, &cy);
        fx dist = fx_vec_dist(fxi(phys_x(&o->chara)), fxi(phys_y(&o->chara)), fxi(cx), fxi(cy));
        if (dist < fxi(16)) s->ustate[3]++; else s->ustate[3] = 0;
        if (s->ustate[3] > 2 && !opp->o.invulnerable && !opp->o.projectile_invulnerable) p->attached = true;
    }
    if (s->current_tick > 120) big_explode(o);
    else if (p->detonating) {
        if (--s->ustate[4] == 0) return fighter_name_id(p->owner, "Explode") >= 0 ? fighter_name_id(p->owner, "Explode") : 1;
    }
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_projectiles_StickyBombDefault = { .enter = sb_enter, .frame = sb_frame, .tick = sb_tick, .hit_something = proj_on_hit };

static sid sbe_tick(obj_t *o, state_t *s) { if (s->current_tick == 15) proj_disable(o); return SID_NONE; }
const state_vt_t vt_characters_stickman_projectiles_StickyBombExplode = { .tick = sbe_tick };

/* ---------------- Grappling hook ---------------- */
#define HOOK_MAX_HEIGHT 300
static void hook_tick(obj_t *self)
{
    proj_t *p = PROJ(self);
    if (phys_y(&self->chara) < p->start_y - HOOK_MAX_HEIGHT) proj_disable(self);
}
static void hook_got_parried(obj_t *self) { proj_disable(self); }
static void hook_on_disable(obj_t *self) { proj_t *p = PROJ(self); if (p->owner) NJ(p)->hook_uid = -1; }
const proj_vt_t vtp_characters_stickman_projectiles_grappling_hook = { .tick = hook_tick, .on_got_parried = hook_got_parried, .on_disable = hook_on_disable };

void ninja_hook_unlock(obj_t *self);
static void hook_unlock(obj_t *self) { ninja_hook_unlock(self); }
void ninja_hook_unlock(obj_t *self)
{
    proj_t *p = PROJ(self);
    if (p->is_locked) {
        sm_queue_state(self, 0, NULL);          /* change_state("Default") */
        p->is_locked = false; p->attached_to = -1;
    }
}
static void hook_lock(obj_t *self, state_t *s, obj_t *target)
{
    (void)s;
    proj_t *p = PROJ(self);
    sm_queue_state(self, 1, NULL);              /* "Locked" */
    p->attached_to = target ? target->uid : -1;
    p->is_locked = true;
}
static sid hd_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    phys_grav_custom(&o->chara, o->chara.grav, fxi(1000000000));
    phys_apply_forces_no_limit(&o->chara);
    o->chara.grounded = o->chara.py >= 0;
    if (o->chara.grounded) { hook_lock(o, s, NULL); return SID_NONE; }
    int sw = p->owner->stage_width;
    if (abs(abs(phys_x(&o->chara)) - sw) < 2) { hook_lock(o, s, NULL); return SID_NONE; }
    game_t *g = eng_game_get();
    obj_t *oh = p->owner->opponent ? (NINJA(p->owner->opponent)->hook_uid >= 0 ? O_OF(NINJA(p->owner->opponent)->hook_uid) : NULL) : NULL;
    int mx, my; obj_hurt_center(o, &mx, &my);
    obj_t *cands[GAME_MAX_OBJS + 2]; int nc = 0;
    for (int i = 0; i < g->nobjs; i++) cands[nc++] = g->objs[i];
    for (int i = 0; i < 2; i++) cands[nc++] = &g->p[i]->o;
    for (int i = 0; i < nc; i++) {
        obj_t *ob = cands[i];
        if (ob->disabled || ob == o || ob == p->creator) continue;
        if (obj_is_proj(ob) && !PROJ(ob)->pj.hookable) continue;
        int ox, oy; obj_hurt_center(ob, &ox, &oy);
        fx dist = fx_vec_len(fxi(ox - mx), fxi(oy - my));
        fx lock_dist = fxi(8);
        if (ob->is_fighter) lock_dist = fxi(16);
        else if (oh && ob == oh) lock_dist = fxi(16);
        if (dist < lock_dist) { hook_lock(o, s, ob); break; }
    }
    return SID_NONE;
}
static void hd_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb) { (void)hb; hook_lock(o, s, victim); }
const state_vt_t vt_characters_stickman_projectiles_GrapplingHook_GrapplingHookDefault = { .tick = hd_tick, .hit_something = hd_hit };

static sid hl_frame(obj_t *o, state_t *s, int t) { (void)o; if (t == 0) s->ustate[0] = 0; return SID_NONE; }
static sid hl_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    obj_t *att = p->attached_to >= 0 ? O_OF(p->attached_to) : NULL;
    if (att) {
        if (att->disabled) hook_unlock(o);
        else { s->ustate[0] = 1; int cx, cy; obj_hurt_center(att, &cx, &cy); phys_set_pos(&o->chara, fxi(cx), fxi(cy)); }
    } else if (s->ustate[0]) hook_unlock(o);
    if (s->current_tick > (att ? 120 : 90)) proj_disable(o);
    return SID_NONE;
}
static void hl_exit(obj_t *o, state_t *s) { (void)o; s->ustate[0] = 0; }
const state_vt_t vt_characters_stickman_projectiles_GrapplingHook_Locked = { .frame = hl_frame, .tick = hl_tick, .exit = hl_exit };

/* Stickman.tick: parte do gancho */
void ninja_hook_tick(fighter_t *f)
{
    ninja_t *n = NINJA(f);
    obj_t *hook = n->hook_uid >= 0 ? O_OF(n->hook_uid) : NULL;
    if (hook && !hook->disabled) {
        if (fighter_is_in_hurt_state(f, false)) proj_disable(hook);
        else {
            proj_t *hp = PROJ(hook);
            obj_t *att = hp->attached_to >= 0 ? O_OF(hp->attached_to) : NULL;
            int hx, hy, mx, my;
            obj_hurt_center(&f->o, &mx, &my);
            if (!att) { obj_hurt_center(hook, &hx, &hy); hx -= mx; hy -= my; }
            else if (att->is_fighter) { hx = phys_x(&att->chara) - phys_x(&f->o.chara); hy = phys_y(&att->chara) - phys_y(&f->o.chara); }
            else { obj_hurt_center(att, &hx, &hy); hx -= mx; hy -= my; }
            if (hp->is_locked && hook->sm.cur && hook->sm.cur->current_tick > 5 && fx_vec_len(fxi(hx), fxi(hy)) < fxi(32)) proj_disable(hook);
            if (n->pulling) {
                fxv dir = fxv_norm_times(fxv_make(fxi(hx), fxi(hy)), fxi(3));
                phys_force(&f->o.chara, dir.x, dir.y);
                fx len = fx_vec_len(f->o.chara.vx, f->o.chara.vy);
                if (len > fxi(15)) { fxv nv = fxv_norm_times(fxv_make(f->o.chara.vx, f->o.chara.vy), fxi(15)); f->o.chara.vx = nv.x; f->o.chara.vy = nv.y; }
                if (f->o.chara.vy < fxi(-10)) f->o.chara.vy = fxi(-10);
                int od = isgn(phys_x(&f->opponent->o.chara) - phys_x(&f->o.chara));
                if (fx_sign(dir.x) != od && f->combo_count <= 0) fighter_add_penalty(f, 2, false);
            }
        }
    } else n->pulling = false;
}

/* ---------------- Summon: Kunai / Slide ---------------- */
extern const chardata_t chardata_ninja_Kunai;
static sid sk_frame(obj_t *o, state_t *s, int t)
{
    proj_t *p = PROJ(o);
    switch (t) {
    case 0: {
        p->stopped = false;
        fx y = fx_mul(fx_mul(fxi(p->sdata.xy.y), fxi(5)), -FX_ONE);
        fxv force = fxv_norm_times(fxv_make(FXC(-0.45), FXC(-1.0)), fxi(6));
        force.y += y;
        phys_force_rel(&o->chara, force.x, force.y);
        break; }
    case 2: p->stopped = true; break;
    case 23: {
        int dx = abs(p->sdata.xy.x) * o->chara.facing, dy = p->sdata.xy.y;
        p->sdata.xy.x = dx;
        p->stopped = false;
        fxv kb = fxv_norm_times(fxv_make(fxi(dx), fxi(dy)), fxi(-2));
        phys_force_rel(&o->chara, kb.x, kb.y);
        for (int i = -2; i <= 0; i++) {
            fxv dir = fx_rotate_vec(fxv_make(fxi(dx), fxi(dy)), fx_deg2rad(fxi(i * o->chara.facing * 20)));
            dir.x = fx_abs(dir.x);
            struct { fxv dir; } kd = { dir };
            proj_spawn_from(o, &chardata_ninja_Kunai, 0, 0, true, &kd, sizeof kd);
        }
        break; }
    case 45:
        proj_terminate_hitboxes(o);
        o->disabled = true;
        if (p->owner) NINJA(p->owner)->can_summon_kunai = true;
        break;
    }
    return SID_NONE;
}
static sid sk_tick(obj_t *o, state_t *s)
{
    (void)s;
    proj_t *p = PROJ(o);
    int x = phys_x(&o->chara), sw = p->owner->stage_width;
    if (x >= sw - 5 || x <= 5 - sw) phys_set_pos(&o->chara, fxi((sw - 6) * isgn(x)), o->chara.py);
    if (p->stopped) {
        obj_apply_grav(o);
        o->chara.grounded = o->chara.py >= 0;
        if (o->chara.vy >= 0) o->chara.vy = 0;
    }
    phys_fric(&o->chara);
    obj_apply_forces(o);
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_projectiles_fireball_states_StickmanSummonKunai = { .frame = sk_frame, .tick = sk_tick };

static sid ss_tick(obj_t *o, state_t *s)
{
    obj_apply_grav(o); phys_fric(&o->chara); obj_apply_forces(o);
    if (o->chara.grounded) return fighter_name_id_cd(PROJ(o)->cd, "Slide");
    (void)s;
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_projectiles_fireball_states_StickmanSummonSlide = { .tick = ss_tick };

static sid ssk_tick(obj_t *o, state_t *s)
{
    proj_t *p = PROJ(o);
    if (o->disabled) return SID_NONE;
    phys_fric(&o->chara); obj_apply_grav(o); obj_apply_forces(o);
    if (s->current_tick > 40) {
        proj_terminate_hitboxes(o);
        o->disabled = true;
        if (p->owner) NINJA(p->owner)->can_summon_kick = true;
    }
    return SID_NONE;
}
const state_vt_t vt_characters_stickman_projectiles_fireball_states_StickmanSummonSlideKick = { .tick = ssk_tick };

extern const chardata_t chardata_ninja_StickyBomb, chardata_ninja_grappling_hook;
bool ninja_obj_is_bomb(obj_t *o) { return !o->is_fighter && PROJ(o)->cd == &chardata_ninja_StickyBomb; }
bool ninja_obj_is_hook(obj_t *o) { return !o->is_fighter && PROJ(o)->cd == &chardata_ninja_grappling_hook; }
