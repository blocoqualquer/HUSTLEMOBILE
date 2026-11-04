/* Lutador (BaseChar.gd): vida, medidores, combo, dano, DI, bloqueio/parry e fluxo de turno.
 * Fora do escopo (nao existem no PSP): estilos/auras, rede, conquistas, emotes, hooks de mods, fantasmas de previsao. */
#ifndef FIGHTER_H
#define FIGHTER_H
#include "engine.h"
#include "chardata.h"

#define MAX_HEALTH_DEFAULT 1500
#define MAX_STALES 15
#define MAX_GROUNDED_HITS 7
#define MAX_WALL_SLAMS 3
#define MAX_BURSTS 1
#define BURST_BUILD_SPEED 4
#define MAX_BURST_METER 1500
#define START_BURSTS 1
#define MAX_SUPER_METER 125
#define MAX_SUPERS 9
#define VEL_SUPER_GAIN_DIVISOR 4
#define NUDGE_DISTANCE 20
#define PARRY_METER 50
#define MIN_PENALTY (-20)
#define MAX_PENALTY 75
#define PENALTY_TICKS 120
#define PROJECTILE_PERFECT_PARRY_WINDOW 3
#define AUTO_PARRY_TICKS 20
#define BURST_ON_DAMAGE_AMOUNT 5
#define DAMAGE_TAKEN_SUPER_GAIN_DIVISOR 3
#define HITLAG_COLLISION_TICKS 4
#define SADNESS_IMMUNITY_TICKS 60
#define WAKEUP_THROW_IMMUNITY_TICKS 3
#define SUPER_METER_GRACE_PERIOD 3
#define SUPER_METER_GRACE_DIVISOR 2
#define PARRY_CHIP_DIVISOR 3
#define PARRY_CHIP_PROJECTILE_DIVISOR 4
#define BASE_PLUS_FRAMES 0
#define VS_AERIAL_ADDITIONAL_PLUS_FRAMES 2
#define WRONG_HIT_HEIGHT_ADDITIONAL_PLUS_FRAMES 2
#define PUSH_BLOCK_ADVANTAGE_PENALTY 0
#define COMBO_SAME_MOVE_HITSTUN_DECREASE_AMOUNT 1

/* constantes fixas (strings no original, ja convertidas para Q16) */
#define FXC_MIN_STALE_MODIFIER   ((fx)13107)    /* 0.2 */
#define FXC_DI_SNAP_DISTANCE     ((fx)655)      /* 0.01 */
#define FXC_METER_GAIN_MODIFIER  ((fx)55706)    /* 0.85 */
#define FXC_PARRY_COMBO_SCALING  ((fx)55706)    /* 0.85 */
#define FXC_PUSH_BLOCK_CHIP_MOD  ((fx)21627)    /* 0.33 */
#define FXC_AIR_BLOCK_PUSHBACK   ((fx)22938)    /* 0.35 */
#define FXC_COUNTER_HIT_DAMAGE   ((fx)72090)    /* 1.1 */
#define FXC_BRACE_HITSTUN        ((fx)22938)    /* 0.35 */
#define FXC_BRACE_DI             ((fx)98304)    /* 1.5 */
#define FXC_PARRY_KB_DIVISOR     ((fx)196608)   /* 3 */
#define FXC_PARRY_GROUNDED_KB_DIVISOR ((fx)98304) /* 1.5 */
#define FXC_AERIAL_VEL_SUPER_GAIN ((fx)32768)   /* 0.5 */
#define FXC_MAX_KNOCKBACK        FX_INT(30)
#define FXC_KB_INCREASE_GROUNDED ((fx)81920)    /* 1.25 */
#define FXC_KB_INCREASE_AERIAL   ((fx)68813)    /* 1.05 */
#define FXC_PUSH_BLOCK_FORCE     FX_INT(-10)
#define FXC_PUSH_BLOCK_DIST      FX_INT(220)

typedef struct fighter_s fighter_t;

/* HitboxData: foto do hitbox no momento do acerto */
typedef struct {
    int id, hit_height, hitstun_ticks, facing_int, pos_x, pos_y, hitlag_ticks, victim_hitlag, damage, combo_count;
    int screenshake_frames, screenshake_amount, minimum_damage, damage_proration, parry_meter_gain, damage_in_combo;
    int minimum_grounded_frames, plus_frames, combo_scaling_amount, hitbox_type, guard_break_proration;
    fx knockback, dir_x, dir_y, sdi_modifier, di_modifier, meter_gain_modifier, chip_damage_modifier, block_pushback_modifier;
    fx ground_bounce_knockback_modifier, block_reverse_pushback_modifier;
    sid aerial_hit_state, grounded_hit_state, followup_state;
    bool counter_hit, knockdown, disable_collision, ground_bounce, air_ground_bounce, hits_otg, parriable, reversible, throw_;
    bool knockdown_extends_hitstun, rumble, increment_combo, ignore_armor, force_grounded, hard_knockdown, wall_slam, hits_vs_dizzy;
    bool is_projectile, scale_combo, vacuum, send_away_from_center, hits_vs_standing, hits_projectiles, cancellable, guard_break;
    bool block_punishable, ignore_projectile_armor, looping, block_cancel_allowed, allowed_to_hit_own_team, block_pushback_reversible;
    int box_facing;                         /* +1/-1 do hitbox */
    obj_t *host;
    const hitbox_t *source;                 /* hitbox de origem (para parried_hitboxes) */
} hitdata_t;

#define ACTION_NONE (-1)
#define ACTION_CONTINUE (-2)
#define MOVES_USED_MAX 16

struct fighter_s {
    obj_t o;
    fighter_t *opponent;
    int stage_width, max_health;
    /* recursos */
    int hp, trail_hp, super_meter, supers_available, burst_meter, bursts_available, combo_supers;
    int penalty, penalty_ticks, penalty_buffer, sadness_immunity_ticks;
    int air_movements_left, num_air_movements, feints, num_feints;
    bool lose_one_air_option_in_neutral;
    /* combo */
    int combo_count, visible_combo_count, combo_damage, combo_proration, hitstun_decay_combo_count, grounded_hits_taken, wall_slams;
    bool combo_proration_set, combo_proration_ready, parry_combo, parried_burst_combo, burst_cancel_combo;
    struct { int name_id, count; } moves_used[MOVES_USED_MAX]; int nmoves_used;
    bool melee_attack_combo_scaling_applied;
    /* turno e estado */
    int turn_frames, blockstun_ticks, blocked_hitbox_plus_frames, lowest_tick, hitlag_applied, wakeup_throw_immunity_ticks, last_parry_tick;
    int super_meter_used_recently, super_meter_grace_ticks, buffered_global_hitlag;
    bool initiative, initiative_effect, busy_interrupt, was_my_turn, dummy_interruptable, colliding_with_opponent, clipping_wall;
    bool parried, got_parried, got_blocked, parried_last_state, blocked_last_hit, blocked_last_turn, last_turn_block, in_blockstring;
    bool feinting, feinted_last, feint_parriable, reverse_state, touching_wall, can_nudge, hit_during_armor, has_hyper_armor, has_projectile_armor;
    bool projectile_hit_cancelling, counterhit_this_turn, guard_broken_this_turn, gained_whiff_meter, block_used_air_movement;
    bool brace_enabled, hit_out_of_brace, braced_attack, brace_effect_applied_yet, used_buffer, use_buffer, used_air_dodge;
    bool moved_forward, moved_backward, buffer_moved_forward, buffer_moved_backward, on_the_ground, sadness_enabled, game_over;
    bool burst_enabled, di_enabled, prorated_di, infinite_resources, one_hit_ko, always_perfect_parry, clashing, grounded_last_frame;
    int action, action_data_i; void *action_data;          /* acao escolhida (sid ou ACTION_*) e dados */
    int queued_action; void *queued_data;
    bool has_extra, ex_has_di, ex_has_reverse, ex_reverse; fxv ex_di; int ex_feint;           /* "extra" da acao: DI, reverse, feint (-1: ausente) */
    bool buffered_valid; int buffered_action; void *buffered_data; bool buffered_has_extra, b_ex_has_di, b_ex_has_reverse, b_ex_reverse; fxv b_ex_di; int b_ex_feint;
    const struct fighter_vt_s *vt; const char *const *names; int nnames; const chardata_t *cd;
    /* DI e knockback */
    fxv current_di, current_nudge, last_vel; fx nudge_amount, min_di_scaling, max_di_scaling, di_modifier, knockback_taken_modifier, damage_taken_modifier;
    fx global_damage_modifier, global_hitstun_modifier, global_hitstop_modifier, parry_knockback_divisor, parry_combo_scaling, burst_parry_combo_scaling;
    int di_combo_limit, parry_chip_divisor, parry_chip_projectile_divisor;
    int last_pos_x, last_pos_y;
    /* parry/hurt: dados "passados ao estado" ficam aqui (anel de HitboxData) */
    hitdata_t hit_ring[4]; int hit_ring_pos;
    int hitbox_parried[8]; int nparried;
    int throw_pos_x, throw_pos_y;
    /* cache de nomes de estado */
    sid st_wait, st_fall, st_parry_high, st_parry_air, st_dash_forward, st_block_high, st_parry_low, st_parry_super, st_dash_backward, st_grabbed,
        st_force_explosion, st_parry_auto, st_throw_tech, st_hurt_grounded, st_hurt_aerial, st_knockdown, st_hard_knockdown, st_hurt_dizzy;
    int stance;
};

void fighter_init(fighter_t *f, fighter_t *opponent, state_t *states, int nstates, sid start, int id);
void fighter_tick_before(fighter_t *f);
void fighter_tick(fighter_t *f);
typedef struct { bool has_di, has_reverse, reverse; fxv di; int feint; } action_extra_t;   /* feint: -1 ausente, 0/1 */
void fighter_select_action(fighter_t *f, int action, void *data, const action_extra_t *extra);
int  fighter_name_id(const fighter_t *f, const char *name);
typedef struct { int timing; int height_x, height_y; } parry_data_t;      /* data dos estados de parry ("Melee Parry Timing", "Block Height") */

/* ganchos por personagem (metodos virtuais de BaseChar/Fighter); NULL = comportamento padrao */
typedef struct fighter_vt_s {
    void (*process_action)(fighter_t *, int action);
    void (*turn_end_effects)(fighter_t *);
    void (*turn_start_effects)(fighter_t *);
    void (*on_launched)(fighter_t *);
    bool (*has_autoblock_armor)(fighter_t *);
    bool (*process_continue)(fighter_t *);
    bool (*can_block_cancel)(fighter_t *);
    bool (*can_guard_break)(fighter_t *);
    bool (*can_perfect_parry)(fighter_t *);
    void (*on_attack_blocked)(fighter_t *);
    void (*on_blocked_something)(fighter_t *);
    bool (*is_in_install_super)(fighter_t *);
} fighter_vt_t;

/* medidores */
void fighter_gain_burst_meter(fighter_t *f, int amount, bool passive);
void fighter_gain_super_meter(fighter_t *f, int amount, fx stale_amount);
void fighter_gain_super_meter_raw(fighter_t *f, int amount);
void fighter_use_super_bar(fighter_t *f);
void fighter_add_penalty(fighter_t *f, int amount, bool ignore_min_distance);
void fighter_gain_burst(fighter_t *f);
void fighter_use_super_meter(fighter_t *f, int amount);
void fighter_drain_super_meter(fighter_t *f, int amount);
void fighter_use_air_movement(fighter_t *f);
fxv  obj_xy_to_dir(fx x, fx y, fx scale);
int  fighter_resolve_continue(fighter_t *f);   /* "Continue": hold restarts / fallback (states_common) */
/* combate */
void fighter_hit_data(hitdata_t *out, const hitbox_t *h);
void fighter_hit_by(fighter_t *f, hitdata_t *hit, bool force_hit);
void fighter_take_damage(fighter_t *f, int damage, int minimum, fx meter_gain_modifier, int combo_scaling_offset, fx damage_taken_meter_gain_modifier, bool self_hit, bool armor_block);
void fighter_reset_combo(fighter_t *f);
void fighter_incr_combo(fighter_t *f, bool scale, bool projectile, bool force, int combo_scale_amount);
fx   fighter_get_combo_stale(int count);
fx   fighter_get_di_scaling(const fighter_t *f, bool brace, int lookahead);
fxv  fighter_get_scaled_di(const fighter_t *f, fxv di);
int  fighter_opponent_dir(const fighter_t *f);
void fighter_stack_move(fighter_t *f, int name_id);
int  fighter_moves_used(const fighter_t *f, int name_id);
bool fighter_is_in_hurt_state(const fighter_t *f, bool count_all);
void fighter_refresh_air_movements(fighter_t *f);
void fighter_set_block_stun(fighter_t *f, int total_plus_frames, int block_hitlag /* -1: igual ao total */);
void fighter_apply_hitlag(fighter_t *f, const hitdata_t *h);
void fighter_enable_interrupt(fighter_t *f, state_t *s, bool check_opponent);
void fighter_enable_hit_cancel(fighter_t *f, bool projectile);
void fighter_update_facing(fighter_t *f);
void fighter_update_advantage(fighter_t *f);
static inline fighter_t *obj_fighter(obj_t *o) { return o->fighter; }
static inline fighter_t *fighter_of(obj_t *o) { return (fighter_t *)o; }   /* valido quando o->is_fighter */


/* ---- camada de estados (states_common.c / parry.c) usada pelo combate ---- */
enum { PH_HIGH, PH_LOW, PH_BOTH };
bool parry_can_parry_hitbox(const fighter_t *f, const state_t *s, const hitdata_t *h);
bool parry_matches_height(const state_t *s, const hitdata_t *h, int parry_type /* -1: do estado */);
bool parry_whiffed_block(const fighter_t *f);
void parry_parry(fighter_t *f, state_t *s, bool perfect);
void cs_enable_interrupt(fighter_t *f, state_t *s, bool check_opponent, bool remove_hitlag);
bool fighter_can_parry_hitbox(const fighter_t *f, const hitdata_t *h);
bool fighter_can_counter_hitbox(const fighter_t *f, const hitdata_t *h);
bool fighter_is_bracing(const fighter_t *f);
bool fighter_is_otg(const fighter_t *f);
void fighter_hit_by_dispatch(fighter_t *f, hitdata_t *h, bool force_hit);
void fighter_block_hitbox(fighter_t *f, hitdata_t *h, bool force_parry, bool force_block, bool ignore_guard_break, bool autoblock_armor);
void fighter_launched_by(fighter_t *f, hitdata_t *h);
void fighter_thrown_by(fighter_t *f, hitdata_t *h);
void fighter_increment_opponent_combo(fighter_t *f, const hitdata_t *h);
void fighter_refresh_feints(fighter_t *f);
/* dados de acao com direcao (Jump/Dash/DashStartup): x,y em -100..100 */
typedef struct { int x, y; bool auto_correct, charged, has_charged; } xy_data_t;
void fighter_set_facing(fighter_t *f, int facing, bool force);
void fighter_set_anim(fighter_t *f, state_t *s, const char *name);
void fighter_release_opponent(fighter_t *f);
void fighter_start_sadness_immunity(fighter_t *f);
fx   fighter_hitbox_x_dir(const fighter_t *f, const hitdata_t *h);
/* camada CharacterState */
sid  cs_enter_shared(obj_t *o, state_t *s);
sid  cs_tick_shared(obj_t *o, state_t *s);
sid  cs_tick_after(obj_t *o, state_t *s);
void cs_exit_shared(obj_t *o, state_t *s);
sid  cs_frame_shared(obj_t *o, state_t *s, int tick);
bool cs_can_interrupt(const state_t *s);
void cs_on_hit_something(fighter_t *host, state_t *s, obj_t *victim, const hitbox_t *hb);

#endif
