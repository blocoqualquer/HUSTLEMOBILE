#include "projectile.h"
#include <stdlib.h>
#include <string.h>

static fx fxi(int v) { return FX_INT(v); }

proj_t *proj_spawn(game_t *g, const chardata_t *cd, obj_t *creator, int px, int py, bool relative, const void *data, size_t dsize, bool local)
{
    if (g->nobjs >= GAME_MAX_OBJS) return NULL;
    proj_t *p = calloc(1, sizeof *p);
    p->cd = cd; p->creator = creator;
    if (data && dsize <= sizeof p->sdata) { memcpy(&p->sdata, data, dsize); p->spawn_data = &p->sdata; }
    p->owner = creator->is_fighter ? creator->fighter : creator->fighter;
    p->states = calloc((size_t)cd->nstates, sizeof(state_t));
    for (int i = 0; i < cd->nstates; i++) p->states[i].def = &cd->states[i];
    obj_t *o = &p->o;
    phys_init(&o->chara);
    o->chara.grav = cd->gravity; o->chara.gfric = cd->ground_friction; o->chara.afric = cd->air_friction;
    o->chara.max_gspeed = cd->max_ground_speed; o->chara.max_aspeed = cd->max_air_speed; o->chara.max_fall = cd->max_fall_speed;
    o->hurtbox = cd->hurtbox; o->collision_box = cd->collision_box;
    o->events = p->evbuf; o->max_events = PROJ_MAX_EVENTS;
    o->gravity_enabled = true; o->can_update_sprite = true;
    o->user = p; o->is_fighter = false; o->fighter = p->owner;
    o->id = creator->id; o->uid = g->next_uid++;
    int facing = creator->chara.facing;
    int cx = phys_x(&creator->chara), cy = phys_y(&creator->chara);
    if (local) phys_set_pos(&o->chara, fxi(cx + px * (relative ? facing : 1)), fxi(cy + py));
    else phys_set_pos(&o->chara, fxi(px), fxi(py));
    o->chara.facing = facing;
    o->chara.grounded = o->chara.py >= 0;
    p->can_stack = true;
    g->objs[g->nobjs++] = o;
    return p;
}

void proj_terminate_hitboxes(obj_t *o)
{
    state_t *s = o->sm.cur;
    if (!s) return;
    for (int i = 0; i < s->d.nhb; i++) hitbox_deactivate(&s->hbr[i]);
}

void proj_disable(obj_t *o)
{
    o->disabled = true;
    obj_t *ob = o;
    for (int i = 0; i < ob->sm.nstates; i++) {
        state_t *s = &ob->sm.states[i];
        for (int k = 0; k < s->d.nhb; k++) hitbox_deactivate(&s->hbr[k]);
    }
}

void proj_got_parried(obj_t *o)
{
    proj_t *p = PROJ(o);
    p->got_parried = true;
    state_t *s = o->sm.cur;
    if (s && s->d.vt && s->d.vt->got_parried) s->d.vt->got_parried(o, s);
}

/* BaseProjectile.hit_by (a parte de simulacao) */
void proj_hit_by(obj_t *victim, hitbox_t *hb)
{
    proj_t *p = PROJ(victim);
    const hitbox_def_t *d = hb->def;
    if (d->is_throwbox) return;
    const proj_vt_t *vt = p->cd->pj.vt;
    fx mod = p->cd->pj.hitlag_modifier;
    victim->hitlag_ticks = (int)fx_round(fx_mul(mod, fxi(hb->victim_hitlag)));
    obj_t *host = hb->host;
    if (host && host->is_fighter) {
        int hh = (int)fx_round(fx_mul(mod, fxi(d->hitlag_ticks)));
        if (p->cd->pj.apply_hitlag_when_hit_by_melee && host->hitlag_ticks < hh) host->hitlag_ticks = hh;
        if (p->cd->pj.free_cancel_on_hit) fighter_projectile_free_cancel(host->fighter);
    }
    if (vt && vt->hit_by) vt->hit_by(victim, hb);
}

/* ---- integracao com o laco do jogo ---- */

void game_init_objects(game_t *g)
{
    for (int i = 0; i < g->nobjs; i++) {
        obj_t *o = g->objs[i];
        if (o->initialized || o->disabled) continue;
        proj_t *p = PROJ(o);
        obj_init_data(o, p->states, p->cd->nstates, 0, p->spawn_data);
        obj_update_boxes(o);
    }
}

void game_reclaim_husks(game_t *g)
{
    int w = 0;
    for (int i = 0; i < g->nobjs; i++) {
        obj_t *o = g->objs[i];
        if (o->disabled && ++PROJ(o)->disabled_linger >= PROJ_HUSK_LINGER) {
            proj_t *p = PROJ(o);
            for (int s = 0; s < p->cd->nstates; s++) {
                state_t *st = &p->states[s];
                for (int k = 0; k < st->d.nhb; k++) free(st->hbr[k].group);
                free(st->hbr); free(st->limb_active);
            }
            free(p->states); free(p);
            continue;
        }
        g->objs[w++] = o;
    }
    g->nobjs = w;
}

void game_free_objects(game_t *g)
{
    for (int i = 0; i < g->nobjs; i++) PROJ(g->objs[i])->disabled_linger = PROJ_HUSK_LINGER;
    for (int i = 0; i < g->nobjs; i++) g->objs[i]->disabled = true;
    game_reclaim_husks(g);
}
