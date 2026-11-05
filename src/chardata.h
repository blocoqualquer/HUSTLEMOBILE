/* Dados constantes de um personagem gerados por tools/gen_c_data.py a partir das cenas .tscn. */
#ifndef CHARDATA_H
#define CHARDATA_H
#include "engine.h"

typedef struct { const char *name; int nframes; const uint32_t *frames; uint8_t loop; } anim_def_t;   /* frames = hash FNV-1a do caminho do PNG (indice do .spi) */

/* propriedades de BaseProjectile (is_proj = 0 para personagens) */
struct proj_vt_s;
typedef struct {
    uint8_t is_proj, immunity_susceptible, roll_immunity_susceptible, deletes_other_projectiles, fizzle_on_ceiling, movable, can_be_hit_by_melee,
            hit_by_self_projectiles, hit_cancel_on_hit, free_cancel_on_hit, apply_hitlag_when_hit_by_melee, projectile_immune, hookable,
            has_projectile_parry_window, always_parriable, damages_own_team;
    fx hitlag_modifier;
    const struct proj_vt_s *vt;       /* script raiz do projetil (porte manual); NULL = so BaseProjectile */
} projdef_t;

typedef struct chardata_s {
    const char *name;
    int nstates; const state_def_t *states;
    int nanims; const anim_def_t *anims;
    int nnames; const char *const *names;       /* nomes internados (estados e categorias) */
    fx gravity, ground_friction, air_friction, max_ground_speed, max_air_speed, max_fall_speed;
    box_t hurtbox, collision_box;
    projdef_t pj;
} chardata_t;

#endif
