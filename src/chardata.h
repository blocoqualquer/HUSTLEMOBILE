/* Dados constantes de um personagem gerados por tools/gen_c_data.py a partir das cenas .tscn. */
#ifndef CHARDATA_H
#define CHARDATA_H
#include "engine.h"

typedef struct { const char *name; int nframes; const uint32_t *frames; uint8_t loop; } anim_def_t;   /* frames = hash FNV-1a do caminho do PNG (indice do .spi) */

typedef struct {
    const char *name;
    int nstates; const state_def_t *states;
    int nanims; const anim_def_t *anims;
    fx gravity, ground_friction, air_friction, max_ground_speed, max_air_speed, max_fall_speed;
    box_t hurtbox, collision_box;
} chardata_t;

#endif
