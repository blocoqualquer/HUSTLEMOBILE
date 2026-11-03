/* Parte de hitbox/hurtbox de ObjectState (process_hitboxes, process_hurtboxes, setup_hitboxes) e utilitarios de objeto. */
#include "engine.h"
#include <stdlib.h>
#include <string.h>

__attribute__((weak)) obj_t *eng_opponent(obj_t *o) { (void)o; return NULL; }
__attribute__((weak)) void eng_hitbox_hit(hitbox_t *hb, obj_t *victim) { (void)hb; (void)victim; }

void obj_update_boxes(obj_t *o)
{
    int x = phys_x(&o->chara), y = phys_y(&o->chara), f = o->chara.facing;
    o->hurtbox.facing = f; o->collision_box.facing = f;
    box_update_position(&o->collision_box, x, y); box_update_position(&o->hurtbox, x, y);
}

/* equivale a ObjectState.setup_hitboxes + setup_hurtboxes: aloca runtime e liga grupos */
void state_setup_runtime(obj_t *o, state_t *s)
{
    const state_def_t *d = s->def;
    s->hbr = d->nhb ? calloc((size_t)d->nhb, sizeof(hitbox_t)) : NULL;
    s->limb_active = d->nlimb ? calloc((size_t)d->nlimb, sizeof(bool)) : NULL;
    for (int i = 0; i < d->nhb; i++) hitbox_init(&s->hbr[i], &d->hb[i], o);
    for (int i = 0; i < d->nhb; i++) {
        int n = 0;
        for (int j = 0; j < d->nhb; j++) if (d->hb[j].group == d->hb[i].group) n++;
        s->hbr[i].group = malloc(sizeof(hitbox_t *) * (size_t)n); s->hbr[i].ngroup = 0;
        for (int j = 0; j < d->nhb; j++) if (d->hb[j].group == d->hb[i].group) s->hbr[i].group[s->hbr[i].ngroup++] = &s->hbr[j];
    }
}

int obj_active_hitboxes(obj_t *o, hitbox_t **out, int max)
{
    int n = 0; state_t *s = o->sm.cur;
    if (!s) return 0;
    for (int i = 0; i < s->def->nhb && n < max; i++) {
        hitbox_t *h = &s->hbr[i];
        if (!h->active || !h->enabled) continue;
        int k = n++;                                              /* insercao estavel por priority crescente */
        while (k > 0 && out[k - 1]->priority > h->priority) { out[k] = out[k - 1]; k--; }
        out[k] = h;
    }
    return n;
}

void eng_process_hitboxes(obj_t *o, state_t *s)
{
    const state_def_t *d = s->def;
    int x = phys_x(&o->chara), y = phys_y(&o->chara);
    for (int i = 0; i < d->nhb; i++) {
        hitbox_t *h = &s->hbr[i];
        if (h->def->start_tick > 0 && h->def->start_tick == s->current_tick + 1 && h->def->activated) {
            box_update_position(&h->box, x, y);
            h->box.facing = o->chara.facing;
            hitbox_activate(h);
            if (h->def->hitbox_type == HB_THROWHIT) {
                obj_t *opp = eng_opponent(o);
                if (opp) eng_hitbox_hit(h, opp);
                hitbox_deactivate(h);
            }
        }
    }
    for (int i = 0; i < d->nhb; i++) {
        hitbox_t *h = &s->hbr[i];
        if (!h->active) continue;
        box_update_position(&h->box, x, y);
        h->box.facing = o->chara.facing;
        hitbox_tick(h);
    }
}

static bool limb_active_now(const state_t *s, const hurtstate_def_t *l)
{
    bool after = l->start_tick <= 0 || s->current_tick + 1 > l->start_tick;
    bool before = l->endless || s->current_tick <= l->start_tick + l->active_ticks;
    return after && before;
}

static void hurt_end(obj_t *o, state_t *s)
{
    if (!s->hurt_started) return;
    s->hurt_started = false;
    o->hurtbox.x = s->hurt_prev.x; o->hurtbox.y = s->hurt_prev.y; o->hurtbox.w = s->hurt_prev.w; o->hurtbox.h = s->hurt_prev.h;
}

void eng_process_hurtboxes(obj_t *o, state_t *s)
{
    const state_def_t *d = s->def;
    if (s->cur_hurt >= 0 && s->cur_hurt < d->nhurt) {              /* HurtboxState.tick */
        const hurtstate_def_t *c = &d->hurt[s->cur_hurt];
        s->hurt_tick++;
        if (s->hurt_tick > c->active_ticks && !c->endless) hurt_end(o, s);
    }
    int pick = -1;                                                  /* o ultimo com este start_tick vence (dict) */
    for (int i = 0; i < d->nhurt; i++) if (d->hurt[i].start_tick == s->current_tick) pick = i;
    if (pick >= 0) {
        if (s->cur_hurt >= 0) hurt_end(o, s);
        s->cur_hurt = pick;
        s->hurt_prev = o->hurtbox; s->hurt_started = true; s->hurt_tick = 0;
        o->hurtbox.x = d->hurt[pick].x; o->hurtbox.y = d->hurt[pick].y; o->hurtbox.w = d->hurt[pick].w; o->hurtbox.h = d->hurt[pick].h;
    }
    for (int i = 0; i < d->nlimb; i++) s->limb_active[i] = limb_active_now(s, &d->limb[i]);
    o->hurtbox.facing = o->chara.facing;
}

/* hitbox atinge a hurtbox principal ou alguma hurtbox de membro ativa do estado atual da vitima */
bool hurtbox_hit_check(obj_t *unused, hitbox_t *hb, obj_t *victim)
{
    (void)unused;
    if (box_overlaps(&hb->box, &victim->hurtbox)) return true;
    state_t *s = victim->sm.cur;
    if (!s) return false;
    for (int i = 0; i < s->def->nlimb; i++) {
        if (!limb_active_now(s, &s->def->limb[i])) continue;
        box_t b = { s->def->limb[i].x, s->def->limb[i].y, s->def->limb[i].w, s->def->limb[i].h,
                    phys_x(&victim->chara), phys_y(&victim->chara), victim->chara.facing };
        if (box_overlaps(&hb->box, &b)) return true;
    }
    return false;
}
