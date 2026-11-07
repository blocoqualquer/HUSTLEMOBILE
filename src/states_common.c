/* Camada CharacterState (CharState.gd) + estados de dano (CharacterHurtState/HurtGrounded/HurtAerial) + parry (NewParry/Parry). */
#include "fighter.h"
#include <string.h>

static inline fx fxi(int v) { return FX_INT(v); }
static inline int rnd(fx v) { return (int)fx_round(v); }
static inline int isign(int v) { return (v > 0) - (v < 0); }
#define F(o) ((o)->fighter)

#define WHIFF_SUPER_GAIN 10
enum { AT_MOVEMENT, AT_ATTACK, AT_SPECIAL, AT_SUPER, AT_DEFENSE, AT_HURT };
enum { BI_NORMAL, BI_HURT, BI_NONE };

static bool is_hurt_kind(const state_t *s) { return state_is_hurt(s); }

/* ---------- CharacterState ---------- */

sid cs_frame_shared(obj_t *o, state_t *s, int tick)
{
    if (tick != 0) return SID_NONE;
    fighter_t *f = F(o);
    if (!xp_find(&s->d, "super_level")) return SID_NONE;       /* so SuperMove gasta barra */
    if (xp_find(&s->d, "is_super") && !xp_int(&s->d, "is_super", 0)) return SID_NONE;   /* RobotState */
    int sl = xp_int(&s->d, "super_level", 1), su = xp_int(&s->d, "supers_used", -1);
    int levels = su == -1 ? sl : su;
    if (xp_int(&s->d, "scale_combo_meter", 1) && levels > 0) f->combo_supers++;
    /* efeito de super/EX (congelamento global): apresentacao, tratado pelo renderer via evento */
    for (int i = 0; i < levels; i++) fighter_use_super_bar(f);
    return SID_NONE;
}

sid cs_enter_shared(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    const charstate_def_t *c = &s->cs;
    if (s->d.force_same_direction_as_previous_state) f->reverse_state = false;
    obj_state_enter_shared(o, s);
    if (s->d.kind != KIND_PARRY_NEW) f->block_used_air_movement = false;
    s->started_during_combo = false;
    if (c->dynamic_iasa) s->cs.interruptible_on_opponent_turn = s->def->cs->interruptible_on_opponent_turn;
    if (xp_int(&s->d, "throw_invuln_frames", 0) > 0) o->throw_invulnerable = true;      /* RobotState */
    s->hit_yet = false; s->land_cancelled = false; s->hit_anything = false; s->hit_fighter = false;
    s->hit_hit_cancellable_projectile = false;
    s->entered_in_air = !o->chara.grounded;
    s->character_state_was_blocked = false;
    s->started_in_air = false;
    phys_update_grounded(&o->chara);
    if (!o->chara.grounded || c->air_type == AIR_AERIAL) s->started_in_air = true;
    if (c->uses_air_movement && !(c->ignore_air_option_use_in_combos && f->combo_count > 0))
        if (!f->infinite_resources && o->gravity_enabled) f->air_movements_left--;
    if (s->d.has_hitboxes) {
        int dir = fx_sign(f->last_vel.x);
        if (dir == 0 || dir == fighter_opponent_dir(f)) fighter_add_penalty(f, -8, false);
        if (!is_hurt_kind(s) && !f->gained_whiff_meter) {
            f->gained_whiff_meter = true;
            fighter_gain_super_meter(f, rnd(fx_mul(fxi(WHIFF_SUPER_GAIN), c->whiff_meter_gain_multiplier)), FX_ONE);
        }
    }
    return SID_NONE;
}

bool cs_can_interrupt(const state_t *s)
{
    if (is_hurt_kind(s)) return false;                       /* CharacterHurtState.can_interrupt */
    const charstate_def_t *c = &s->cs;
    int t = s->current_tick;
    if (t == c->iasa_at || t == s->d.anim_length - 1) return true;
    for (int i = 0; i < c->ninterrupt_frames; i++) if (c->interrupt_frames[i] == t) return true;
    if ((s->hit_fighter || s->hit_hit_cancellable_projectile) && t == c->iasa_on_hit) return true;
    /* iasa_on_hit_on_block (default true) */
    if (s->character_state_was_blocked && t == c->iasa_on_hit) return true;
    return false;
}

sid cs_tick_shared(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    charstate_def_t *c = &s->cs;
    { int tif = xp_int(&s->d, "throw_invuln_frames", 0); if (tif > 0 && s->current_tick == tif) o->throw_invulnerable = false; }
    if (s->current_tick == 0) {
        s->initiative_startup_reduction = false;
        s->feinting = f->feinting;
        s->hit_cancelled = false;
        if (f->initiative) {
            if (c->initiative_effect) {
                f->initiative_effect = false;
                if (c->initiative_startup_reduction_amount > 0) s->initiative_startup_reduction = true;
            }
        }
        f->moved_forward = false;
        if (c->release_opponent_on_startup) fighter_release_opponent(f);
        if (!is_hurt_kind(s) && c->reversible && !s->d.force_same_direction_as_previous_state) {
            if (f->reverse_state) {
                int facing = o->chara.facing;
                bool equal_x = phys_x(&f->opponent->o.chara) == phys_x(&o->chara);
                fighter_set_facing(f, facing * (equal_x ? 1 : -1), false);
            }
        } else f->reverse_state = false;
        f->moved_backward = false;
    }
    if (s->initiative_startup_reduction) { s->current_tick += c->initiative_startup_reduction_amount; s->initiative_startup_reduction = false; }
    if (!o->chara.grounded || c->air_type == AIR_AERIAL) s->started_in_air = true;
    sid next = obj_state_tick_shared(o, s);
    if (f->combo_count > 0 && s->hit_yet) s->started_during_combo = true;
    if (next != SID_NONE) return next;

    if (c->land_cancel && o->chara.grounded && s->started_in_air && s->current_tick > c->min_land_cancel_frame && o->chara.vy >= 0) {
        sm_queue_state(o, c->land_cancel_state, c->landing_recovery >= 0 ? (void *)(intptr_t)c->landing_recovery : NULL);
        s->land_cancelled = true;
    }
    if (s->current_tick <= s->d.anim_length && !s->d.endless) {
        if (cs_can_interrupt(s) && !ns_empty(&c->interrupt_into)) {
            cs_enable_interrupt(f, s, true, false);
            if (c->dynamic_iasa) c->interruptible_on_opponent_turn = 1;
        }
    }
    if (s->current_tick == c->no_collision_start_frame) f->colliding_with_opponent = false;
    if (s->current_tick == c->no_collision_end_frame) f->colliding_with_opponent = true;
    return SID_NONE;
}

sid cs_tick_after(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (s->current_real_tick < f->lowest_tick) f->lowest_tick = s->current_real_tick;     /* set_lowest_tick */
    return SID_NONE;
}

void cs_exit_shared(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    s->cs.beats_backdash = 0;
    if (s->feinting && s->cs.end_feint) { fighter_update_facing(f); f->feinting = false; }
    s->feinting = false;
    f->melee_attack_combo_scaling_applied = false;
    obj_state_exit_shared(o, s);
    if (s->cs.update_facing_on_exit) fighter_update_facing(f);
    else { f->reverse_state = false; fighter_set_facing(f, o->chara.facing, false); }
    if (s->cs.release_opponent_on_exit) fighter_release_opponent(f);
    f->got_parried = false; f->got_blocked = false; f->colliding_with_opponent = true;
    o->state_interruptable = false; f->projectile_hit_cancelling = false;
    f->has_hyper_armor = false; f->has_projectile_armor = false;
    o->state_hit_cancellable = false; f->clipping_wall = false;
}

/* CharacterState.enable_interrupt */
void cs_enable_interrupt(fighter_t *f, state_t *s, bool check_opponent, bool remove_hitlag)
{
    (void)remove_hitlag;
    charstate_def_t *c = &s->cs;
    if (s->d.kind == KIND_PARRY_NEW) {                        /* NewParry.enable_interrupt */
        int id = fighter_name_id(f, "AerialMovement");
        ns_del(&c->interrupt_exceptions, id);
        if (s->d.disable_aerial_movement) ns_add(&c->interrupt_exceptions, id);
    }
    fighter_t *op = f->opponent;
    state_t *os = op->o.sm.cur;
    if (c->backdash_iasa && os->cs.beats_backdash) return;
    if (check_opponent && c->beats_backdash && os->cs.beats_backdash) {
        cs_enable_interrupt(op, os, false, false);
        if (!c->allow_framecheat) sm_queue_state(&f->o, s->d.fallback_state, NULL);
    }
    if (c->beats_backdash && os->cs.backdash_iasa) {
        if (!os->cs.allow_framecheat) sm_queue_state(&op->o, os->d.fallback_state, NULL);
        if (!c->allow_framecheat) sm_queue_state(&f->o, s->d.fallback_state, NULL);
    }
    if (s->hit_fighter && !c->allow_framecheat) sm_queue_state(&f->o, s->d.fallback_state, NULL);
    fighter_enable_interrupt(f, s, check_opponent);
}

/* Hitbox 'hit_something' -> CharacterState._on_hit_something + ObjectState + try_hit_cancel */
void cs_on_hit_something(fighter_t *host, state_t *s, obj_t *victim, const hitbox_t *hb)
{
    if (!s->d.cs) return;
    charstate_def_t *c = &s->cs;
    bool vf = victim->is_fighter;
    if (!s->hit_yet && victim == &host->opponent->o) {
        s->hit_yet = true;
        fighter_stack_move(host, s->d.name_id);
        if (host->combo_count > 0) s->started_during_combo = true;
    }
    if (host->vt && host->vt->on_hit_something) host->vt->on_hit_something(host, victim);
    if (c->count_projectile_hits && !vf) s->hit_anything = true;
    if (vf) {
        s->hit_anything = true; s->hit_fighter = true;
        host->melee_attack_combo_scaling_applied = true;
        fighter_add_penalty(host, -25, false);
    }
    if (hb->def->followup_state >= 0 && vf) sm_queue_state(&host->o, hb->def->followup_state, NULL);
    /* try_hit_cancel */
    if (hb->cancellable) {
        bool ok = false;
        if (!(vf && victim->fighter->has_hyper_armor) && !hb->def->is_throwbox) {
            bool burst_only = false;      /* hit_cancel_into == ["OffensiveBurst"]: TODO quando hit_cancel_into estiver completo */
            (void)burst_only;
            if (vf) ok = hb->def->followup_state < 0;
        }
        if (ok) fighter_enable_hit_cancel(host, false);
    }
    (void)c;
}

/* ---------- CharacterHurtState (camada compartilhada dos estados de dano) ---------- */

static sid hurt_enter_shared(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    hitdata_t *h = (hitdata_t *)s->data;
    f->blocked_hitbox_plus_frames = 0;
    s->hu.brace = false;
    f->feinting = false;
    fighter_release_opponent(f);
    s->hu.hit = h;
    if (h && h->disable_collision) f->colliding_with_opponent = false;
    if (f->penalty_ticks <= 0) fighter_refresh_air_movements(f);
    o->state_interruptable = true;
    f->busy_interrupt = true;
    f->buffered_valid = false;
    cs_enter_shared(o, s);
    f->brace_effect_applied_yet = true;
    return SID_NONE;
}

static sid hurt_tick_shared(obj_t *o, state_t *s)
{
    sid r = cs_tick_shared(o, s);
    if (s->current_tick < 5) fighter_release_opponent(F(o));
    return r;
}

static void hurt_exit_shared(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    s->hu.brace = false; f->hit_out_of_brace = false; s->hu.guard_broken = false;
    fighter_start_sadness_immunity(f);
    cs_exit_shared(o, s);
}

static int hurt_hitstun(fighter_t *f, const hitdata_t *h)
{
    return rnd(fx_mul(fxi(h->hitstun_ticks /* + COUNTER_HIT_ADDITIONAL_HITSTUN_FRAMES(0) */), f->global_hitstun_modifier));
}

static fxv vacuum_dir(const fighter_t *f, const hitdata_t *h)
{
    fxv d = fxv_make(fxi(h->pos_x - phys_x(&f->o.chara)), fxi(h->pos_y - phys_y(&f->o.chara)));
    return fxv_norm(d);
}

#define GROUND_FRIC 3277
#define HG_DI_STRENGTH 229376      /* 3.5 */
#define AIR_FRIC 983
#define HIT_GRAV 16384
#define HIT_FALL_SPEED 983040
#define HA_DI_STRENGTH 131072      /* 2.0 */
#define BOUNCE_FACTOR (-55706)
#define BOUNCE_FRAMES 4
enum { BOUNCE_LEFT, BOUNCE_RIGHT, BOUNCE_NONE };

/* ---- HurtGrounded ---- */

static sid hg_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    hitdata_t *h = (hitdata_t *)s->data;
    s->hu.can_act = false;
    s->hu.hit = h;
    switch (h->hit_height) {
    case HEIGHT_HIGH: fighter_set_anim(f, s, "HurtGroundedHigh"); break;
    case HEIGHT_MID: fighter_set_anim(f, s, "HurtGroundedMid"); break;
    case HEIGHT_LOW: fighter_set_anim(f, s, "HurtGroundedLow"); break;
    }
    int hitstun = hurt_hitstun(f, h);
    s->hu.wall_slam = h->wall_slam && f->wall_slams < MAX_WALL_SLAMS;
    s->hu.counter = h->counter_hit;
    if (s->hu.counter) f->opponent->counterhit_this_turn = true;
    fx x = fighter_hitbox_x_dir(f, h);
    fighter_set_facing(f, isign((int)fx_round(x)) * -1, false);
    fx y = h->dir_y;
    if (h->vacuum) { fxv v = vacuum_dir(f, h); x = v.x; y = v.y; }
    else if (h->send_away_from_center) { fxv v = vacuum_dir(f, h); x = -v.x; y = -v.y; }
    fxv kb = fxv_norm_times(fxv_make(x, y), h->knockback);
    kb.y = 0;
    fxv di = fighter_get_scaled_di(f, f->current_di);
    fxv dif = fxv_make(fx_mul(di.x, fx_mul(HG_DI_STRENGTH, h->di_modifier)), 0);
    if (h->hitbox_type == HB_BURST) { dif.x = 0; dif.y = 0; }
    else kb = fxv_mul(kb, f->knockback_taken_modifier);
    if (f->braced_attack) hitstun = rnd(fx_mul(fxi(hitstun), FXC_BRACE_HITSTUN));
    if (f->touching_wall && !s->hu.wall_slam) kb.x = 0;
    phys_force(&o->chara, kb.x + dif.x, kb.y + dif.y);
    s->hu.hitstun = hitstun;
    return SID_NONE;
}

static sid hg_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    o->chara.py = 0;
    phys_x_fric(&o->chara, GROUND_FRIC);
    phys_apply_forces_no_limit(&o->chara);
    if (s->hu.wall_slam) {
        int bounce = BOUNCE_NONE;
        aabb_t cb = box_aabb(&o->collision_box);
        if (cb.x1 <= -f->stage_width && o->chara.vx < 0) bounce = BOUNCE_LEFT;
        else if (cb.x2 >= f->stage_width && o->chara.vx > 0) bounce = BOUNCE_RIGHT;
        if (bounce != BOUNCE_NONE) {
            sm_queue_state(o, fighter_name_id(f, "WallSlam"), (void *)(intptr_t)bounce);
            return SID_NONE;
        }
    }
    if (s->current_tick >= s->hu.hitstun) {
        if (s->hu.can_act) return s->d.fallback_state;
        cs_enable_interrupt(f, s, true, false);
        s->hu.can_act = true;
    }
    return SID_NONE;
}

const state_vt_t vt_characters_states_HurtGrounded = {
    .enter_shared = hurt_enter_shared, .tick_shared = hurt_tick_shared, .exit_shared = hurt_exit_shared,
    .enter = hg_enter, .tick = hg_tick,
};

/* ---- HurtAerial ---- */

static void begin_ground_bounce(state_t *s, hitdata_t *h)
{
    h->dir_y = -h->dir_y;
    h->knockback = fx_mul(h->knockback, h->ground_bounce_knockback_modifier);
    s->hu.bounce_frames = BOUNCE_FRAMES;
    s->hu.ground_bounced = true;
}

static sid ha_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    hitdata_t *h = (hitdata_t *)s->data;
    s->hu.ground_bounced = false;
    s->hu.can_act = false;
    s->hu.knockdown = h->knockdown;
    s->hu.hard_knockdown = h->hard_knockdown;
    s->hu.wall_slam = h->wall_slam && f->wall_slams < MAX_WALL_SLAMS;
    int hitstun = hurt_hitstun(f, h);
    s->hu.counter = h->counter_hit;
    if (s->hu.counter) f->opponent->counterhit_this_turn = true;
    if (h->ground_bounce && o->chara.grounded && h->dir_y > 0) begin_ground_bounce(s, h);
    fx x = fighter_hitbox_x_dir(f, h);
    fx y = h->dir_y;
    if (h->vacuum) { fxv v = vacuum_dir(f, h); x = v.x; y = v.y; }
    else if (h->send_away_from_center) { fxv v = vacuum_dir(f, h); x = -v.x; y = -v.y; }
    fxv kb = fxv_norm_times(fxv_make(x, y), h->knockback);
    fighter_set_facing(f, isign((int)fx_round(x)) * -1, false);
    fxv di = fighter_get_scaled_di(f, f->current_di);
    fxv dif = fxv_mul(di, fx_mul(HA_DI_STRENGTH, h->di_modifier));
    if (h->hitbox_type == HB_BURST) { dif.x = 0; dif.y = 0; }
    else kb = fxv_mul(kb, f->knockback_taken_modifier);
    phys_force(&o->chara, kb.x + dif.x, kb.y + dif.y);
    phys_move(&o->chara, 0, -FX_ONE);
    fighter_set_anim(f, s, "HurtAerial");
    s->hu.hitstun = hitstun;
    s->hu.hit = h;
    return SID_NONE;
}

static sid ha_frame(obj_t *o, state_t *s, int t)
{
    if (t == 1 && F(o)->braced_attack) s->hu.hitstun = rnd(fx_mul(fxi(s->hu.hitstun), FXC_BRACE_HITSTUN));
    return SID_NONE;
}

static void ha_exit(obj_t *o, state_t *s) { (void)o; s->hu.bounce_frames = 0; }

static sid ha_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    hitdata_t *h = (hitdata_t *)s->hu.hit;
    phys_t *c = &o->chara;
    if (c->grounded && s->hu.bounce_frames > 0) fighter_set_anim(f, s, "Knockdown"); else fighter_set_anim(f, s, "HurtAerial");
    phys_x_fric(c, AIR_FRIC);
    phys_grav_custom(c, HIT_GRAV, HIT_FALL_SPEED);
    phys_apply_forces_no_limit(c);
    fx vx = c->vx, vy = c->vy;
    int bounce = BOUNCE_NONE;
    aabb_t cb = box_aabb(&o->collision_box);
    if (o->hitlag_ticks > 0 || (c->grounded && s->hu.bounce_frames > 0)) {}
    else if (cb.x1 <= -f->stage_width && vx < 0) bounce = BOUNCE_LEFT;
    else if (cb.x2 >= f->stage_width && vx > 0) bounce = BOUNCE_RIGHT;
    if (bounce != BOUNCE_NONE) {
        if (s->hu.wall_slam) { sm_queue_state(o, fighter_name_id(f, "WallSlam"), (void *)(intptr_t)bounce); return SID_NONE; }
        o->hitlag_ticks = 3;
        c->vx = fx_mul(vx, BOUNCE_FACTOR);
    }
    if (s->hu.bounce_frames > 0) {
        c->py = 0;
        s->hu.bounce_frames--;
        if (s->hu.bounce_frames == 0) c->py = -FX_ONE;
    } else {
        if (c->grounded && vy >= 0) {
            if (h->air_ground_bounce && !s->hu.ground_bounced) {
                begin_ground_bounce(s, h);
                c->vy = fx_mul(vy, -FX_ONE);
            } else {
                if (s->current_tick > h->minimum_grounded_frames) {
                    if (s->hu.knockdown || f->hp == 0)
                        return s->hu.hard_knockdown ? f->st_hard_knockdown : f->st_knockdown;
                    if (f->hp > 0) return fighter_name_id(f, "Landing");
                    return f->st_knockdown;
                } else if (s->current_tick > 0) {
                    switch (h->hit_height) {
                    case HEIGHT_HIGH: fighter_set_anim(f, s, "HurtGroundedHigh"); break;
                    case HEIGHT_MID: fighter_set_anim(f, s, "HurtGroundedMid"); break;
                    case HEIGHT_LOW: fighter_set_anim(f, s, "HurtGroundedLow"); break;
                    }
                }
            }
        } else fighter_set_anim(f, s, "HurtAerial");
    }
    bool extended = h->knockdown_extends_hitstun && h->knockdown && !s->hu.ground_bounced;
    if (!extended && s->current_tick > s->hu.hitstun) {
        if (s->hu.can_act && f->hp > 0) return s->d.fallback_state;
        cs_enable_interrupt(f, s, true, false);
        s->hu.can_act = true;
    }
    return SID_NONE;
}

const state_vt_t vt_characters_states_HurtAerial = {
    .enter_shared = hurt_enter_shared, .tick_shared = hurt_tick_shared, .exit_shared = hurt_exit_shared,
    .enter = ha_enter, .frame = ha_frame, .tick = ha_tick, .exit = ha_exit,
};

/* ---------- Parry / NewParry ---------- */

static void parry_start(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    const parry_data_t *pd = (const parry_data_t *)s->data;
    s->pr.started_in_combo = f->combo_count > 0;
    s->d.endless = 0;
    s->pr.perfect = true;
    s->pr.parry_type = s->pr.initial_parry_type;
    if (s->d.kind == KIND_PARRY_NEW) {
        s->pr.parry_type = (pd && pd->height_y != 0) ? PH_LOW : PH_HIGH;
        s->cs.interruptible_on_opponent_turn = f->combo_count <= 0;
        s->d.anim_length = 20 + s->pr.extra_iasa;
        f->blocked_hitbox_plus_frames = 0;
        bool low = !(o->chara.grounded && (!pd || pd->height_y == 0));
        fighter_set_anim(f, s, s->d.use_guard_sprites ? (low ? "ShieldLow" : "ShieldHigh") : (low ? "ParryLow" : "ParryHigh"));
    } else {
        s->cs.interruptible_on_opponent_turn = 0;
        s->d.anim_length = 20;
    }
    s->pr.parry_active = true;
    s->pr.parry_tick = 0;
    s->pr.parried = false;
    s->cs.iasa_at = -1;
}

static sid parry_enter(obj_t *o, state_t *s)
{
    if (s->d.kind == KIND_PARRY_NEW) { s->pr.extra_iasa = 0; /* disable_aerial_movement restaurado do def */ s->d.disable_aerial_movement = s->def->disable_aerial_movement; parry_start(o, s); }
    return SID_NONE;
}

static sid parry_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    if (s->d.kind == KIND_PARRY_NEW) {
        if (t == 0) {
            parry_start(o, s);
            state_t *prev = obj_prev_state(o);
            if (!(prev && prev->d.kind == KIND_PARRY_NEW && !prev->d.push)) s->pr.punishable = false;
        }
    } else if (t == 0) {
        o->throw_invulnerable = false;
        parry_start(o, s);
    }
    if (t == 10 && s->d.kind != KIND_PARRY_NEW && s->d.can_parry && !s->pr.parried && s->pr.perfect) s->pr.parry_active = false;
    (void)f;
    return SID_NONE;
}

static sid parry_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    phys_fric(&o->chara);
    if (s->d.kind == KIND_PARRY_NEW) {
        if (o->gravity_enabled) obj_apply_grav(o);               /* NewParry._tick: apply_grav */
        if (f->combo_count > 0 && s->current_tick > 60 && s->pr.parried) { cs_enable_interrupt(f, s, true, false); return s->d.fallback_state; }
        obj_apply_forces(o);
        f->parry_knockback_divisor = FXC_PARRY_GROUNDED_KB_DIVISOR;
        state_t *os = f->opponent->o.sm.cur;
        if (os->d.kind == KIND_PARRY_NEW) {
            int stop = 14;
            if (!s->d.push && !os->d.push) stop = 4;
            if (s->current_tick == stop) cs_enable_interrupt(f, s, true, false);
        }
        if (f->hp <= 0) return fighter_name_id(f, "Wait");
    } else {
        if (s->cs.air_type == AIR_AERIAL && o->gravity_enabled) obj_apply_grav(o);
        if (f->combo_count > 0 && s->current_tick > 30 && s->pr.parried) { cs_enable_interrupt(f, s, true, false); return s->d.fallback_state; }
        obj_apply_forces(o);
        if (s->current_tick >= 10 && s->pr.perfect && s->d.can_parry) s->pr.parry_active = false;
    }
    return SID_NONE;
}

static void parry_exit(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    s->pr.parry_active = false;
    if (s->d.kind == KIND_PARRY_NEW) s->pr.parried_last = s->pr.parried;
    f->blocked_last_hit = false;
}

const state_vt_t vt_characters_states_NewParry = {
    .enter = parry_enter, .frame = parry_frame, .tick = parry_tick, .exit = parry_exit,
};
const state_vt_t vt_characters_states_Parry = {
    .enter = parry_enter, .frame = parry_frame, .tick = parry_tick, .exit = parry_exit,
};
