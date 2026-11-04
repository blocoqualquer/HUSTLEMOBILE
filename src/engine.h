/* Nucleo da simulacao: objeto base (BaseObj.gd), maquina de estados (StateMachine.gd/StateInterface.gd)
 * e a parte guiada por dados de ObjectState.gd. Estados sao identificados por indice (sid) em vez de nome. */
#ifndef ENGINE_H
#define ENGINE_H
#include "fx.h"
#include "phys.h"
#include "hitbox.h"

#define STATE_STACK 32
#define SID_NONE (-1)

typedef int sid;
typedef struct obj_s obj_t;
typedef struct state_s state_t;

/* Comportamento por script (porte manual dos .gd). Qualquer ponteiro pode ser NULL.
 * Funcoes que retornam sid devolvem o proximo estado ou SID_NONE (o GDScript devolve nome ou null). */
typedef struct {
    sid  (*enter_shared)(obj_t *, state_t *);   /* camada CharState (chama obj_state_enter_shared antes) */
    sid  (*enter)(obj_t *, state_t *);
    void (*tick_before)(obj_t *, state_t *);
    sid  (*tick_shared)(obj_t *, state_t *);    /* se NULL usa obj_state_tick_shared */
    sid  (*tick)(obj_t *, state_t *);
    sid  (*tick_after)(obj_t *, state_t *);
    void (*exit_shared)(obj_t *, state_t *);    /* se NULL usa obj_state_exit_shared */
    void (*exit)(obj_t *, state_t *);
    sid  (*frame_shared)(obj_t *, state_t *, int tick);   /* _frame_N_shared */
    sid  (*frame)(obj_t *, state_t *, int tick);          /* _frame_N */
    sid  (*animation_finished)(obj_t *, state_t *);
} state_vt_t;


/* nome/categoria internado por personagem; conjuntos de ate 256 nomes (estados + categorias de interrupcao) */
typedef struct { uint32_t w[8]; } nameset_t;
static inline bool ns_has(const nameset_t *s, int id) { return id >= 0 && (s->w[id >> 5] >> (id & 31)) & 1; }
static inline void ns_add(nameset_t *s, int id) { if (id >= 0) s->w[id >> 5] |= 1u << (id & 31); }
static inline void ns_del(nameset_t *s, int id) { if (id >= 0) s->w[id >> 5] &= ~(1u << (id & 31)); }
static inline bool ns_empty(const nameset_t *s) { for (int i = 0; i < 8; i++) if (s->w[i]) return false; return true; }

/* exports de CharState.gd (CharacterState) */
typedef struct {
    uint8_t type, air_type, busy_interrupt_type;
    uint8_t uses_air_movement, ignore_air_option_use_in_combos, land_cancel, throw_techable, interruptible_on_opponent_turn,
            update_facing_on_exit, dynamic_iasa, backdash_iasa, allow_framecheat, next_state_on_hold, next_state_on_hold_on_opponent_turn,
            combo_only, neutral_only, end_feint, usable_from_whiff_cancel_if_possible, count_projectile_hits, burst_cancellable, burstable,
            self_hit_cancellable, self_interruptable, reversible, instant_cancellable, force_feintable, can_feint_if_possible, selectable,
            always_usable, release_opponent_on_startup, release_opponent_on_exit, initiative_effect, apply_pushback, beats_backdash,
            can_be_counterhit, force_super_effect, super_effect, scale_combo_meter, show_in_menu, flip_icon, flip_with_facing;
    int landing_recovery, min_land_cancel_frame, iasa_at, iasa_on_hit, initiative_startup_reduction_amount, no_collision_start_frame,
        no_collision_end_frame, tick_priority, super_level, supers_used, super_freeze_ticks;
    sid land_cancel_state, hold_restart;
    fx velocity_forward_meter_gain_multiplier, whiff_meter_gain_multiplier;
    nameset_t interrupt_from, interrupt_into, hit_cancel_into, hit_cancel_exceptions, interrupt_exceptions, allowed_stances;
    const int16_t *interrupt_frames; int ninterrupt_frames;
    const char *title;
} charstate_def_t;

enum { KIND_NORMAL, KIND_HURT_GROUNDED, KIND_HURT_AERIAL, KIND_PARRY_OLD, KIND_PARRY_NEW, KIND_COUNTER, KIND_THROW };

/* dados constantes do estado (exports de ObjectState, ja convertidos para fx) */
typedef struct {
    const char *name;
    const state_vt_t *vt;
    uint8_t apply_forces, apply_fric, apply_grav, reset_momentum, reset_x_momentum, reset_y_momentum;
    uint8_t apply_custom_x_fric, apply_custom_y_fric, apply_custom_grav, apply_forces_no_limit, apply_custom_limits;
    uint8_t loop_animation, absolute_loop, refresh_loop, endless, disable_at_end, force_same_direction_as_previous_state;
    uint8_t spawn_particle_on_enter, projectile_match_facing, projectile_local_pos;
    fx custom_x_fric, custom_y_fric, custom_grav, custom_grav_max_fall_speed, custom_max_air_speed, custom_max_ground_speed;
    int anim_length, sprite_anim_length, ticks_per_frame, animation_loop_start;
    sid fallback_state;
    fx force_dir_x, force_dir_y, force_speed; int force_tick;           /* ja pre-divididos: forca = dir*speed */
    fx enter_force_x, enter_force_y; uint8_t has_enter_force;           /* forca de entrada ja resolvida (relativa a facing) */
    fx force_x, force_y; uint8_t has_force;
    int state_screenshake_tick, flip_frame, sfx_tick, projectile_tick, timed_spawn_particle_tick;
    int extra_parry_hitlag;
    int sprite_anim;                                                    /* indice da animacao no atlas */
    const hitbox_def_t *hb; int nhb;                                    /* hitboxes filhos do estado */
    const hurtstate_def_t *hurt; int nhurt;                             /* HurtboxState (troca a hurtbox em ticks) */
    const hurtstate_def_t *limb; int nlimb;                             /* LimbHurtbox (hurtboxes extras) */
    /* exports de ParryState/NewParry (kind PARRY_*) */
    uint8_t can_parry, push, use_guard_sprites, autoguard, disable_aerial_movement, reblock; int parry_type, particle_x, particle_y;
    uint8_t kind;                                                       /* KIND_*: papel do estado no combate (hurt, parry, counter, throw) */
    uint8_t is_grab, has_hitboxes, is_guard_break;                      /* derivados dos hitboxes (preenchidos em obj_init) */
    int earliest_hitbox;
    const charstate_def_t *cs;                                          /* NULL para estados que nao sao CharacterState */
    int name_id;                                                        /* id internado do nome do estado */
} state_def_t;

struct state_s {
    const state_def_t *def;           /* definicao constante (identidade do estado) */
    state_def_t d;                    /* copia mutavel: scripts alteram anim_length, endless, fallback_state, etc. */
    charstate_def_t cs;               /* idem para CharacterState (listas de interrupcao mutaveis); valido se d.cs != NULL */
    int anim;                         /* animacao atual (anim_name pode ser trocado em runtime) */
    sid id;
    bool active;
    void *data;                       /* dados passados na troca de estado (ver queue_state) */
    int current_tick, current_real_tick, start_tick, last_facing, exit_tick;
    bool same_as_last_state, was_blocked; int number_of_hits_blocked;
    int max_tick, max_tick_shared;
    int ustate[8];                    /* variaveis privadas do script do estado (membros 'var' do .gd) */
    hitbox_t *hbr;                    /* hitboxes em runtime (def->nhb itens) */
    /* variaveis de CharacterState */
    bool hit_yet, hit_anything, hit_fighter, character_state_was_blocked, hit_cancelled, started_during_combo, feinting, land_cancelled;
    bool feinted_last, started_in_air, entered_in_air, hit_hit_cancellable_projectile, initiative_startup_reduction, is_brace;
    /* ParryState / NewParry */
    struct { bool parry_active, perfect, parried, punishable, parried_last, started_in_combo; int parry_type, initial_parry_type, parry_tick, extra_iasa; } pr;
    /* CharacterHurtState / HurtGrounded / HurtAerial */
    struct { int hitstun, bounce_frames; bool can_act, wall_slam, knockdown, hard_knockdown, ground_bounced, brace, guard_broken, counter; void *hit; } hu;
    int cur_hurt; int hurt_tick; bool hurt_started; box_t hurt_prev;   /* HurtboxState em andamento */
    bool *limb_active;                /* def->nlimb itens */
};

/* eventos de apresentacao (som, particula, projetil, tremor): o render/audio consome depois do tick */
typedef enum { EV_SFX, EV_ENTER_SFX, EV_PARTICLE, EV_TIMED_PARTICLE, EV_PROJECTILE, EV_SHAKE } ev_type;
typedef struct { ev_type type; obj_t *obj; sid state; } ev_t;

typedef struct {
    state_t *states; int nstates;
    state_t *cur;
    sid stack[STATE_STACK]; int nstack;      /* pilha circular simplificada: descarta o mais antigo ao encher */
    sid queued; void *queued_data; bool has_queued;
    bool initialized;
} sm_t;

struct obj_s {
    phys_t chara;
    sm_t sm;
    int id;
    int uid;                                         /* identificador unico do objeto (substitui o nome nos hit_objects) */
    int hitlag_ticks, current_tick, game_tick, combo_count;
    bool gravity_enabled, can_update_sprite, disabled, initialized, is_ghost;
    bool state_interruptable, state_hit_cancellable;
    bool invulnerable, grounded_attack_immune, aerial_attack_immune, projectile_invulnerable, throw_invulnerable, roll_projectile_invulnerable;
    box_t hurtbox, collision_box, def_hurtbox;       /* hurtbox atual / padrao e caixa de colisao entre personagens */
    int sprite_anim, sprite_frame;                  /* o que o renderer desenha */
    bool is_fighter; struct fighter_s *fighter;      /* fighter dono (o proprio, para lutadores; criador, para projeteis) */
    void *user;                                      /* dados do tipo de objeto (personagem/projetil) */
    ev_t *events; int nevents, max_events;
};

/* maquina de estados */
void sm_queue_state(obj_t *o, sid next, void *data);
void sm_change_state(obj_t *o, sid next, void *data, bool enter, bool exit);
void sm_tick(obj_t *o);
sid  sm_state_by_name(const obj_t *o, const char *name);

/* ObjectState base */
sid  obj_state_enter_shared(obj_t *o, state_t *s);
sid  obj_state_tick_shared(obj_t *o, state_t *s);
void obj_state_exit_shared(obj_t *o, state_t *s);

/* BaseObj */
void obj_init(obj_t *o, state_t *states, int nstates, sid start);
void obj_tick(obj_t *o);
void obj_state_tick(obj_t *o);          /* BaseObj.state_tick */
void obj_change_state(obj_t *o, sid st, void *data);
void obj_emit(obj_t *o, ev_type t, sid st);
void obj_update_boxes(obj_t *o);
void state_setup_runtime(obj_t *o, state_t *s);   /* aloca hitboxes/limb do estado (chamar para cada estado antes de obj_init) */
obj_t *eng_opponent(obj_t *o);
void eng_hitbox_hit(hitbox_t *hb, obj_t *victim);
int  obj_active_hitboxes(obj_t *o, hitbox_t **out, int max);   /* ativos e habilitados, ordenados por priority (estavel) */
bool hurtbox_hit_check(obj_t *attacker_unused, hitbox_t *hb, obj_t *victim);  /* hitbox sobrepoe hurtbox ou hurtbox de membro */
static inline state_t *obj_state(obj_t *o) { return o->sm.cur; }
static inline int obj_facing(const obj_t *o) { return o->chara.facing; }

#endif
