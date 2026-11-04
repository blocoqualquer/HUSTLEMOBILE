#include "engine.h"
#include <string.h>

/* pontos de extensao implementados pelo modulo de hitbox/hurtbox (versao fraca: nada a fazer) */
__attribute__((weak)) void eng_process_hitboxes(obj_t *o, state_t *s) { (void)o; (void)s; }
__attribute__((weak)) void eng_process_hurtboxes(obj_t *o, state_t *s) { (void)o; (void)s; }
__attribute__((weak)) void eng_deactivate_hitboxes(obj_t *o) { (void)o; }
__attribute__((weak)) void eng_host_commands(obj_t *o, state_t *s) { (void)o; (void)s; }

void obj_emit(obj_t *o, ev_type t, sid st)
{
    if (o->is_ghost || !o->events || o->nevents >= o->max_events) return;
    ev_t *e = &o->events[o->nevents++]; e->type = t; e->obj = o; e->state = st;
}

sid sm_state_by_name(const obj_t *o, const char *name)
{
    for (int i = 0; i < o->sm.nstates; i++) if (!strcmp(o->sm.states[i].def->name, name)) return i;
    return SID_NONE;
}

/* camada CharacterState (states_common.c); ausente nos testes do motor puro */
extern sid  cs_enter_shared(obj_t *, state_t *) __attribute__((weak));
extern sid  cs_tick_shared(obj_t *, state_t *) __attribute__((weak));
extern sid  cs_tick_after(obj_t *, state_t *) __attribute__((weak));
extern void cs_exit_shared(obj_t *, state_t *) __attribute__((weak));
extern sid  cs_frame_shared(obj_t *, state_t *, int) __attribute__((weak));

state_t *obj_prev_state(obj_t *o) { return o->sm.nstack > 1 ? &o->sm.states[o->sm.stack[o->sm.nstack - 2]] : NULL; }

/* StateInterface.queue_state_change / StateMachine.queue_state: so enfileira se o estado de origem ainda esta ativo */
void sm_queue_state(obj_t *o, sid next, void *data)
{
    sm_t *m = &o->sm;
    if (m->cur && m->cur->active) { m->queued = next; m->queued_data = data; m->has_queued = true; }
}

void sm_change_state(obj_t *o, sid next, void *data, bool enter, bool exit)
{
    sm_t *m = &o->sm;
    if (next < 0 || next >= m->nstates) return;
    m->has_queued = false; m->queued_data = NULL;
    if (m->cur) {
        state_t *c = m->cur;
        if (exit) {
            if (c->d.vt && c->d.vt->exit_shared) c->d.vt->exit_shared(o, c);
            else if (c->d.cs && cs_exit_shared) cs_exit_shared(o, c);
            else obj_state_exit_shared(o, c);
            if (c->d.vt && c->d.vt->exit) c->d.vt->exit(o, c);
            eng_deactivate_hitboxes(o);              /* BaseObj._on_state_exited */
        }
        c->active = false;
    }
    state_t *n = &m->states[next];
    m->cur = n;
    if (m->nstack == STATE_STACK) { memmove(m->stack, m->stack + 1, sizeof(sid) * (STATE_STACK - 1)); m->nstack--; }
    m->stack[m->nstack++] = next;
    n->active = true;
    n->data = data;
    if (enter) {
        const state_vt_t *vt = n->d.vt;
        sid r = (vt && vt->enter_shared) ? vt->enter_shared(o, n) : (n->d.cs && cs_enter_shared) ? cs_enter_shared(o, n) : obj_state_enter_shared(o, n);
        if (r != SID_NONE) { sm_change_state(o, r, NULL, true, true); return; }
        if (vt && vt->enter) { r = vt->enter(o, n); if (r != SID_NONE) { sm_change_state(o, r, NULL, true, true); return; } }
    }
}

void sm_tick(obj_t *o)
{
    sm_t *m = &o->sm;
    if (m->has_queued) sm_change_state(o, m->queued, m->queued_data, true, true);
    state_t *s = m->cur; const state_vt_t *vt = s->d.vt;
    if (vt && vt->tick_before) vt->tick_before(o, s);
    sid r = (vt && vt->tick_shared) ? vt->tick_shared(o, s) : (s->d.cs && cs_tick_shared) ? cs_tick_shared(o, s) : obj_state_tick_shared(o, s);
    if (r == SID_NONE && vt && vt->tick) r = vt->tick(o, s);
    if (r == SID_NONE && vt && vt->tick_after) r = vt->tick_after(o, s);
    else if (r == SID_NONE && s->d.cs && cs_tick_after && !(vt && vt->tick_after)) r = cs_tick_after(o, s);
    if (r != SID_NONE) sm_queue_state(o, r, NULL);
}

/* ---------- ObjectState base ---------- */
sid obj_state_enter_shared(obj_t *o, state_t *s)
{
    const state_def_t *d = &s->d;
    s->same_as_last_state = false;
    state_t *prev = obj_prev_state(o);
    if (prev) {
        if (d->force_same_direction_as_previous_state) o->chara.facing = prev->last_facing;
        if (prev != s) s->exit_tick = -1; else s->same_as_last_state = true;
    }
    if (d->reset_momentum) phys_reset_momentum(&o->chara);
    if (d->reset_x_momentum) o->chara.vx = 0;
    if (d->reset_y_momentum) o->chara.vy = 0;
    s->was_blocked = false; s->number_of_hits_blocked = 0;
    s->current_tick = -1; s->current_real_tick = -1; s->start_tick = o->current_tick;
    obj_emit(o, EV_ENTER_SFX, s->id);
    o->state_interruptable = false; o->state_hit_cancellable = false;   /* BaseObj.on_state_started */
    return SID_NONE;
}

void obj_state_exit_shared(obj_t *o, state_t *s)
{
    s->exit_tick = s->current_tick + s->exit_tick;
    s->hurt_started = false;
    s->last_facing = o->chara.facing;
    { box_t d = o->def_hurtbox; d.pos_x = o->hurtbox.pos_x; d.pos_y = o->hurtbox.pos_y; d.facing = o->hurtbox.facing; o->hurtbox = d; }
    for (int i = 0; i < s->d.nlimb; i++) s->limb_active[i] = false;
    o->invulnerable = o->projectile_invulnerable = o->roll_projectile_invulnerable = o->throw_invulnerable = false;
    o->aerial_attack_immune = o->grounded_attack_immune = false;
    if (s->d.disable_at_end) o->disabled = true;
}

static void update_sprite_frame(obj_t *o, state_t *s)
{
    const state_def_t *d = &s->d;
    if (s->anim < 0) return;
    if (o->sprite_anim != s->anim) { o->sprite_anim = s->anim; o->sprite_frame = 0; }
    int tpf = d->ticks_per_frame > 0 ? d->ticks_per_frame : 1;
    int sprite_tick = s->current_tick / tpf;
    if (d->loop_animation && d->absolute_loop) sprite_tick = o->current_tick / tpf;
    else if (d->loop_animation && !d->refresh_loop && s->same_as_last_state) sprite_tick = (s->current_tick + s->exit_tick) / tpf;
    int len = d->sprite_anim_length;
    int frame;
    if (d->loop_animation && sprite_tick > d->animation_loop_start) frame = sprite_tick % (len - d->animation_loop_start) + d->animation_loop_start;
    else frame = sprite_tick < len ? sprite_tick : len;
    o->sprite_frame = frame;
}

static void limit_x_speed(obj_t *o, fx limit)
{
    if (fx_abs(o->chara.vx) > limit) o->chara.vx = (o->chara.vx > 0 ? 1 : -1) * limit;
}

sid obj_state_tick_shared(obj_t *o, state_t *s)
{
    const state_def_t *d = &s->d; phys_t *c = &o->chara;
    if (s->current_tick == -1) {
        if (d->spawn_particle_on_enter) obj_emit(o, EV_PARTICLE, s->id);
        if (d->has_enter_force) phys_force_rel(c, d->enter_force_x, d->enter_force_y);
    }
    s->current_real_tick++;
    if (s->current_tick < d->anim_length || d->endless) {
        s->current_tick++;
        eng_process_hitboxes(o, s);
        eng_process_hurtboxes(o, s);
        if (o->can_update_sprite) update_sprite_frame(o, s);
        int t = s->current_tick;
        if (t == d->sfx_tick) obj_emit(o, EV_SFX, s->id);
        if (t == d->force_tick && d->has_force) phys_force_rel(c, d->force_x, d->force_y);
        if (t == d->projectile_tick) obj_emit(o, EV_PROJECTILE, s->id);
        if (t == d->timed_spawn_particle_tick) obj_emit(o, EV_TIMED_PARTICLE, s->id);
        if (t == d->state_screenshake_tick) obj_emit(o, EV_SHAKE, s->id);
        if (t == d->flip_frame) c->facing = -c->facing;
        eng_host_commands(o, s);
        if (t > s->max_tick) s->max_tick = t;
        const state_vt_t *vt = d->vt;
        if (vt && vt->frame_shared) { sid r = vt->frame_shared(o, s, t); if (r != SID_NONE) return r; }
        else if (d->cs && cs_frame_shared) { sid r = cs_frame_shared(o, s, t); if (r != SID_NONE) return r; }
        if (vt && vt->frame) { sid r = vt->frame(o, s, t); if (r != SID_NONE) return r; }
    }
    if (d->apply_fric) phys_fric(c);
    if (d->apply_grav && o->gravity_enabled) phys_grav(c);
    if (d->apply_custom_x_fric) phys_x_fric(c, d->custom_x_fric);
    if (d->apply_custom_y_fric) phys_y_fric(c, d->custom_y_fric);
    if (d->apply_custom_grav && o->gravity_enabled) phys_grav_custom(c, d->custom_grav, d->custom_grav_max_fall_speed);
    if (d->apply_forces) {
        if (d->apply_forces_no_limit) phys_apply_forces_no_limit(c);
        else if (d->apply_custom_limits) {
            limit_x_speed(o, c->grounded ? d->custom_max_ground_speed : d->custom_max_air_speed);
            phys_apply_forces_no_limit(c);
        } else phys_apply_forces(c);
    }
    return SID_NONE;
}

/* ---------- BaseObj ---------- */
void obj_init(obj_t *o, state_t *states, int nstates, sid start)
{
    o->sm.states = states; o->sm.nstates = nstates; o->sm.cur = NULL; o->sm.nstack = 0; o->sm.has_queued = false;
    for (int i = 0; i < nstates; i++) {
        const state_def_t *d = states[i].def;   /* preenchido pelo chamador antes */
        memset(&states[i], 0, sizeof states[i]);
        states[i].def = d; states[i].id = i; states[i].d = *d; states[i].anim = d->sprite_anim;
        if (d->cs) {
            states[i].cs = *d->cs; states[i].d.cs = &states[i].cs;
            if (states[i].cs.iasa_at < 0) states[i].cs.iasa_at += d->anim_length;         /* CharacterState.init */
        }
        states[i].current_tick = states[i].current_real_tick = states[i].start_tick = -1;
        states[i].exit_tick = states[i].max_tick = states[i].max_tick_shared = -1;
        states[i].last_facing = 1; states[i].cur_hurt = -1;
        state_setup_runtime(o, &states[i]);
    }
    o->def_hurtbox = o->hurtbox;
    sm_change_state(o, start, NULL, true, true);
    o->sm.initialized = true; o->initialized = true;
}

void obj_state_tick(obj_t *o)
{
    bool once = true;
    while (once || o->sm.cur->current_tick < 0) {
        once = false;
        sm_tick(o);
        state_t *s = o->sm.cur;
        if (!s->d.endless && s->current_tick >= s->d.anim_length && !o->sm.has_queued) {
            sm_queue_state(o, s->d.fallback_state, NULL);
            sm_tick(o);
        }
    }
}

void obj_tick(obj_t *o)
{
    if (o->hitlag_ticks > 0) o->hitlag_ticks--;
    else {
        obj_state_tick(o);
        o->current_tick++; o->game_tick++;
        phys_update_grounded(&o->chara);
    }
    o->can_update_sprite = true;
}

void obj_change_state(obj_t *o, sid st, void *data) { sm_change_state(o, st, data, true, true); }
