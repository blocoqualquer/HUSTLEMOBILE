#include "hitbox.h"
#include "engine.h"

void hitbox_init(hitbox_t *h, const hitbox_def_t *d, obj_t *host)
{
    h->throw_state = d->throw_state;
    h->def = d; h->host = host;
    h->box.x = d->x; h->box.y = d->y; h->box.w = d->w < 0 ? -d->w : d->w; h->box.h = d->h < 0 ? -d->h : d->h; h->box.facing = 1;
    h->damage = d->damage; h->hitstun_ticks = d->hitstun_ticks; h->priority = d->priority;
    h->combo_hitstun_ticks = d->combo_hitstun_ticks == -1 ? d->hitstun_ticks : d->combo_hitstun_ticks;
    h->victim_hitlag = d->victim_hitlag; h->combo_victim_hitlag = d->combo_victim_hitlag;
    h->cancellable = d->cancellable; h->block_cancel_allowed = d->block_cancel_allowed;
    h->tick = 1; h->active = h->enabled = h->played_whiff_sound = false; h->nhit = 0;
}

void hitbox_activate(hitbox_t *h)
{
    if (h->active) return;
    box_update_position(&h->box, phys_x(&h->host->chara), phys_y(&h->host->chara));
    h->tick = 0; h->active = true; h->enabled = true;
    if (h->victim_hitlag == -1) h->victim_hitlag = h->def->hitlag_ticks;
    if (h->combo_victim_hitlag == -1) h->combo_victim_hitlag = h->victim_hitlag;
    if (h->def->is_throwbox) h->priority = 9999;
}

void hitbox_deactivate(hitbox_t *h)
{
    h->played_whiff_sound = false; h->active = false; h->enabled = false; h->nhit = 0;
}

void hitbox_tick(hitbox_t *h)
{
    const hitbox_def_t *d = h->def;
    box_update_position(&h->box, phys_x(&h->host->chara), phys_y(&h->host->chara));
    if (d->looping) {
        int loop_tick = h->tick % (d->loop_active_ticks + d->loop_inactive_ticks);
        bool prev = h->enabled;
        h->enabled = loop_tick < d->loop_active_ticks;
        if (!h->enabled && prev) {
            for (int i = 0; i < h->ngroup; i++) h->group[i]->nhit = 0;
            h->played_whiff_sound = false;
        }
    }
    h->tick++;
    if (h->tick > d->active_ticks && !d->always_on) hitbox_deactivate(h);
}

bool hitbox_already_hit(const hitbox_t *h, int uid)
{
    for (int g = 0; g < h->ngroup; g++)
        for (int i = 0; i < h->group[g]->nhit; i++) if (h->group[g]->hit_objects[i] == uid) return true;
    return false;
}

void hitbox_save_hit(hitbox_t *h, int uid)
{
    for (int g = 0; g < h->ngroup; g++) {
        hitbox_t *o = h->group[g];
        if (o->nhit < HB_MAX_HIT_OBJECTS) o->hit_objects[o->nhit++] = uid;
    }
}

void hitbox_reset_hit_objects(hitbox_t *h)
{
    h->nhit = 0;
    for (int g = 0; g < h->ngroup; g++) h->group[g]->nhit = 0;
    if (h->active && h->enabled) { hitbox_deactivate(h); hitbox_activate(h); }
}

bool hitbox_overlaps(const hitbox_t *h, const box_t *b) { return box_overlaps(&h->box, b); }
