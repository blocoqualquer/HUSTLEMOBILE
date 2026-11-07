/* Funcoes dos scripts-base (Idle, Landing, Taunt, ThrowState) reutilizadas por scripts que os estendem. */
#ifndef STATES_H
#define STATES_H
#include "fighter.h"
sid idle_enter(obj_t *, state_t *);   sid idle_tick(obj_t *, state_t *);
sid land_frame(obj_t *, state_t *, int);  sid land_tick(obj_t *, state_t *);
sid taunt_frame(obj_t *, state_t *, int);
sid fall_tick(obj_t *, state_t *);
sid jump_enter(obj_t *, state_t *); sid jump_frame(obj_t *, state_t *, int); sid jump_tick(obj_t *, state_t *);
sid grab_enter(obj_t *, state_t *); sid grab_frame(obj_t *, state_t *, int); sid grab_tick(obj_t *, state_t *);
sid th_enter(obj_t *, state_t *);  sid th_frame_shared(obj_t *, state_t *, int);  sid th_tick_shared(obj_t *, state_t *);
sid th_tick_after(obj_t *, state_t *);  void th_exit(obj_t *, state_t *);
void cs_exit_shared(obj_t *, state_t *);
void th_release_now(fighter_t *f, state_t *s);
sid roll_enter(obj_t *, state_t *); sid roll_frame(obj_t *, state_t *, int); sid roll_tick(obj_t *, state_t *);
#define VT_ROLL .enter = roll_enter, .frame = roll_frame, .tick = roll_tick
#define VT_IDLE .enter = idle_enter, .tick = idle_tick
#define VT_LANDING .frame = land_frame, .tick = land_tick
#define VT_THROWSTATE .enter = th_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit
/* DefaultFireball (Default.gd) reutilizado pelos projeteis que o estendem */
#include "projectile.h"
sid proj_enter(obj_t *, state_t *);  sid proj_frame0(obj_t *, state_t *, int);
void proj_on_hit(obj_t *, state_t *, obj_t *, hitbox_t *);
void default_move(obj_t *, state_t *);
sid default_tick_common(obj_t *, state_t *, void (*move)(obj_t *, state_t *));
void proj_fizzle(obj_t *, state_t *);  void proj_kill_hurtbox(obj_t *);
enum { PU_HIT = 0, PU_HIT_TICK = 1, PU_NUM_HITS = 2 };
#endif
