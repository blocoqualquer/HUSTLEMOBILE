/* Lutador: porte de BaseChar.gd (recursos, penalidade, combo, dano, DI, tick). Combate (hit_by/bloqueio) fica em combat.c. */
#include "fighter.h"
#include <string.h>
#include <stddef.h>

#define AERIAL_VEL_GAIN FXC_AERIAL_VEL_SUPER_GAIN

static inline fx fxi(int v) { return FX_INT(v); }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int isign(int v) { return (v > 0) - (v < 0); }

int fighter_opponent_dir(const fighter_t *f)
{
    return isign(phys_x(&f->opponent->o.chara) - phys_x(&f->o.chara));
}

/* centro da hurtbox em coordenadas absolutas (get_hurtbox_center) */
static void hurt_center(const fighter_t *f, int *x, int *y)
{
    box_t b = f->o.hurtbox;
    box_update_position(&b, phys_x(&f->o.chara), phys_y(&f->o.chara));
    b.facing = f->o.chara.facing;
    *x = box_center_x(&b);
    *y = box_center_y(&b);
}

/* obj_local_center(opponent): centro do oponente relativo ao meu */
static void local_center(const fighter_t *f, int *dx, int *dy)
{
    int mx, my, ox, oy;
    hurt_center(f, &mx, &my);
    hurt_center(f->opponent, &ox, &oy);
    *dx = ox - mx;
    *dy = oy - my;
}

int fighter_name_id(const fighter_t *f, const char *name)
{
    for (int i = 0; i < f->nnames; i++)
        if (f->names[i] && strcmp(f->names[i], name) == 0) return i;
    return -1;
}

/* ---------- medidores ---------- */

void fighter_gain_burst(fighter_t *f)
{
    if (f->bursts_available < MAX_BURSTS) { f->bursts_available++; f->burst_meter = 0; }
}

/* passive: amount < 0 significa "null" (ganho passivo) */
void fighter_gain_burst_meter(fighter_t *f, int amount, bool passive)
{
    if (!f->burst_enabled || f->penalty_ticks > 0) return;
    if (f->bursts_available >= MAX_BURSTS) return;
    int speed = passive ? BURST_BUILD_SPEED : amount;
    if (passive) {
        if (f->combo_count <= 0 && f->opponent->combo_count <= 0) {
            speed -= 1;
            if (!f->o.chara.grounded) speed -= 1;
        }
    }
    if (f->burst_cancel_combo) speed -= 3;
    f->burst_meter += speed;
    if (f->burst_meter > MAX_BURST_METER) fighter_gain_burst(f);
}

void fighter_use_super_bar(fighter_t *f)
{
    if (f->infinite_resources) return;
    f->supers_available--;
    if (f->supers_available < 0) { f->supers_available = 0; f->super_meter = 0; }
    f->super_meter_grace_ticks = SUPER_METER_GRACE_PERIOD;
    f->super_meter_used_recently += MAX_SUPER_METER;
}

static void use_super_meter(fighter_t *f, int amount)
{
    if (f->infinite_resources) return;
    f->super_meter -= amount;
    if (f->super_meter < 0) {
        if (f->supers_available > 0) { f->super_meter = MAX_SUPER_METER + f->super_meter; fighter_use_super_bar(f); }
        else f->super_meter = 0;
    }
}
void fighter_use_super_meter(fighter_t *f, int amount) { use_super_meter(f, amount); }

fx fighter_get_combo_stale(int count)
{
    fx ratio = fx_div(fxi(MAX_STALES - imin(count, MAX_STALES)), fxi(MAX_STALES));
    fx mod = fx_mul(FX_ONE - FXC_MIN_STALE_MODIFIER, fx_powu(ratio, 2));
    return mod + FXC_MIN_STALE_MODIFIER;
}

static int combo_stale_meter(const fighter_t *f, int meter)
{
    fx staling = fighter_get_combo_stale(f->combo_count);
    fx k = f->o.current_tick > 0 ? FXC_METER_GAIN_MODIFIER : FX_ONE;
    return (int)fx_round(fx_mul(fx_mul(fxi(meter), staling), k));
}

static int meter_gain_modified(const fighter_t *f, int amount)
{
    if (f->penalty_ticks > 0) return 0;
    fx pen = fx_div(fxi(f->penalty), fxi(MAX_PENALTY));
    if (f->penalty <= 0) pen = fx_div(pen, fxi(5));
    return (int)fx_round(fx_mul(FX_ONE - pen, fxi(amount)));
}

void fighter_gain_super_meter_raw(fighter_t *f, int amount)
{
    f->super_meter += amount;
    while (f->super_meter >= MAX_SUPER_METER) {
        if (f->supers_available < MAX_SUPERS) {
            f->super_meter -= MAX_SUPER_METER;
            f->supers_available++;
            /* TODO: som SuperGain */
        } else {
            f->super_meter = MAX_SUPER_METER;
            break;
        }
    }
}

void fighter_gain_super_meter(fighter_t *f, int amount, fx stale_amount)
{
    int full = combo_stale_meter(f, amount);
    amount = (int)fx_round(fx_lerp(fxi(amount), fxi(full), stale_amount));
    amount = meter_gain_modified(f, amount);
    fx den = fx_powu(fxi(2), (unsigned)f->combo_supers);
    int modded = (int)fx_round(fx_div(fxi(amount), den));
    amount = (int)fx_round(fx_lerp(fxi(amount), fxi(modded), stale_amount));
    fighter_gain_super_meter_raw(f, amount);
}

void fighter_drain_super_meter(fighter_t *f, int amount)
{
    if (f->infinite_resources) return;
    f->super_meter -= amount;
    if (f->super_meter < 0) {
        if (f->supers_available > 0) { f->super_meter += MAX_SUPER_METER; f->supers_available--; }
        else f->super_meter = 0;
    }
}

void fighter_use_air_movement(fighter_t *f)
{
    if (f->infinite_resources) return;
    if (f->air_movements_left > 0) f->air_movements_left--;
}

void fighter_refresh_air_movements(fighter_t *f)
{
    if (!f->lose_one_air_option_in_neutral) { f->air_movements_left = f->num_air_movements; return; }
    if (f->num_air_movements == 0) { f->air_movements_left = 0; return; }
    f->air_movements_left = f->combo_count == 0 ? imax(f->num_air_movements - 1, 1) : f->num_air_movements;
}

/* ---------- penalidade ("sadness") ---------- */

void fighter_add_penalty(fighter_t *f, int amount, bool ignore_min_distance)
{
    if (!f->sadness_enabled || f->sadness_immunity_ticks > 0) return;
    bool g = f->o.chara.grounded, og = f->opponent->o.chara.grounded;
    if (amount > 0) {
        if (!ignore_min_distance) {
            int dx, dy; local_center(f, &dx, &dy);
            if (fx_vec_len(fxi(dx), fxi(dy)) < fxi(128)) return;
        }
        fx m = FX_ONE;
        if (g && !og) m = 16384;            /* 0.25 */
        amount = (int)fx_round(fx_mul(fxi(amount), m));
    } else {
        fx m = (!g && og) ? 49152 : FX_ONE;   /* 0.75 */
        amount = (int)fx_round(fx_mul(fxi(amount), m));
    }
    if (f->hp < f->opponent->hp) {
        int diff = f->hp - f->opponent->hp; if (diff < 0) diff = -diff;
        fx ratio = FX_ONE - fx_div(fxi(diff), fxi(3000));
        amount = (int)fx_round(fx_mul(fxi(amount), ratio));
    }
    f->penalty += amount;
    if (f->penalty > MAX_PENALTY) {
        f->supers_available = 0; f->super_meter = 0; f->penalty = 0; f->penalty_buffer = 0;
        f->penalty_ticks = PENALTY_TICKS;
    }
    if (f->penalty < MIN_PENALTY) f->penalty = MIN_PENALTY;
}

static int sadness_distance_penalty(const fighter_t *f)
{
    if (f->combo_count > 0 || fighter_is_in_hurt_state(f->opponent, false)) return 1;
    int dx, dy; local_center(f, &dx, &dy);
    int amount = 0;
    if (dy > 0) amount = (int)fx_round(fx_div(fxi(dy), fxi(180)));
    return 1 + amount;
}

static void passive_sadness_gain(fighter_t *f)
{
    int dir = fx_sign(f->last_vel.x), od = fighter_opponent_dir(f);
    int t = f->o.current_tick;
    if (dir != 0 && dir != od && t % 3 == 0) fighter_add_penalty(f, 1, false);
    if (dir != 0 && dir == od && t % 4 == 0)
        if (fx_abs(f->last_vel.x) > 655) fighter_add_penalty(f, -1, false);
    if (t % 7 == 0) fighter_add_penalty(f, sadness_distance_penalty(f), false);
}

static fx penalty_damage_modifier(const fighter_t *f)
{
    const int minp = 20;
    if (f->penalty_ticks > 0) return 98304;     /* 1.5 */
    if (f->penalty < minp) return FX_ONE;
    return FX_ONE + fx_mul(fx_div(fxi(f->penalty - minp), fxi(MAX_PENALTY - minp)), FX_HALF);
}

/* ---------- combo / dano ---------- */

void fighter_stack_move(fighter_t *f, int name_id)
{
    for (int i = 0; i < f->nmoves_used; i++)
        if (f->moves_used[i].name_id == name_id) { f->moves_used[i].count++; return; }
    if (f->nmoves_used < MOVES_USED_MAX) { f->moves_used[f->nmoves_used].name_id = name_id; f->moves_used[f->nmoves_used++].count = 1; }
}

int fighter_moves_used(const fighter_t *f, int name_id)
{
    for (int i = 0; i < f->nmoves_used; i++)
        if (f->moves_used[i].name_id == name_id) return f->moves_used[i].count;
    return 0;
}

void fighter_reset_combo(fighter_t *f)
{
    fighter_t *op = f->opponent;
    f->combo_count = 0; f->o.combo_count = 0;
    f->visible_combo_count = 0; f->combo_damage = 0; f->hitstun_decay_combo_count = 0;
    f->combo_proration = 0; f->combo_proration_set = false; f->combo_proration_ready = false;
    f->nmoves_used = 0; f->burst_cancel_combo = false; f->combo_supers = 0;
    op->grounded_hits_taken = 0;
    op->trail_hp = op->hp;
    op->wall_slams = 0; op->hit_out_of_brace = false; op->braced_attack = false; op->brace_effect_applied_yet = false;
    f->parry_combo = false; f->parried_burst_combo = false;
    if (f->lose_one_air_option_in_neutral && f->num_air_movements == f->air_movements_left)
        fighter_refresh_air_movements(f);
}

void fighter_incr_combo(fighter_t *f, bool scale, bool projectile, bool force, int combo_scale_amount)
{
    if ((scale && (!f->melee_attack_combo_scaling_applied || projectile)) || force) {
        f->combo_count += combo_scale_amount;
        f->o.combo_count = f->combo_count;
        f->hitstun_decay_combo_count += 1;
    }
    f->visible_combo_count += 1;
}

static int combo_stale_damage(const fighter_t *f, int damage, int offset, bool self_hit)
{
    const fighter_t *src = self_hit ? f : f->opponent;
    int n = src->combo_count - offset + (src->combo_count > 1 ? src->combo_proration : 0) - 1;
    fx st = fighter_get_combo_stale(imax(n, 0));
    return (int)fx_round(fx_mul(fxi(damage), st));
}

void fighter_take_damage(fighter_t *f, int damage, int minimum, fx meter_gain_modifier, int combo_scaling_offset,
                         fx damage_taken_meter_gain_modifier, bool self_hit, bool armor_block)
{
    fighter_t *op = f->opponent;
    fighter_t *ref = self_hit ? f : op;
    if (ref->combo_count == 0) f->trail_hp = f->hp;
    if (damage == 0) return;

    fighter_gain_burst_meter(f, damage / BURST_ON_DAMAGE_AMOUNT, false);
    damage = imax(combo_stale_damage(f, damage, combo_scaling_offset, self_hit), 1);
    damage = imax(damage, minimum);
    /* guts: GUTS_REDUCTIONS so tem 1.0 -> 1.0 */
    damage = imax(damage, 1);
    if (!self_hit) {
        if (op->parried_burst_combo) damage = (int)fx_round(fx_mul(fxi(damage), f->burst_parry_combo_scaling));
        else if (op->parry_combo) damage = (int)fx_round(fx_mul(fxi(damage), f->parry_combo_scaling));
    }
    damage = (int)fx_round(fx_mul(fxi(damage), penalty_damage_modifier(f)));
    int meter_gain = (int)fx_round(fx_mul(fxi(damage / 1 /* DAMAGE_SUPER_GAIN_DIVISOR */), meter_gain_modifier));
    if (!self_hit) fighter_gain_super_meter(op, meter_gain, FX_ONE);
    fighter_gain_super_meter(f, (int)fx_round(fx_mul(fxi(damage / DAMAGE_TAKEN_SUPER_GAIN_DIVISOR), damage_taken_meter_gain_modifier)), FX_ONE);
    damage = (int)fx_round(fx_mul(fx_mul(fxi(damage), f->damage_taken_modifier), f->global_damage_modifier));
    if (!self_hit) op->combo_damage += damage;
    f->hp -= damage;
    fighter_add_penalty(f, -25, false);
    if (f->hp < 0) f->hp = 0;
    bool armor = (f->has_hyper_armor) || (f->vt && f->vt->has_autoblock_armor && f->vt->has_autoblock_armor(f)) || armor_block;
    if (f->hp <= 0 && armor) f->hp = 1;
    state_t *s = f->o.sm.cur;
    if (s && s->d.kind == KIND_PARRY_NEW && s->d.push && f->hp <= 0) f->hp = 1;
}

/* ---------- DI ---------- */

fx fighter_get_di_scaling(const fighter_t *f, bool brace, int lookahead)
{
    if (brace && f->hit_out_of_brace) return 0;
    fx max_extra = f->max_di_scaling - f->min_di_scaling;
    const fighter_t *op = f->opponent;
    int eff = op->combo_count + lookahead, dic = eff;
    if (f->prorated_di && eff > 1 && op->combo_proration > 0 && op->combo_proration_ready)
        dic += imin(op->combo_proration, MAX_STALES);
    int n = dic < 0 ? 0 : (dic > f->di_combo_limit ? f->di_combo_limit : dic);
    fx ratio = fx_div(fxi(n), fxi(f->di_combo_limit));
    fx total = f->min_di_scaling + fx_mul(max_extra, ratio);
    if (brace && f->braced_attack) total = fx_mul(total, FXC_BRACE_DI);
    return fx_mul(total, f->di_modifier);
}

/* xy_to_dir(x, y, scale): componentes limitados a [-100,100] e escalados; ver BaseObj.xy_to_dir */
fxv obj_xy_to_dir(fx x, fx y, fx scale)
{
    fx ux = fx_div(x, fxi(100)), uy = fx_div(y, fxi(100));
    return fxv_make(fx_mul(ux, scale), fx_mul(uy, scale));
}

fxv fighter_get_scaled_di(const fighter_t *f, fxv di)
{
    fx scaling = fighter_get_di_scaling(f, true, 0);
    fxv r = obj_xy_to_dir(di.x, di.y, scaling);
    fx len = fx_vec_len(r.x, r.y);
    if (fx_abs(len - scaling) < FXC_DI_SNAP_DISTANCE || len > scaling) r = fxv_norm_times(r, scaling);
    return r;
}

bool fighter_is_in_hurt_state(const fighter_t *f, bool count_all)
{
    const state_t *s = f->o.sm.cur;
    if (!s) return false;
    bool hurt = state_is_hurt(s);
    if (count_all) return hurt || (s->d.cs && s->d.cs->busy_interrupt_type == 1 /* Hurt */);
    return hurt;
}

/* ---------- bloqueio / hitlag ---------- */

void fighter_set_block_stun(fighter_t *f, int total_plus_frames, int block_hitlag)
{
    if (block_hitlag < 0 && total_plus_frames >= 0) block_hitlag = total_plus_frames;
    else if (block_hitlag < 0) block_hitlag = total_plus_frames;
    if (total_plus_frames > 0) f->blocked_hitbox_plus_frames = total_plus_frames;
    else if (total_plus_frames < 0) f->opponent->blocked_hitbox_plus_frames = -total_plus_frames;
    f->blockstun_ticks += block_hitlag;
}

void fighter_apply_hitlag(fighter_t *f, const hitdata_t *h)
{
    int t = h->victim_hitlag + (h->counter_hit ? 0 /* COUNTER_HIT_ADDITIONAL_HITLAG_FRAMES */ : 0);
    if (f->braced_attack) t = (int)fx_round(fx_mul(fxi(t), FXC_BRACE_HITSTUN));
    t = (int)fx_round(fx_mul(fxi(t), f->global_hitstop_modifier));
    f->o.hitlag_ticks = t;
    f->hitlag_applied = t;
}

void fighter_update_facing(fighter_t *f)
{
    int ax = phys_x(&f->o.chara), bx = phys_x(&f->opponent->o.chara);
    int face = ax < bx ? 1 : (ax > bx ? -1 : 0);
    if (face == 0) return;
    if (f->reverse_state) face = -face;
    f->o.chara.facing = face;
}

/* ---------- vantagem / iniciativa ---------- */

static bool get_advantage(fighter_t *f)
{
    fighter_t *op = f->opponent;
    if (!op) return true;
    if (f->combo_count > 0) return true;
    bool adv = op->lowest_tick <= f->lowest_tick || f->parried_last_state;
    if (f->o.state_interruptable && op->o.state_interruptable) adv = true;
    return adv;     /* WhiffInstantCancel: tratado por personagem (TODO) */
}

void fighter_update_advantage(fighter_t *f)
{
    bool n = get_advantage(f);
    if (n && !f->initiative) f->initiative_effect = true;
    f->initiative = n;
}

void fighter_enable_interrupt(fighter_t *f, state_t *s, bool check_opponent)
{
    (void)s; (void)check_opponent;
    f->o.state_interruptable = true;
    f->was_my_turn = true;
    if (f->prorated_di && f->opponent->combo_count > 0 && f->opponent->combo_proration > 0) f->opponent->combo_proration_ready = true;
}

void fighter_enable_hit_cancel(fighter_t *f, bool projectile)
{
    f->o.state_hit_cancellable = true;
    if (projectile) f->projectile_hit_cancelling = true;
    if (f->prorated_di && f->opponent->combo_count > 0 && f->opponent->combo_proration > 0) f->opponent->combo_proration_ready = true;
}

/* ---------- selecao de acao ---------- */

void fighter_select_action(fighter_t *f, int action, void *data, const action_extra_t *extra)
{
    f->queued_action = action;
    f->queued_data = data;
    f->has_extra = extra != NULL;
    if (extra) {
        f->ex_has_di = extra->has_di; f->ex_di = extra->di;
        f->ex_has_reverse = extra->has_reverse; f->ex_reverse = extra->reverse;
        f->ex_feint = extra->feint;
    }
    f->o.state_interruptable = false;
    f->o.state_hit_cancellable = false;
}

static void process_extra(fighter_t *f)
{
    if (f->ex_has_di) {
        if (f->di_enabled) { f->current_nudge = obj_xy_to_dir(f->ex_di.x, f->ex_di.y, fxi(NUDGE_DISTANCE)); f->current_di = f->ex_di; }
        else f->current_di = fxv_make(0, 0);
    }
    if (f->ex_has_reverse) f->reverse_state = f->ex_reverse;
    if (f->ex_feint >= 0) {
        f->feinting = f->ex_feint != 0;
        if (f->feinting && !f->infinite_resources) {
            if (f->feints > 0) f->feints--;
            else if (!f->used_buffer) fighter_use_super_bar(f);
        }
    } else f->feinting = false;
}

/* ---------- tick ---------- */

void fighter_tick_before(fighter_t *f)
{
    obj_t *o = &f->o;
    if (f->clashing) {
        obj_change_state(o, o->chara.grounded ? f->st_wait : f->st_fall, NULL);
        f->clashing = false;
    }
    if (f->vt && f->vt->turn_end_effects) f->vt->turn_end_effects(f);
    f->dummy_interruptable = false;
    f->nparried = 0 > f->nparried ? 0 : f->nparried;   /* clean_parried_hitboxes: tratado em combat.c */
    f->busy_interrupt = false;
    phys_update_grounded(&o->chara);

    f->feinted_last = f->feinting;
    bool pressed_feint = false;
    f->blocked_last_turn = false;
    if (f->use_buffer && f->buffered_valid) {
        f->queued_action = f->buffered_action; f->queued_data = f->buffered_data;
        f->has_extra = f->buffered_has_extra; f->ex_has_di = f->b_ex_has_di; f->ex_di = f->b_ex_di;
        f->ex_has_reverse = f->b_ex_has_reverse; f->ex_reverse = f->b_ex_reverse; f->ex_feint = f->b_ex_feint;
        f->buffered_valid = false; f->use_buffer = false; f->used_buffer = true;
    }
    if (f->has_extra) {
        f->turn_frames = 0; f->opponent->turn_frames = 0;
        process_extra(f);
        pressed_feint = f->feinting;
    }
    if (f->queued_action != ACTION_NONE) {
        if (f->vt && f->vt->process_action) f->vt->process_action(f, f->queued_action);
        f->turn_frames = 0; f->opponent->turn_frames = 0;
        if (f->vt && f->vt->turn_start_effects) f->vt->turn_start_effects(f);
        f->counterhit_this_turn = false; f->guard_broken_this_turn = false; f->gained_whiff_meter = false;
        if (f->brace_effect_applied_yet) { f->brace_effect_applied_yet = false; f->braced_attack = false; }
        f->feint_parriable = false;
        /* "Continue": resolvido em states_common (hold restarts / fallback) -> fighter_resolve_continue */
        if (f->queued_action == ACTION_CONTINUE) {
            if (f->feinted_last) f->feint_parriable = true;
            f->queued_action = fighter_resolve_continue(f);
        }
        if (f->queued_action >= 0) {
            sm_change_state(o, (sid)f->queued_action, f->queued_data, true, true);
            state_t *cs = o->sm.cur;
            if (cs->d.kind != KIND_HURT_GROUNDED && cs->d.kind != KIND_HURT_AERIAL) {
                if (!f->last_turn_block) o->hitlag_ticks = 0;
                f->last_turn_block = false;
            }
            if (cs->d.kind != KIND_PARRY_OLD && cs->d.kind != KIND_PARRY_NEW) {
                if (f->blocked_hitbox_plus_frames > 0) {
                    o->hitlag_ticks += f->blocked_hitbox_plus_frames;
                    f->blocked_hitbox_plus_frames = 0;
                }
            }
            if (pressed_feint) { f->feinting = true; cs->feinting = true; }
        }
        o->sm.cur->feinted_last = f->feinted_last;
    }
    f->queued_action = ACTION_NONE; f->queued_data = NULL; f->has_extra = false;
    f->was_my_turn = false;
    f->lowest_tick = o->sm.cur->current_real_tick;
}

void fighter_tick(fighter_t *f)
{
    obj_t *o = &f->o;
    if (o->hitlag_ticks > 0) {
        if (f->can_nudge) {
            if (fx_round(fx_mul(fx_vec_len(f->current_nudge.x, f->current_nudge.y), fxi(100))) > 1) {
                f->current_nudge = fxv_mul(f->current_nudge, f->nudge_amount);
                phys_move(&o->chara, f->current_nudge.x, o->chara.grounded ? 0 : f->current_nudge.y);
            }
            f->can_nudge = false;
        }
        o->hitlag_ticks--;
        if (o->hitlag_ticks == 0) {
            if (o->state_hit_cancellable || f->projectile_hit_cancelling) { o->state_interruptable = true; f->can_nudge = false; }
        }
        f->turn_frames++;
    } else if (f->blockstun_ticks > 0) {
        f->blockstun_ticks--;
    } else {
        f->turn_frames++;
        if (f->projectile_hit_cancelling) { o->state_interruptable = true; f->can_nudge = false; }
        f->projectile_hit_cancelling = false;
        if (f->parried) {
            f->parried = false;
            o->sm.cur->pr.parry_active = false;
        }
        obj_state_tick(o);
        if (f->in_blockstring && o->sm.cur->d.kind != KIND_PARRY_NEW) f->in_blockstring = false;
        if (o->state_hit_cancellable) { o->state_interruptable = true; f->can_nudge = false; }
        state_t *s = o->sm.cur;
        if (s->d.cs && s->d.cs->apply_pushback && s->d.kind != KIND_THROW) phys_apply_pushback(&o->chara, fighter_opponent_dir(f));
        if (o->chara.grounded) { f->used_air_dodge = false; fighter_refresh_air_movements(f); }
        o->current_tick++; o->game_tick++;
        state_t *os = f->opponent->o.sm.cur;
        bool me_hurt = state_is_hurt(s);
        bool op_hurt = state_is_hurt(os);
        if (!me_hurt && !op_hurt) {
            int xv = (int)fx_to_int(o->chara.vx);              /* get_x_vel_int: parte inteira */
            if (isign(xv) == isign(phys_x(&f->opponent->o.chara) - phys_x(&o->chara))) {
                fx mult = s->d.cs ? s->d.cs->velocity_forward_meter_gain_multiplier : FX_ONE;
                fx g = fx_abs(fx_mul(fxi(xv), mult));
                if (!o->chara.grounded) g = fx_mul(g, AERIAL_VEL_GAIN);
                int gain = (int)fx_round(fx_div(g, fxi(VEL_SUPER_GAIN_DIVISOR)));
                fighter_gain_super_meter(f, imax(gain, 0), FX_ONE);
            }
        }
    }
    if (o->state_interruptable) phys_update_grounded(&o->chara);
    if (f->hit_during_armor) { f->has_hyper_armor = false; f->hit_during_armor = false; }
    if (f->parried_last_state && o->current_tick - f->last_parry_tick >= AUTO_PARRY_TICKS) f->parried_last_state = false;
    if (f->wakeup_throw_immunity_ticks > 0) f->wakeup_throw_immunity_ticks--;
    if (f->sadness_immunity_ticks > 0) f->sadness_immunity_ticks--;

    fighter_gain_burst_meter(f, 0, true);
    obj_update_boxes(o);
    state_t *s = o->sm.cur;
    if (s->d.cs && s->d.cs->beats_backdash) f->buffer_moved_forward = true;
    if (s->d.cs && s->d.cs->backdash_iasa) f->buffer_moved_backward = true;
    o->can_update_sprite = true;
    f->last_vel = fxv_make(o->chara.vx, o->chara.vy);
    if (!fighter_is_in_hurt_state(f, true) && !fighter_is_in_hurt_state(f->opponent, true) && f->combo_count <= 0 && f->penalty_ticks <= 0)
        passive_sadness_gain(f);
    f->last_pos_x = phys_x(&o->chara); f->last_pos_y = phys_y(&o->chara);
    if (f->penalty_ticks > 0) {
        f->penalty_ticks--;
        if (o->current_tick % 3 == 0 && f->hp > 0) {
            fighter_take_damage(f, 2, 1, FX_ONE, 0, FX_ONE, false, false);
            if (f->hp <= 0) f->hp = 1;
        }
    }
    f->touching_wall = false;
    aabb_t cb = box_aabb(&o->collision_box);
    if (cb.x1 <= -f->stage_width && f->last_vel.x < 0) f->touching_wall = true;
    if (cb.x2 >= f->stage_width && f->last_vel.x > 0) f->touching_wall = true;
    if (!(s->d.kind == KIND_PARRY_OLD || s->d.kind == KIND_PARRY_NEW)) f->parried_last_state = false;
    f->used_buffer = false;
    f->grounded_last_frame = o->chara.grounded;
    if (f->super_meter_grace_ticks > 0) {
        f->super_meter_grace_ticks--;
        if (f->super_meter_grace_ticks == 0) f->super_meter_used_recently = 0;
    }
}

void fighter_init(fighter_t *f, fighter_t *opponent, state_t *states, int nstates, sid start, int id)
{
    const struct fighter_vt_s *vt = f->vt; const char *const *names = f->names; int nnames = f->nnames; const chardata_t *cd = f->cd;
    memset(&f->hp, 0, sizeof(*f) - offsetof(fighter_t, hp));
    f->vt = vt; f->names = names; f->nnames = nnames; f->cd = cd;
    f->st_hurt_grounded = fighter_name_id(f, "HurtGrounded"); f->st_hurt_aerial = fighter_name_id(f, "HurtAerial");
    f->st_knockdown = fighter_name_id(f, "Knockdown"); f->st_hard_knockdown = fighter_name_id(f, "HardKnockdown");
    f->st_hurt_dizzy = fighter_name_id(f, "HurtDizzy"); f->st_parry_high = fighter_name_id(f, "ParryHigh");
    f->st_parry_auto = fighter_name_id(f, "ParryAuto"); f->st_grabbed = fighter_name_id(f, "Grabbed");
    f->st_wait = fighter_name_id(f, "Wait"); f->st_fall = fighter_name_id(f, "Fall");
    f->opponent = opponent;
    f->o.id = id;
    f->o.is_fighter = true;
    f->o.fighter = f;
    f->max_health = MAX_HEALTH_DEFAULT;
    f->hp = f->trail_hp = f->max_health;
    f->bursts_available = START_BURSTS;
    f->burst_enabled = f->di_enabled = f->sadness_enabled = true;
    f->prorated_di = true;
    f->min_di_scaling = FX_ONE; f->max_di_scaling = fxi(6); f->di_combo_limit = 15;
    f->nudge_amount = FX_ONE; f->di_modifier = FX_ONE; f->knockback_taken_modifier = FX_ONE; f->damage_taken_modifier = FX_ONE;
    f->global_damage_modifier = f->global_hitstun_modifier = f->global_hitstop_modifier = FX_ONE;
    f->parry_knockback_divisor = FXC_PARRY_KB_DIVISOR; f->parry_combo_scaling = FXC_PARRY_COMBO_SCALING;
    f->burst_parry_combo_scaling = FXC_PARRY_COMBO_SCALING;
    f->parry_chip_divisor = PARRY_CHIP_DIVISOR; f->parry_chip_projectile_divisor = PARRY_CHIP_PROJECTILE_DIVISOR;
    f->queued_action = ACTION_NONE;
    f->stage_width = 1000;
    f->lowest_tick = 0;
    f->initiative = true;
    if (f->cd) {                                   /* parametros fisicos e caixas do personagem */
        phys_init(&f->o.chara);
        f->o.chara.grav = f->cd->gravity; f->o.chara.gfric = f->cd->ground_friction; f->o.chara.afric = f->cd->air_friction;
        f->o.chara.max_gspeed = f->cd->max_ground_speed; f->o.chara.max_aspeed = f->cd->max_air_speed; f->o.chara.max_fall = f->cd->max_fall_speed;
        f->o.hurtbox = f->o.def_hurtbox = f->cd->hurtbox; f->o.collision_box = f->cd->collision_box;
        f->o.gravity_enabled = true;
    }
    obj_init(&f->o, states, nstates, start);
}

void fighter_refresh_feints(fighter_t *f) { f->feints = f->num_feints; }
__attribute__((weak)) void cs_enable_interrupt(fighter_t *f, state_t *s, bool c, bool r) { (void)r; fighter_enable_interrupt(f, s, c); }

void fighter_set_facing(fighter_t *f, int facing, bool force)
{
    if (f->reverse_state && !force) facing = -facing;
    if (facing != 1 && facing != -1) facing = 1;
    f->o.chara.facing = facing;
    obj_update_boxes(&f->o);
}

void fighter_set_anim(fighter_t *f, state_t *s, const char *name)
{
    if (!f->cd) return;
    for (int i = 0; i < f->cd->nanims; i++)
        if (strcmp(f->cd->anims[i].name, name) == 0) { s->anim = i; return; }
}

void fighter_release_opponent(fighter_t *f)
{
    fighter_t *op = f->opponent;
    if (op->o.sm.cur->id == op->st_grabbed) sm_change_state(&op->o, op->st_fall, NULL, true, true);
}

void fighter_start_sadness_immunity(fighter_t *f) { f->sadness_immunity_ticks = SADNESS_IMMUNITY_TICKS; }

fx fighter_hitbox_x_dir(const fighter_t *f, const hitdata_t *h)
{
    fx x = h->dir_x * h->box_facing;
    if (h->reversible) {
        int dir = (h->pos_x > phys_x(&f->o.chara)) - (h->pos_x < phys_x(&f->o.chara));
        int m = 1;
        if ((dir == -1 && h->box_facing < 0) || (dir == 1 && h->box_facing > 0)) m = -1;
        x *= m;
    }
    return x;
}

/* ---------- "Continue": mantem/reinicia o estado enquanto o jogador segura (BaseChar.tick_before) ---------- */

static const char *const HOLD_RESTARTS[] = { "Wait", "Fall", "DashForward", "ParryHigh", "BlockHigh", "ParryLow", "ParryAir", NULL };

int fighter_resolve_continue(fighter_t *f)
{
    state_t *s = f->o.sm.cur;
    const char *cname = s->d.name;
    bool opp_turn = s->d.cs && s->cs.interruptible_on_opponent_turn;
    int action = ACTION_CONTINUE;
    if (f->vt && f->vt->process_continue && f->vt->process_continue(f)) return f->queued_action;
    int hr = state_hold_restart(f, s);
    if (hr >= 0 && opp_turn) { action = hr; f->queued_data = s->data; }
    else {
        bool restart = false;
        for (int i = 0; HOLD_RESTARTS[i]; i++) if (strcmp(cname, HOLD_RESTARTS[i]) == 0) restart = true;
        if (restart && opp_turn) { action = s->id; f->queued_data = s->data; }
        else if (opp_turn && strcmp(cname, "ParrySuper") == 0) action = fighter_name_id(f, "ParryHigh");
        else if (opp_turn && strcmp(cname, "DashBackward") == 0) action = fighter_name_id(f, "Wait");
        else if (s->d.cs && s->cs.next_state_on_hold && !f->feinting &&
                 (f->was_my_turn || (opp_turn && s->cs.next_state_on_hold_on_opponent_turn) || (s->hit_fighter && f->combo_count == 0)))
            action = s->d.fallback_state;
    }
    return action;
}

/* get_hold_restart(): Dash aereo -> AirDashForward/Fall; padrao = hold_restart exportado (-1 = nenhum) */
__attribute__((weak)) int state_hold_restart(fighter_t *f, state_t *s)
{
    if (xp_find(&s->d, "dir_x") && xp_int(&s->d, "air", 0)) return f->air_movements_left > 0 ? fighter_name_id(f, "AirDashForward") : f->st_fall;
    return s->d.cs ? s->cs.hold_restart : -1;
}
