#include "projectile.h"
#include <stdlib.h>
#include <string.h>

static fx fxi(int v) { return FX_INT(v); }

proj_t *proj_spawn(game_t *g, const chardata_t *cd, obj_t *creator, int px, int py, bool relative, const void *data, size_t dsize, bool local)
{
    if (g->nobjs >= GAME_MAX_OBJS) return NULL;
    proj_t *p = calloc(1, sizeof *p);
    p->cd = cd; p->pj = cd->pj; p->creator = creator;
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
    p->can_stack = true; p->attached_to = -1;
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
    bool was = o->disabled;
    o->disabled = true;
    obj_t *ob = o;
    for (int i = 0; i < ob->sm.nstates; i++) {
        state_t *s = &ob->sm.states[i];
        for (int k = 0; k < s->d.nhb; k++) hitbox_deactivate(&s->hbr[k]);
    }
    const proj_vt_t *vt = PROJ(o)->pj.vt;
    if (!was && vt && vt->on_disable) vt->on_disable(o);
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
    const proj_vt_t *vt = p->pj.vt;
    fx mod = p->pj.hitlag_modifier;
    victim->hitlag_ticks = (int)fx_round(fx_mul(mod, fxi(hb->victim_hitlag)));
    obj_t *host = hb->host;
    if (host && host->is_fighter) {
        int hh = (int)fx_round(fx_mul(mod, fxi(d->hitlag_ticks)));
        if (p->pj.apply_hitlag_when_hit_by_melee && host->hitlag_ticks < hh) host->hitlag_ticks = hh;
        if (p->pj.free_cancel_on_hit) fighter_projectile_free_cancel(host->fighter);
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

proj_t *proj_spawn_from(obj_t *host, const chardata_t *cd, int x, int y, bool relative, const void *data, size_t dsize)
{
    return proj_spawn(eng_game_get(), cd, host, x, y, relative, data, dsize, true);
}

void obj_hurt_center(const obj_t *o, int *x, int *y)
{
    box_t b = o->hurtbox;
    box_update_position(&b, phys_x(&o->chara), phys_y(&o->chara));
    b.facing = o->chara.facing;
    *x = box_center_x(&b); *y = box_center_y(&b);
}

bool obj_hurtboxes_overlap(const obj_t *a, const obj_t *b)
{
    box_t ba = a->hurtbox, bb = b->hurtbox;
    box_update_position(&ba, phys_x(&a->chara), phys_y(&a->chara)); ba.facing = a->chara.facing;
    box_update_position(&bb, phys_x(&b->chara), phys_y(&b->chara)); bb.facing = b->chara.facing;
    return box_overlaps(&ba, &bb);
}

/* ---- ganchos fortes (substituem os fracos de combat.c) ---- */
bool eng_proj_always_parriable(obj_t *p) { return PROJ(p)->pj.always_parriable; }
bool eng_proj_has_parry_window(obj_t *p) { return PROJ(p)->pj.has_projectile_parry_window; }
void eng_host_got_parried(obj_t *host, fighter_t *by)
{
    (void)by;
    if (!obj_is_proj(host)) return;
    proj_got_parried(host);                                   /* _got_parried do estado (sinal da hitbox) */
    state_t *s = host->sm.cur;
    host->hitlag_ticks += s->d.extra_parry_hitlag;
    if (s->d.vt && s->d.vt->got_perfect_parried) s->d.vt->got_perfect_parried(host, s);
    const proj_vt_t *vt = PROJ(host)->pj.vt;
    if (vt && vt->on_got_parried) vt->on_got_parried(host);
}
void eng_host_got_blocked(obj_t *host, fighter_t *by)
{
    (void)by;
    if (!obj_is_proj(host)) return;
    state_t *s = host->sm.cur;
    s->was_blocked = true; s->number_of_hits_blocked++;
    if (s->d.vt && s->d.vt->got_blocked) s->d.vt->got_blocked(host, s);
}

int fighter_name_id_cd(const chardata_t *cd, const char *name)
{
    for (int i = 0; i < cd->nnames; i++) if (cd->names[i] && strcmp(cd->names[i], name) == 0) return i;
    return -1;
}

/* ObjectState.spawn_exported_projectile */
void eng_spawn_exported_projectile(obj_t *o, state_t *s)
{
    const state_def_t *d = &s->def[0];
    (void)d;
    const state_def_t *sd = s->def;
    int x = sd->projectile_pos_x, y = sd->projectile_pos_y;
    if (sd->vt && sd->vt->projectile_pos) sd->vt->projectile_pos(o, s, &x, &y);
    unsigned char buf[32]; size_t n = 0;
    if (sd->vt && sd->vt->projectile_data) n = sd->vt->projectile_data(o, s, buf);
    proj_t *p = proj_spawn(eng_game_get(), (const chardata_t *)sd->projectile_scene, o, x, y, true, n ? buf : NULL, n, sd->projectile_local_pos);
    if (!p) return;
    if (sd->projectile_match_facing) p->o.chara.facing = o->chara.facing;
    if (sd->vt && sd->vt->process_projectile) sd->vt->process_projectile(o, s, &p->o);
}
