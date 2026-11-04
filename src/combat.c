/* Combate do lutador: snapshot do golpe (HitboxData), hit_by, launched_by, bloqueio/parry, Hitbox.hit.
 * Fonte: BaseChar.gd 1481-1600, 1608-2100; mechanics/HitboxData.gd; mechanics/Hitbox.gd (get_real_*, is_counter_hit, hit). */
#include "fighter.h"
#include <string.h>

static inline fx fxi(int v) { return FX_INT(v); }
static inline int rnd(fx v) { return (int)fx_round(v); }
static inline int isign(int v) { return (v > 0) - (v < 0); }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int imin(int a, int b) { return a < b ? a : b; }

/* ganchos de apresentacao/projeteis (sobrescritos por projectile.c / render) */
__attribute__((weak)) bool eng_proj_always_parriable(obj_t *p) { (void)p; return false; }
__attribute__((weak)) bool eng_proj_has_parry_window(obj_t *p) { (void)p; return true; }
__attribute__((weak)) void eng_host_got_blocked(obj_t *host, fighter_t *by) { (void)host; (void)by; }
__attribute__((weak)) void eng_host_got_parried(obj_t *host, fighter_t *by) { (void)host; (void)by; }
__attribute__((weak)) void eng_host_got_push_blocked(obj_t *host) { (void)host; }
__attribute__((weak)) void eng_fighter_got_push_blocked(fighter_t *f) { (void)f; }
__attribute__((weak)) void eng_set_parry_anim(fighter_t *f, state_t *s, bool low) { (void)f; (void)s; (void)low; }
__attribute__((weak)) void eng_host_detect(obj_t *host, obj_t *victim) { (void)host; (void)victim; }
__attribute__((weak)) void eng_sfx(obj_t *o, int which) { (void)o; (void)which; }

#define SFX_BLOCK 1
#define SFX_PARRY 2

static inline charstate_def_t *csd(state_t *s) { return s->d.cs ? &s->cs : NULL; }

/* ---------- snapshot do golpe ---------- */

static fighter_t *creator_of(const hitbox_t *h)
{
    obj_t *host = h->host;
    return host ? host->fighter : NULL;
}

static int real_hitstun(const hitbox_t *h)
{
    fighter_t *c = creator_of(h);
    if (!c) return h->hitstun_ticks;
    int ticks = c->combo_count <= 0 ? h->hitstun_ticks : h->combo_hitstun_ticks;
    bool started_above_0 = ticks > 0;
    /* PRORATION_HITSTUN_ADJUSTMENT_AMOUNT = 0 */
    if (h->host->is_fighter) {
        int used = fighter_moves_used(c, c->o.sm.cur->d.name_id);
        if (used > 0) ticks = imax(ticks - COMBO_SAME_MOVE_HITSTUN_DECREASE_AMOUNT * (used + 1), ticks / 2);
    }
    if (started_above_0 && ticks <= 0) ticks = 1;
    return ticks;
}

static int real_victim_hitlag(const hitbox_t *h)
{
    fighter_t *c = creator_of(h);
    if (!c) return h->victim_hitlag;
    return c->combo_count <= 0 ? h->victim_hitlag : h->combo_victim_hitlag;
}

static fx real_knockback(const hitbox_t *h)
{
    fighter_t *c = creator_of(h);
    fx kb = h->def->knockback;
    if (!c || !h->host->is_fighter) return kb;
    int used = fighter_moves_used(c, c->o.sm.cur->d.name_id);
    if (used <= 0) return kb;
    fx k = c->opponent->o.chara.grounded ? FXC_KB_INCREASE_GROUNDED : FXC_KB_INCREASE_AERIAL;
    fx out = fx_mul(kb, fx_powu(k, (unsigned)used));
    return out > FXC_MAX_KNOCKBACK ? FXC_MAX_KNOCKBACK : out;
}

static bool is_counter_hit(const hitbox_t *h)
{
    obj_t *host = h->host;
    if (!host->is_fighter) return false;
    fighter_t *f = host->fighter;
    if (f->counterhit_this_turn) return true;
    fighter_t *op = f->opponent;
    state_t *os = op->o.sm.cur;
    bool a = h->def->can_counter_hit && f->initiative && os->d.has_hitboxes && os->d.cs && os->cs.can_be_counterhit;
    bool b = os->is_brace && 0;      /* is_brace: CounterAttack; tratado em can_counter_hitbox (TODO: exige hitdata) */
    return a || b;
}

void fighter_hit_data(hitdata_t *o, const hitbox_t *h)
{
    const hitbox_def_t *d = h->def;
    obj_t *host = h->host;
    memset(o, 0, sizeof *o);
    o->hit_height = d->hit_height;
    o->hitstun_ticks = real_hitstun(h);
    o->victim_hitlag = real_victim_hitlag(h);
    o->facing_int = host->chara.facing;
    o->box_facing = host->chara.facing >= 0 ? 1 : -1;   /* HitboxData.facing = facing do host */
    o->id = host->id;
    o->knockback = real_knockback(h);
    o->damage = h->damage;
    o->dir_x = d->dir_x; o->dir_y = d->dir_y;
    o->hitlag_ticks = d->hitlag_ticks;
    o->disable_collision = d->disable_collision;
    o->knockdown = d->knockdown;
    o->aerial_hit_state = d->aerial_hit_state; o->grounded_hit_state = d->grounded_hit_state;
    o->ground_bounce = d->ground_bounce; o->throw_ = d->is_throwbox;
    o->hard_knockdown = d->hard_knockdown; o->force_grounded = d->force_grounded; o->wall_slam = d->wall_slam;
    o->reversible = d->launch_reversible;
    o->pos_x = h->box.pos_x + box_x_facing(&h->box); o->pos_y = h->box.pos_y + h->box.y;
    o->is_projectile = !host->is_fighter;
    o->knockdown_extends_hitstun = d->knockdown_extends_hitstun;
    o->hits_otg = d->hits_otg; o->rumble = d->rumble;
    fighter_t *c = creator_of(h);
    o->combo_count = c ? c->combo_count : 0;
    o->screenshake_amount = d->screenshake_amount; o->screenshake_frames = d->screenshake_frames;
    o->counter_hit = is_counter_hit(h);
    o->minimum_damage = d->minimum_damage; o->sdi_modifier = d->sdi_modifier; o->di_modifier = d->di_modifier;
    o->increment_combo = d->increment_combo; o->ignore_armor = d->ignore_armor; o->damage_proration = d->damage_proration;
    o->parry_meter_gain = d->parry_meter_gain; o->hitbox_type = d->hitbox_type;
    o->damage_in_combo = d->damage_in_combo;
    if (o->damage_in_combo == -1) o->damage_in_combo = o->damage;
    o->hits_vs_dizzy = d->hits_vs_dizzy; o->meter_gain_modifier = d->meter_gain_modifier;
    o->air_ground_bounce = d->air_ground_bounce; o->scale_combo = d->scale_combo; o->plus_frames = d->plus_frames;
    o->vacuum = d->vacuum; o->send_away_from_center = d->send_away_from_center;
    o->minimum_grounded_frames = d->minimum_grounded_frames;
    o->chip_damage_modifier = d->chip_damage_modifier; o->block_pushback_modifier = d->block_pushback_modifier;
    o->hits_vs_standing = d->hits_vs_standing; o->combo_scaling_amount = d->combo_scaling_amount;
    o->ground_bounce_knockback_modifier = d->ground_bounce_knockback_modifier;
    o->hits_projectiles = d->hits_projectiles; o->cancellable = h->cancellable;
    o->followup_state = d->followup_state;
    o->guard_break = d->guard_break; o->parriable = d->parriable; o->block_punishable = d->block_punishable;
    o->looping = d->looping; o->ignore_projectile_armor = d->ignore_projectile_armor; o->block_cancel_allowed = d->block_cancel_allowed;
    o->allowed_to_hit_own_team = d->allowed_to_hit_own_team; o->block_pushback_reversible = d->block_pushback_reversible;
    o->block_reverse_pushback_modifier = d->block_reverse_pushback_modifier; o->guard_break_proration = d->guard_break_proration;
    o->host = host; o->source = h;
}

/* ---------- consultas ---------- */

bool fighter_is_otg(const fighter_t *f)
{
    sid s = f->o.sm.cur ? f->o.sm.cur->id : -1;
    return s == f->st_knockdown || s == f->st_hard_knockdown;
}

bool fighter_is_bracing(const fighter_t *f)
{
    const state_t *s = f->o.sm.cur;
    return s->d.kind == KIND_COUNTER && s->ustate[0] /* bracing */;
}

bool fighter_can_counter_hitbox(const fighter_t *f, const hitdata_t *h)
{
    if (!fighter_is_bracing(f)) return false;
    const state_t *s = f->o.sm.cur;
    switch (s->ustate[1] /* counter_type: 0 High, 1 Low, 2 Grab */) {
    case 2: return h->throw_;
    case 0: return !h->throw_ && (h->hit_height == HEIGHT_HIGH || h->hit_height == HEIGHT_MID);
    case 1: return !h->throw_ && h->hit_height == HEIGHT_LOW;
    }
    return false;
}

bool fighter_can_parry_hitbox(const fighter_t *f, const hitdata_t *h)
{
    const state_t *s = f->o.sm.cur;
    if (s->d.kind != KIND_PARRY_OLD && s->d.kind != KIND_PARRY_NEW) return false;
    return parry_can_parry_hitbox(f, s, h);
}

/* chave do hitbox na lista de parried */
static int hb_key(const hitbox_t *h) { return (int)(((intptr_t)h) & 0x7fffffff); }
static bool in_parried_list(const fighter_t *f, const hitdata_t *h)
{
    for (int i = 0; i < f->nparried; i++) if (f->hitbox_parried[i] == hb_key(h->source)) return true;
    return false;
}

/* ---------- combo ---------- */

void fighter_increment_opponent_combo(fighter_t *f, const hitdata_t *h)
{
    fighter_t *op = f->opponent;
    bool projectile = !h->host->is_fighter;
    bool will_scale = h->scale_combo || op->combo_count == 0;
    /* combo_scaling_disabled (projeteis especiais): por hook futuro */
    int old = op->combo_count;
    if (h->increment_combo) {
        fighter_incr_combo(op, will_scale, projectile, will_scale && projectile && h->scale_combo, h->combo_scaling_amount);
        if (h->scale_combo) {
            if (!op->combo_proration_set) {
                op->combo_proration = imin(h->damage_proration, MAX_STALES);
                op->combo_proration_set = true;
            }
            if (op->combo_count == 1 && old == 0 && op->air_movements_left < op->num_air_movements) op->air_movements_left++;
        }
    }
}

static bool is_self_hit(const fighter_t *f, const hitdata_t *h) { return h->host->fighter == f; }

/* ---------- ser lancado ---------- */

static hitdata_t *ring_push(fighter_t *f, const hitdata_t *h)
{
    hitdata_t *r = &f->hit_ring[f->hit_ring_pos++ & 3];
    *r = *h;
    return r;
}

void fighter_launched_by(fighter_t *f, hitdata_t *hin)
{
    obj_t *o = &f->o;
    hitdata_t *h = ring_push(f, hin);
    if (f->super_meter_used_recently > 0) {
        fighter_gain_super_meter_raw(f, f->super_meter_used_recently / SUPER_METER_GRACE_DIVISOR);
        f->super_meter_used_recently = 0; f->super_meter_grace_ticks = 0;
    }
    fighter_apply_hitlag(f, h);
    f->feinting = false;

    obj_t *host = h->host;
    if (host) {
        int hl = rnd(fx_mul(fxi(h->hitlag_ticks), f->global_hitstop_modifier));
        if (host->hitlag_ticks < hl) host->hitlag_ticks = hl;
    }
    f->nudge_amount = h->sdi_modifier;
    bool projectile = !host->is_fighter;

    bool autoblock = f->vt && f->vt->has_autoblock_armor && f->vt->has_autoblock_armor(f);
    bool armor = f->has_hyper_armor;
    bool proj_armor = f->has_projectile_armor;
    bool will_launch = h->ignore_armor || !armor;
    if (!h->ignore_armor && projectile && proj_armor && !h->ignore_projectile_armor) will_launch = false;
    bool will_block = false;
    if (will_launch && autoblock && !h->ignore_projectile_armor && !h->ignore_armor) { will_launch = false; will_block = !projectile; }

    int scaling_offset = h->combo_scaling_amount - 1;
    bool self_hit = is_self_hit(f, h);

    if (will_launch) {
        sid st = o->chara.grounded ? h->grounded_hit_state : h->aerial_hit_state;
        if (st == f->st_hurt_grounded) {
            f->grounded_hits_taken++;
            if (f->grounded_hits_taken >= MAX_GROUNDED_HITS && !h->force_grounded) { st = f->st_hurt_aerial; f->grounded_hits_taken = 0; }
        }
        if (!self_hit) fighter_increment_opponent_combo(f, h);
        sm_change_state(o, st, h, true, true);
        f->was_my_turn = false;
        if (h->disable_collision) f->colliding_with_opponent = false;
        f->busy_interrupt = true;
        f->can_nudge = true;
        if (!projectile) { fighter_refresh_feints(f); fighter_refresh_feints(f->opponent); }
        if (f->vt && f->vt->on_launched) f->vt->on_launched(f);
    } else if (will_block) {
        sm_change_state(o, autoblock ? f->st_parry_auto : f->st_parry_high, NULL, true, true);
        fighter_block_hitbox(f, h, false, true, false, autoblock);
    }
    if (f->has_hyper_armor) f->hit_during_armor = true;

    int damage = h->combo_count > 0 ? h->damage_in_combo : h->damage;
    if (will_block) damage = rnd(fx_mul(fxi(damage), FX_HALF));
    if (h->counter_hit) damage = rnd(fx_mul(fxi(damage), FXC_COUNTER_HIT_DAMAGE));
    fighter_take_damage(f, damage, h->minimum_damage, h->meter_gain_modifier, scaling_offset, FX_ONE, self_hit, will_block);
    if (will_launch) obj_state_tick(o);
}

void fighter_thrown_by(fighter_t *f, hitdata_t *h)
{
    (void)h;
    sm_change_state(&f->o, f->st_grabbed, NULL, true, true);
}

/* ---------- hit_by ---------- */

void fighter_hit_by(fighter_t *f, hitdata_t *h, bool force_hit)
{
    obj_t *o = &f->o;
    if (f->parried) return;
    if (in_parried_list(f, h)) return;
    if (!h->hits_otg && fighter_is_otg(f)) return;
    if (!h->hits_vs_dizzy && o->sm.cur->id == f->st_hurt_dizzy) return;
    if (fighter_can_counter_hitbox(f, h)) f->braced_attack = true;
    else if (o->sm.cur->d.kind == KIND_COUNTER) f->hit_out_of_brace = true;

    if (h->throw_ && !fighter_is_otg(f)) { fighter_thrown_by(f, h); return; }
    if (force_hit || !fighter_can_parry_hitbox(f, h)) {
        bool self_hit = is_self_hit(f, h);
        switch (h->hitbox_type) {
        case HB_NORMAL: case HB_BURST:
            fighter_launched_by(f, h); break;
        case HB_NOHITSTUN: {
            fighter_t *ref = self_hit ? f : f->opponent;
            fighter_take_damage(f, ref->combo_count <= 0 ? h->damage : h->damage_in_combo, 0, FX_ONE, 0, FX_ONE, self_hit, false);
            break; }
        case HB_FLIP:
            f->o.chara.facing = -f->o.chara.facing;
            f->o.chara.vx = -f->o.chara.vx;
            obj_update_boxes(o);
            if (!self_hit) fighter_increment_opponent_combo(f, h);
            fighter_take_damage(f, h->combo_count > 0 ? h->damage_in_combo : h->damage, h->minimum_damage, h->meter_gain_modifier, 0, FX_ONE, self_hit, false);
            break;
        case HB_THROWHIT:
            fighter_apply_hitlag(f, h);
            fighter_apply_hitlag(f->opponent, h);
            fighter_take_damage(f, h->combo_count > 0 ? h->damage_in_combo : h->damage, h->minimum_damage, h->meter_gain_modifier, 0, FX_ONE, self_hit, false);
            if (!self_hit) fighter_incr_combo(f->opponent, h->scale_combo, false, false, h->combo_scaling_amount);
            break;
        case HB_OFFENSIVE_BURST:
            f->opponent->hitstun_decay_combo_count = 0;
            fighter_launched_by(f, h);
            phys_reset_pushback(&o->chara);
            phys_reset_pushback(&f->opponent->o.chara);
            break;
        }
    } else {
        fighter_block_hitbox(f, h, false, false, false, false);
    }
}

/* ---------- bloqueio / parry ---------- */

void fighter_block_hitbox(fighter_t *f, hitdata_t *h, bool force_parry, bool force_block, bool ignore_guard_break, bool autoblock_armor)
{
    obj_t *o = &f->o;
    fighter_t *op = f->opponent;
    obj_t *host = h->host;
    bool projectile = !host->is_fighter;
    state_t *s = o->sm.cur;
    bool is_new = s->d.kind == KIND_PARRY_NEW;
    bool perfect;
    const parry_data_t *pd = (const parry_data_t *)s->data;
    int parry_type = s->pr.parry_type;
    int input_timing = (pd ? pd->timing : 0) + (f->blocked_hitbox_plus_frames - op->blocked_hitbox_plus_frames);
    int parry_timing = 0;
    o->state_interruptable = false;
    bool pk_perfect = f->vt && f->vt->can_perfect_parry ? f->vt->can_perfect_parry(f) : true;
    const state_t *os = op->o.sm.cur;

    if (!projectile) {
        if (is_new) {
            parry_timing = f->turn_frames + op->o.hitlag_ticks;
            bool in_window = (parry_timing == input_timing || (input_timing >= 20 && f->turn_frames >= 20)) ||
                             (h->hitbox_type == HB_BURST && f->combo_count > 0);
            bool req_nh = pk_perfect && !parry_whiffed_block(f) && (os->feinting || op->feinting || f->initiative) && h->parriable;
            bool req = req_nh && parry_matches_height(s, h, -1);
            perfect = s->d.can_parry && ((pk_perfect && f->always_perfect_parry) || (in_window && req && !f->blocked_last_hit));
            if (op->feint_parriable) perfect = true;
        } else {
            parry_timing = f->turn_frames;
            perfect = s->d.can_parry && (f->always_perfect_parry || os->feinting || op->feinting || (f->initiative && !f->blocked_last_hit) || f->parried_last_state);
            perfect = perfect && h->parriable;
        }
    } else {
        perfect = s->d.can_parry && !s->d.push && (f->always_perfect_parry || eng_proj_always_parriable(host) || f->parried_last_state ||
                  (s->current_tick < PROJECTILE_PERFECT_PARRY_WINDOW && eng_proj_has_parry_window(host)));
    }
    if (force_parry) perfect = true;
    if (force_block) perfect = false;

    if (perfect) {
        f->parried_last_state = true;
        f->last_parry_tick = o->current_tick;
        if (!h->block_punishable && !projectile) {
            if (h->hitbox_type == HB_BURST) f->parried_burst_combo = true; else f->parry_combo = true;
        }
    } else {
        f->blocked_last_hit = true;
        f->blocked_last_turn = true;
        o->throw_invulnerable = true;
        if (!projectile && !perfect && !f->last_turn_block && f->initiative) {
            bool can_gb = op->vt && op->vt->can_guard_break ? op->vt->can_guard_break(op) : true;
            if (h->guard_break && !ignore_guard_break && !parry_whiffed_block(f) && can_gb) {
                if (h->guard_break_proration > h->damage_proration) h->damage_proration = h->guard_break_proration;
                fighter_hit_by(f, h, true);
                if (s->d.kind == KIND_PARRY_NEW || s->d.kind == KIND_PARRY_OLD) { s->hu.guard_broken = true; f->guard_broken_this_turn = true; }
                return;
            }
        }
    }

    if (!projectile) {
        op->feinting = false;
        if (!perfect) { op->got_blocked = true; if (op->vt && op->vt->on_attack_blocked) { /* on_blocked_melee_attack e hook do atacante */ } }
        else { op->got_parried = true; if (csd(op->o.sm.cur)) op->o.sm.cur->cs.interruptible_on_opponent_turn = 0; }
        if (!is_new) op->o.sm.cur->feinting = false;
    }

    f->parried = true;
    o->hitlag_ticks = 0;
    if (f->nparried < 8) f->hitbox_parried[f->nparried++] = hb_key(h->source);

    if (is_new) eng_set_parry_anim(f, s, h->hit_height == HEIGHT_LOW || !o->chara.grounded);

    int parry_meter = h->parry_meter_gain == -1 ? PARRY_METER : h->parry_meter_gain;
    parry_parry(f, s, perfect);
    s = o->sm.cur;

    if (!perfect) {
        f->last_turn_block = true;
        int chip = rnd(fx_mul(fxi(h->damage / (projectile ? f->parry_chip_projectile_divisor : f->parry_chip_divisor)), h->chip_damage_modifier));
        bool push_block = s->d.push;
        if (push_block) chip = rnd(fx_mul(fxi(chip), FXC_PUSH_BLOCK_CHIP_MOD));
        if (!parry_matches_height(s, h, parry_type))
            chip += rnd(fx_mul(fx_mul(fxi(h->damage / f->parry_chip_divisor), FX_HALF), h->chip_damage_modifier));
        if (!autoblock_armor) fighter_take_damage(f, chip, 0, 39322 /* 0.6 */, 0, fxi(2), false, false);
        fighter_gain_super_meter(op, parry_meter / 6, FX_ONE);
        int block_hitlag = h->hitlag_ticks + 1;

        if (!projectile) {
            f->block_used_air_movement = true;
            if (autoblock_armor) op->o.hitlag_ticks = 0;
            state_t *ost = op->o.sm.cur;
            s->d.anim_length = ost->d.anim_length; s->d.endless = ost->d.endless;
            if (csd(s) && csd(ost)) s->cs.iasa_at = ost->cs.iasa_at;
            s->current_tick = 0;
            if (op->vt && op->vt->on_attack_blocked) op->vt->on_attack_blocked(op);
            ost->character_state_was_blocked = true;
            op->blockstun_ticks += block_hitlag;
            fighter_add_penalty(op, -10, false);
            if (op->feints < op->num_feints) op->feints++;
            bool can_bc = op->vt && op->vt->can_block_cancel ? op->vt->can_block_cancel(op) : true;
            if (!(h->looping && !h->cancellable) && h->block_cancel_allowed && can_bc) {
                if (!h->block_punishable || autoblock_armor) {
                    cs_enable_interrupt(f, s, true, false);
                    if (csd(ost)) ost->cs.interruptible_on_opponent_turn = 1;
                    op->last_turn_block = true;
                } else cs_enable_interrupt(f, s, true, false);
            }
            int od = fighter_opponent_dir(f);
            if (fx_sign(od) == fx_sign(o->chara.vx)) o->chara.vx = 0;
            fx bpm = h->block_pushback_modifier;
            if (h->block_pushback_reversible) {
                int dir = isign(h->pos_x - phys_x(&o->chara));
                fx m = FX_ONE;
                if ((dir == -1 && h->box_facing < 0) || (dir == 1 && h->box_facing > 0)) m = -FX_ONE;
                if (m == -FX_ONE) m = fx_mul(h->block_reverse_pushback_modifier, m);
                bpm = fx_mul(bpm, m);
            }
            if (s->d.push && bpm < 0) bpm = -bpm;
            int dd = od ? od : 1;
            fx pb = fx_mul(fx_div(h->knockback, fx_mul(fxi(dd), -f->parry_knockback_divisor)), bpm);
            if (!o->chara.grounded) pb = fx_mul(pb, FXC_AIR_BLOCK_PUSHBACK);
            phys_force(&o->chara, pb, 0);
            phys_force_rel(&op->o.chara, fx_mul(fx_div(h->knockback, fx_mul(f->parry_knockback_divisor, -fxi(2))), bpm), 0);
        }
        if (s->d.push) {
            fx dist = 0;
            if (projectile) { int dx = phys_x(&host->chara) - phys_x(&o->chara), dy = phys_y(&host->chara) - phys_y(&o->chara); dist = fx_vec_len(fxi(dx), fxi(dy)); }
            if (!projectile || dist <= FXC_PUSH_BLOCK_DIST) {
                phys_force(&op->o.chara, fx_mul(fxi(fighter_opponent_dir(op)), FXC_PUSH_BLOCK_FORCE), 0);
                eng_fighter_got_push_blocked(op);
            }
            if (projectile) eng_host_got_push_blocked(host);
        }
        f->blocked_hitbox_plus_frames = BASE_PLUS_FRAMES;
        if (csd(s)) s->cs.interruptible_on_opponent_turn = 1;
        int total = h->plus_frames;
        if (!parry_matches_height(s, h, parry_type)) total += WRONG_HIT_HEIGHT_ADDITIONAL_PLUS_FRAMES;
        if (s->d.push) total += PUSH_BLOCK_ADVANTAGE_PENALTY;
        if (!projectile) {
            total += BASE_PLUS_FRAMES;
            if (!o->chara.grounded) total += VS_AERIAL_ADDITIONAL_PLUS_FRAMES;
        }
        fighter_set_block_stun(f, total, projectile ? 0 : block_hitlag);
        f->parried = false;
        if (projectile) phys_force_rel(&o->chara, fx_mul(fx_div(h->knockback, fx_mul(FXC_PARRY_KB_DIVISOR, -FX_ONE)), h->block_pushback_modifier), 0);
        eng_sfx(o, SFX_BLOCK);
        eng_host_got_blocked(host, f);
        if (f->vt && f->vt->on_blocked_something) f->vt->on_blocked_something(f);
        if (h->hitbox_type == HB_FLIP) { o->chara.facing = -o->chara.facing; obj_update_boxes(o); }
    } else {
        if (!projectile) { fighter_gain_super_meter(f, parry_meter, FX_ONE); fighter_add_penalty(f, -20, false); }
        else { fighter_add_penalty(f, -10, false); fighter_gain_super_meter(f, parry_meter / 3, FX_ONE); }
        eng_host_got_parried(host, f);
        eng_sfx(o, SFX_PARRY);
    }
}

/* ---------- Hitbox.hit ---------- */

void eng_hitbox_hit(hitbox_t *hb, obj_t *victim)
{
    const hitbox_def_t *d = hb->def;
    if (hitbox_already_hit(hb, victim->uid)) return;
    if (victim->invulnerable && d->hitbox_type != HB_THROWHIT) return;
    if (!(!(victim->is_fighter && fighter_is_otg(victim->fighter)) || d->hits_otg)) return;
    hitbox_save_hit(hb, victim->uid);
    if (d->hitbox_type == HB_DETECT) { eng_host_detect(hb->host, victim); return; }
    if (!victim->is_fighter) return;           /* projeteis como alvo: projectile.c (hit_by do projetil) */
    fighter_t *vf = victim->fighter;
    hitdata_t hd;
    fighter_hit_data(&hd, hb);
    fighter_hit_by(vf, &hd, false);
    bool can_hit = true;
    obj_t *host = hb->host;
    if (host->is_fighter) {
        state_t *hs = host->sm.cur;
        if (hs->d.cs && hs->cs.end_feint) { host->fighter->feinting = false; hs->feinting = false; }
    }
    if (fighter_can_parry_hitbox(vf, &hd) || in_parried_list(vf, &hd)) can_hit = false;
    if (fighter_can_counter_hitbox(vf, &hd)) can_hit = false;
    if (vf->on_the_ground && !d->hits_otg) can_hit = false;
    if (!d->hits_vs_dizzy && victim->sm.cur->id == vf->st_hurt_dizzy) can_hit = false;
    if (can_hit) {
        fighter_t *hf = host->fighter;
        fx pm = fx_mul(host->is_fighter ? fxi(hf->hitstun_decay_combo_count) : 0, 26214 /* 0.4 */);
        fx pb = fx_mul(d->pushback_x + pm, -FX_ONE);
        pb = fx_div(pb, fxi(2));
        phys_add_pushback(&host->chara, pb);
        phys_add_pushback(&victim->chara, pb);
        if (vf->opponent && &vf->opponent->o != host) phys_add_pushback(&vf->opponent->o.chara, pb);
        if (host->is_fighter) cs_on_hit_something(host->fighter, host->sm.cur, victim, hb);
    }
}
