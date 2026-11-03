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
    uint8_t is_grab, has_hitboxes, is_guard_break;                      /* derivados dos hitboxes (preenchidos em obj_init) */
    int earliest_hitbox;
} state_def_t;

struct state_s {
    const state_def_t *def;
    sid id;
    bool active;
    void *data;                       /* dados passados na troca de estado (ver queue_state) */
    int current_tick, current_real_tick, start_tick, last_facing, exit_tick;
    bool same_as_last_state, was_blocked; int number_of_hits_blocked;
    int max_tick, max_tick_shared;
    int ustate[8];                    /* variaveis privadas do script do estado (membros 'var' do .gd) */
    hitbox_t *hbr;                    /* hitboxes em runtime (def->nhb itens) */
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
