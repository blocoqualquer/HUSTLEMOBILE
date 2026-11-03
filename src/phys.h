/* Fisica de objeto do YOMI HUSTLE (substitui o FGObject nativo da lib tbfg).
 * Reconstruida por caixa-preta: modelo em tools/physmodel.py, validado contra a lib original
 * com fuzzer diferencial (tools/physfuzz.py); ver docs/PHYSICS.md. Tudo em Q48.16 (fx.h).
 * Erro residual conhecido: apply_fric pode diferir ate ~3 LSB (4,6e-5) da lib original. */
#ifndef PHYS_H
#define PHYS_H
#include "fx.h"
#include <stdbool.h>

#define PHYS_MIN_VEL   ((fx)7)        /* parse("0.0001") */
#define PHYS_PB_CAP    FX_INT(10)
#define PHYS_PB_DECAY  ((fx)36045)    /* parse("0.55") */

typedef struct {
    fx px, py, vx, vy, frx, fry;        /* posicao, velocidade, forcas acumuladas */
    fx grav, gmod;                      /* gravidade e modificador (padrao FX_ONE) */
    fx gfric, afric, max_gspeed, max_aspeed, max_fall;
    fx pushback;
    int facing;                         /* +1 / -1 */
    bool grounded, snap;
} phys_t;

static inline void phys_init(phys_t *o)
{
    o->px = o->py = o->vx = o->vy = o->frx = o->fry = 0;
    o->grav = 0; o->gmod = FX_ONE;
    o->gfric = o->afric = o->max_gspeed = o->max_aspeed = o->max_fall = 0;
    o->pushback = 0; o->facing = 1; o->grounded = true; o->snap = true;
}

static inline int phys_x(const phys_t *o) { return (int)fx_round(o->px); }
static inline int phys_y(const phys_t *o) { return (int)fx_round(o->py); }
static inline void phys_set_pos(phys_t *o, fx x, fx y) { o->px = x; o->py = y; }
static inline void phys_update_grounded(phys_t *o) { o->grounded = o->py >= 0; }
static inline void phys_reset_momentum(phys_t *o) { o->vx = o->vy = o->frx = o->fry = 0; }

/* forca: no chao, a componente y positiva (para baixo) e descartada no momento da chamada */
static inline void phys_force(phys_t *o, fx x, fx y)
{
    if (o->grounded && y > 0) y = 0;
    o->frx += x; o->fry += y;
}
static inline void phys_force_rel(phys_t *o, fx x, fx y) { phys_force(o, x * o->facing, y); }

static inline void phys_grav(phys_t *o)
{
    if (!o->grounded && o->vy < o->max_fall) o->fry += fx_mul(o->grav, o->gmod);
}
static inline void phys_grav_custom(phys_t *o, fx g, fx fall)
{
    if (!o->grounded && o->vy < fall) o->fry += fx_mul(g, o->gmod);
}

static inline void phys_take_forces(phys_t *o)
{
    o->vx += o->frx; o->vy += o->fry; o->frx = o->fry = 0;
}

/* aplica forcas com limite de velocidade e pouso */
static inline void phys_apply_forces(phys_t *o)
{
    fx lim = o->grounded ? o->max_gspeed : o->max_aspeed;   /* flag "velha" de proposito */
    phys_take_forces(o);
    if (o->vx > lim) o->vx = lim;
    if (o->vx < -lim) o->vx = -lim;
    if (!o->grounded && o->vy > o->max_fall) o->vy = o->max_fall;
    o->px += o->vx; o->py += o->vy;
    if (o->py >= 0) {
        o->grounded = true;
        if (o->vy > 0) o->vy = 0;
        if (o->snap) o->py = 0;
    } else o->grounded = false;
}

static inline void phys_apply_forces_no_limit(phys_t *o)
{
    phys_take_forces(o);
    o->grounded = o->py >= 0;
    if (o->snap) {
        if (o->grounded && o->vy > 0) o->vy = 0;
        o->px += o->vx; o->py += o->vy;
        if (o->py > 0) o->py = 0;
    } else {
        o->px += o->vx; o->py += o->vy;
        if (o->grounded && o->vy > 0) o->vy = 0;
    }
}

/* friccao por eixo: so age se |v| > f (confirmado na lib), v *= (1 - f) */
static inline void phys_x_fric(phys_t *o, fx f) { if (fx_abs(o->vx) > f) o->vx = fx_mul(o->vx, FX_ONE - f); }
static inline void phys_y_fric(phys_t *o, fx f) { if (fx_abs(o->vy) > f) o->vy = fx_mul(o->vy, FX_ONE - f); }

/* friccao do modo atual: vx -= trunc(vx * fric / vel_max) */
static inline void phys_fric(phys_t *o)
{
    fx fr = o->grounded ? o->gfric : o->afric, m = o->grounded ? o->max_gspeed : o->max_aspeed;
    if (fx_abs(o->vx) > PHYS_MIN_VEL && m != 0) {
        fx q = (fx_abs(o->vx) * fr) / m;
        o->vx += o->vx > 0 ? -q : q;
    }
}

static inline void phys_move(phys_t *o, fx x, fx y)
{
    o->px += x; o->py += y;
    if (o->snap && o->py > 0) o->py = 0;
}
static inline void phys_move_rel(phys_t *o, fx x, fx y) { phys_move(o, x * o->facing, y); }

static inline void phys_add_pushback(phys_t *o, fx v) { if (o->pushback < PHYS_PB_CAP) o->pushback += v; }
static inline void phys_reset_pushback(phys_t *o) { o->pushback = 0; }
static inline void phys_apply_pushback(phys_t *o, int dir)
{
    o->px += o->pushback * dir;
    o->pushback -= PHYS_PB_DECAY;
    if (o->pushback < 0) o->pushback = 0;
}

#endif
