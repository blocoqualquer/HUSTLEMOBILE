/* Projeteis e objetos extras (BaseProjectile.gd + spawn_object de BaseObj.gd). */
#ifndef PROJECTILE_H
#define PROJECTILE_H
#include "game.h"
#include <stddef.h>

#define PROJ_HUSK_LINGER 300          /* DISABLED_HUSK_LINGER_TICKS */
#define PROJ_MAX_EVENTS 8

typedef struct proj_s {
    obj_t o;                          /* primeiro membro: obj_t* == proj_t* */
    const chardata_t *cd;
    fighter_t *owner;                 /* lutador dono (get_fighter) */
    obj_t *creator;                   /* quem criou (lutador ou outro projetil) */
    bool got_parried, stopped;
    int disabled_linger;
    void *spawn_data;
    union { struct { bool has; fx sm; } speed; struct { fxv dir; } dir; unsigned char raw[32]; } sdata;   /* copia dos dados de spawn */
    state_t *states;
    ev_t evbuf[PROJ_MAX_EVENTS];
    int ix, iy, idata;                /* variaveis livres para scripts (ex.: dir_x/dir_y fx) */
    fx dir_x, dir_y;                  /* NinjaShuriken.dir_x/dir_y */
    bool can_stack;
} proj_t;

/* script raiz do projetil (ex.: Kunai.gd): hooks sobre BaseProjectile */
typedef struct proj_vt_s {
    void (*hit_by)(obj_t *self, hitbox_t *hb);
    void (*on_hit_ceiling)(obj_t *self);
    void (*init)(obj_t *self);
    void (*tick)(obj_t *self);        /* depois de obj_tick, antes do clamp de palco */
} proj_vt_t;

#define PROJ(o) ((proj_t *)(o))
static inline bool obj_is_proj(const obj_t *o) { return o && !o->is_fighter && o->user; }

/* BaseObj.spawn_object: cria e registra no jogo; a init roda no mesmo tick (initialize_objects) */
proj_t *proj_spawn(game_t *g, const chardata_t *cd, obj_t *creator, int px, int py, bool relative, const void *data, size_t dsize, bool local);
void proj_disable(obj_t *o);                     /* BaseProjectile.disable */
void proj_terminate_hitboxes(obj_t *o);          /* ObjectState.terminate_hitboxes (estado atual) */
void proj_hit_by(obj_t *victim, hitbox_t *hb);   /* BaseProjectile.hit_by */
void proj_got_parried(obj_t *o);
void game_init_objects(game_t *g);               /* initialize_objects */
void game_reclaim_husks(game_t *g);              /* reclaim_disabled_husks */
void game_free_objects(game_t *g);

#endif
