/* Laco de jogo de 2 lutadores (game.gd): ordem de tick, colisoes, aplicacao de hitboxes. */
#ifndef GAME_H
#define GAME_H
#include "fighter.h"

#define GAME_MAX_OBJS 48
typedef struct {
    fighter_t *p[2];
    int current_tick, time, stage_width, max_char_distance, ceiling_height;
    bool has_ceiling, game_finished;
    int next_uid;
    obj_t *objs[GAME_MAX_OBJS]; int nobjs;           /* projeteis e outros objetos (alem dos lutadores) */
} game_t;

void game_init(game_t *g, fighter_t *p1, fighter_t *p2);
void game_tick(game_t *g);
bool game_should_end(const game_t *g);
game_t *eng_game_get(void);
obj_t *game_obj_by_uid(game_t *g, int uid);
#endif
