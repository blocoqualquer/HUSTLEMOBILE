/* Camada de parry/bloqueio: ParryState.gd e NewParry.gd (GroundedParryState). */
#include "fighter.h"

static bool is_parry(const state_t *s) { return s->d.kind == KIND_PARRY_OLD || s->d.kind == KIND_PARRY_NEW; }

bool parry_matches_height(const state_t *s, const hitdata_t *h, int type)
{
    if (s->d.kind == KIND_PARRY_NEW && s->d.push) return true;
    if (type < 0) type = s->pr.parry_type;
    switch (h->hit_height) {
    case HEIGHT_HIGH: case HEIGHT_MID: return type == PH_HIGH || type == PH_BOTH;
    case HEIGHT_LOW: return type == PH_LOW || type == PH_BOTH;
    }
    return false;
}

bool parry_whiffed_block(const fighter_t *f)
{
    const state_t *cur = f->o.sm.cur;
    if (!cur || cur->d.kind != KIND_PARRY_NEW) return false;
    state_t *prev = obj_prev_state((obj_t *)&f->o);
    if (prev && prev->d.kind == KIND_PARRY_NEW)
        return !prev->pr.parried_last && !prev->d.autoguard && f->combo_count <= 0;
    return false;
}

bool parry_can_parry_hitbox(const fighter_t *f, const state_t *s, const hitdata_t *h)
{
    if (!is_parry(s)) return false;
    if (s->d.kind == KIND_PARRY_NEW) {
        if (s->pr.punishable) return false;
        if (!s->pr.perfect) return true;
        if (!h) return false;
        if (!s->active) return false;
        if (!s->pr.parry_active) return false;
        if (s->pr.started_in_combo && f->combo_count <= 0) return false;
        return true;
    }
    if (!s->pr.perfect) return true;
    if (!h) return false;
    if (!s->active || !s->pr.parry_active) return false;
    return parry_matches_height(s, h, -1);
}

void parry_parry(fighter_t *f, state_t *s, bool perfect)
{
    if (!is_parry(s)) return;
    perfect = perfect && s->d.can_parry;
    if (perfect) {
        if (s->d.kind == KIND_PARRY_NEW) {
            s->d.disable_aerial_movement = 0;
            cs_enable_interrupt(f, s, true, false);
            fighter_set_block_stun(f, 0, -1);
            f->blocked_hitbox_plus_frames = 0;
        } else cs_enable_interrupt(f, s, true, false);
    } else {
        s->pr.parry_type = PH_BOTH;
        f->o.throw_invulnerable = true;
    }
    f->parried = true;
    s->pr.parried = true;
    if (s->d.kind == KIND_PARRY_NEW) s->pr.parried_last = true;
    s->pr.perfect = perfect;
}
