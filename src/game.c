/* Porte de game.gd: tick(), prioridade de porta, colisoes, apply_hitboxes. Objetos extras: projectile.c (futuro). */
#include "game.h"
#include <stdlib.h>

static inline int iabs(int n) { return n < 0 ? -n : n; }
static int iclamp(int n, int lo, int hi) { if (n > lo && n < hi) return n; if (n <= lo) return lo; return hi; }   /* int_clamp do jogo */

static void set_x(obj_t *o, int x) { o->chara.px = FX_INT(x); obj_update_boxes(o); }
static void set_y(obj_t *o, int y) { o->chara.py = FX_INT(y); obj_update_boxes(o); }

static obj_t *eng_opp_game;
obj_t *eng_opponent(obj_t *o) { return o->fighter ? &o->fighter->opponent->o : NULL; }

void game_init(game_t *g, fighter_t *p1, fighter_t *p2)
{
    g->p[0] = p1; g->p[1] = p2;
    g->current_tick = 0; g->time = 3000; g->stage_width = 1000; g->max_char_distance = 600;
    g->ceiling_height = 400; g->has_ceiling = true; g->game_finished = false; g->nobjs = 0;
    p1->stage_width = p2->stage_width = g->stage_width;
    (void)eng_opp_game;
}

/* ---- prioridade de tick (resolve_port_priority) ---- */

static int attacks_prio(const state_t *a, const state_t *b)
{
    bool ah = a->d.has_hitboxes, bh = b->d.has_hitboxes;     /* "filhos Hitbox" do estado: inclui Detect? (has_hitboxes exclui) */
    if (a->d.nhb == 0 && b->d.nhb == 0) return 0;
    if (a->d.nhb == 0) return 2;
    if (b->d.nhb == 0) return 1;
    (void)ah; (void)bh;
    int as = 999, bs = 999, ad = 0, bd = 0;
    for (int i = 0; i < a->d.nhb; i++) if (a->d.hb[i].start_tick < as) { as = a->d.hb[i].start_tick; ad = a->hbr[i].damage; }
    for (int i = 0; i < b->d.nhb; i++) if (b->d.hb[i].start_tick < bs) { bs = b->d.hb[i].start_tick; bd = b->hbr[i].damage; }
    if (as != bs) return as > bs ? 1 : 2;
    if (ad != bd) return ad > bd ? 1 : 2;
    return 0;
}

static bool is_hurt(const state_t *s) { return state_is_hurt(s); }

static int port_priority(game_t *g)
{
    fighter_t *p1 = g->p[0], *p2 = g->p[1];
    state_t *s1 = p1->o.sm.cur, *s2 = p2->o.sm.cur;
    int tp1 = s1->d.cs ? s1->cs.tick_priority : 0, tp2 = s2->d.cs ? s2->cs.tick_priority : 0;
    int pr = 0;
    if (tp1 < tp2) pr = 1; else if (tp2 < tp1) pr = 2;
    if (!pr) { if (is_hurt(s1)) pr = 2; else if (is_hurt(s2)) pr = 1; }
    if (!pr) pr = attacks_prio(s1, s2);
    if (!pr && abs(p1->penalty - p2->penalty) >= 10) pr = p1->penalty < p2->penalty ? 1 : 2;
    if (!pr) {
        bool b1 = s1->d.cs && s1->cs.beats_backdash, b2 = s2->d.cs && s2->cs.beats_backdash;
        if (b1 && !b2) pr = 1; else if (!b1 && b2) pr = 2;
    }
    if (!pr) {
        int h1 = p1->hp / p1->max_health, h2 = p2->hp / p2->max_health;      /* divisao inteira, como no original */
        if (h1 != h2) pr = h1 < h2 ? 1 : 2;
    }
    return pr < 1 ? 1 : pr;
}

/* ---- posicoes ---- */

static void resolve_same_x(game_t *g)
{
    obj_t *a = &g->p[0]->o, *b = &g->p[1]->o;
    int ax = phys_x(&a->chara), bx = phys_x(&b->chara);
    if (ax != bx) return;
    obj_t *mv = g->current_tick % 2 == 0 ? a : b;
    int dir = g->current_tick % 2 == 0 ? 1 : -1;
    if (ax < 0) { dir = 1; if (a->chara.facing == -1) mv = a; else if (b->chara.facing == -1) mv = b; }
    else if (ax > 0) { dir = -1; if (a->chara.facing == 1) mv = a; else if (b->chara.facing == 1) mv = b; }
    set_x(mv, phys_x(&mv->chara) + dir);
}

static bool colliding(const fighter_t *f)
{
    const state_t *s = f->o.sm.cur;
    bool hurt_recent = is_hurt(s) && (f->hitlag_applied - f->o.hitlag_ticks) < HITLAG_COLLISION_TICKS;
    return (f->colliding_with_opponent || hurt_recent) && s->id != f->st_grabbed;
}

static void update_cboxes(fighter_t *f)
{
    obj_t *o = &f->o;
    o->hurtbox.facing = o->collision_box.facing = o->chara.facing;
    box_update_position(&o->collision_box, phys_x(&o->chara), phys_y(&o->chara));
    box_update_position(&o->hurtbox, phys_x(&o->chara), phys_y(&o->chara));
}

static void resolve_collisions(game_t *g, fighter_t *p1, fighter_t *p2, int step)
{
    update_cboxes(p1); update_cboxes(p2);
    int x = phys_x(&p1->o.chara), ox = phys_x(&p2->o.chara);
    const box_t *c1 = &p1->o.collision_box, *c2 = &p2->o.collision_box;
    int p1r = x + c1->w + c1->x, p1l = x - c1->w + c1->x, p2r = ox + c2->w + c2->x, p2l = ox - c2->w + c2->x;
    int edge_distance = x < ox ? iabs(p2r - p1l) : iabs(p1r - p2l);
    if (colliding(p1) && colliding(p2) && box_overlaps(c1, c2)) {
        bool push1l = p1->o.chara.facing == 1;
        if (p1->reverse_state) push1l = !push1l;
        bool push2l = p1->o.chara.facing == -1;
        if (p2->reverse_state) push2l = !push2l;
        if (push1l) {
            if (p2l < p1r) { int ov = iabs(p2l - p1r); set_x(&p1->o, x - ov / 2); set_x(&p2->o, ox + ov / 2); }
        } else if (push2l) {
            if (p2r > p1l) { int ov = iabs(p2r - p1l); set_x(&p1->o, x + ov / 2); set_x(&p2->o, ox - ov / 2); }
        }
    }
    if (edge_distance > g->max_char_distance) {
        int mid = (x + ox) / 2;
        int left = iclamp(mid - g->max_char_distance / 2, -g->stage_width, g->stage_width);
        int right = iclamp(mid + g->max_char_distance / 2, -g->stage_width, g->stage_width);
        set_x(&p1->o, iclamp(x, left + (c1->w + c1->x), right + (-c1->w + c1->x)));
        set_x(&p2->o, iclamp(ox, left + (c2->w + c2->x), right + (-c2->w + c2->x)));
    }
    int y1 = phys_y(&p1->o.chara), y2 = phys_y(&p2->o.chara);
    if (g->has_ceiling) {
        if (y1 - c1->h * 2 < -g->ceiling_height) { set_y(&p1->o, -g->ceiling_height + c1->h * 2); p1->o.chara.vy = 0; }
        if (y2 - c2->h * 2 < -g->ceiling_height) { set_y(&p2->o, -g->ceiling_height + c2->h * 2); p2->o.chara.vy = 0; }
    }
    if (step < 5) {
        if (!p1->clipping_wall && x - c1->w < -g->stage_width) { set_x(&p1->o, -g->stage_width + c1->w); resolve_collisions(g, p1, p2, step + 1); return; }
        else if (!p1->clipping_wall && x + c1->w > g->stage_width) { set_x(&p1->o, g->stage_width - c1->w); resolve_collisions(g, p1, p2, step + 1); return; }
        if (!p2->clipping_wall && ox - c2->w < -g->stage_width) { set_x(&p2->o, -g->stage_width + c2->w); resolve_collisions(g, p1, p2, step + 1); return; }
        else if (!p2->clipping_wall && ox + c2->w > g->stage_width) { set_x(&p2->o, g->stage_width - c2->w); resolve_collisions(g, p1, p2, step + 1); return; }
        if (colliding(p1) && colliding(p2) && box_overlaps(&p1->o.collision_box, &p2->o.collision_box)) { resolve_collisions(g, p1, p2, step + 1); return; }
    }
}

/* ---- hitboxes ---- */

/* get_colliding_hitbox: ULTIMO hitbox valido (lista ordenada por priority crescente) */
static hitbox_t *colliding_hitbox(hitbox_t **hbs, int n, obj_t *victim)
{
    hitbox_t *hit_by = NULL;
    for (int i = 0; i < n; i++) {
        hitbox_t *h = hbs[i];
        const hitbox_def_t *d = h->def;
        obj_t *attacker = h->host;
        fighter_t *vf = victim->is_fighter ? victim->fighter : NULL;
        bool grounded = victim->chara.grounded;
        bool otg = vf ? fighter_is_otg(vf) : false;
        if (!hurtbox_hit_check(NULL, h, victim)) continue;
        if (d->is_throwbox) {
            bool can_be_thrown = !victim->throw_invulnerable && (!vf || vf->blockstun_ticks <= 0);
            if (!can_be_thrown) { if (vf && vf->blockstun_ticks > 0) hitbox_save_hit(h, victim->uid); continue; }
            if (vf && vf->wakeup_throw_immunity_ticks > 0) continue;
        }
        if ((!d->hits_vs_aerial && !grounded) || (!d->hits_vs_grounded && grounded)) continue;
        if (!otg && !d->hits_vs_standing) continue;
        if (otg && !d->hits_otg) continue;
        if (!vf && !d->hits_projectiles) continue;
        if (hitbox_already_hit(h, victim->uid)) continue;
        if (attacker) {
            if (!attacker->chara.grounded) { if (victim->aerial_attack_immune) continue; }
            else if (victim->grounded_attack_immune) continue;
            if (attacker->id == victim->id && !d->allowed_to_hit_own_team) continue;
        }
        hit_by = h;
    }
    return hit_by;
}

static void apply_hitboxes(game_t *g, fighter_t *px1, fighter_t *px2)
{
    hitbox_t *h1[32], *h2[32];
    int n1 = obj_active_hitboxes(&px1->o, h1, 32), n2 = obj_active_hitboxes(&px2->o, h2, 32);
    for (int i = 0; i < n1; i++) box_update_position(&h1[i]->box, phys_x(&px1->o.chara), phys_y(&px1->o.chara));
    for (int i = 0; i < n2; i++) box_update_position(&h2[i]->box, phys_x(&px2->o.chara), phys_y(&px2->o.chara));
    hitbox_t *p2_by = !px2->o.invulnerable ? colliding_hitbox(h1, n1, &px2->o) : NULL;
    hitbox_t *p1_by = !px1->o.invulnerable ? colliding_hitbox(h2, n2, &px1->o) : NULL;
    bool p1_hit = false, p2_hit = false, p1_throwing = false, p2_throwing = false;
    if (p1_by) { if (!p1_by->def->is_throwbox) p1_hit = true; else p2_throwing = true; }
    if (p2_by) { if (!p2_by->def->is_throwbox) p2_hit = true; else p1_throwing = true; }
    /* clash: desligado por padrao no original (clashing_enabled=false) */
    if (p1_hit) { if (!(p1_throwing && !p1_by->def->beats_grab)) eng_hitbox_hit(p1_by, &px1->o); else p1_hit = false; }
    if (p2_hit) { if (!(p2_throwing && !p2_by->def->beats_grab)) eng_hitbox_hit(p2_by, &px2->o); else p2_hit = false; }

    bool hittable = true;
    int tech = fighter_name_id(px1, "ThrowTech");
    if (!p2_hit && !p1_hit) {
        bool t1 = px1->o.sm.cur->d.cs && px1->o.sm.cur->cs.throw_techable, t2 = px2->o.sm.cur->d.cs && px2->o.sm.cur->cs.throw_techable;
        if (p2_throwing && p1_throwing && t1 && t2) {
            sm_queue_state(&px1->o, tech, NULL); sm_queue_state(&px2->o, tech, NULL); hittable = false;
        } else if (p2_throwing && p1_throwing && !t1 && !t2) hittable = false;
        else if (p1_throwing || p2_throwing) {
            fighter_t *thrower = p1_throwing ? px1 : px2, *victim = p1_throwing ? px2 : px1;
            hitbox_t *tb = p1_throwing ? p2_by : p1_by;
            if (t1 && t2) { sm_queue_state(&px1->o, tech, NULL); sm_queue_state(&px2->o, tech, NULL); hittable = false; }
            bool can_hit = true;
            if (victim->o.chara.grounded && !tb->def->hits_vs_grounded) can_hit = false;
            if (!victim->o.chara.grounded && !tb->def->hits_vs_aerial) can_hit = false;
            if (!hittable) can_hit = false;
            if (can_hit) {
                eng_hitbox_hit(tb, &victim->o);
                if (tb->def->throw_state >= 0) sm_queue_state(&thrower->o, tb->def->throw_state, NULL);
                hittable = false;
            }
        }
    }
    (void)hittable; (void)g;
    /* objetos extras (projeteis): projectile.c */
}

/* ---- tick ---- */

void game_tick(game_t *g)
{
    fighter_t *p1 = g->p[0], *p2 = g->p[1];
    for (int i = 0; i < g->nobjs; i++) {
        obj_t *o = g->objs[i];
        if (o->disabled) continue;
        obj_tick(o);
        int x = phys_x(&o->chara);
        if (x < -g->stage_width) set_x(o, -g->stage_width); else if (x > g->stage_width) set_x(o, g->stage_width);
    }
    g->current_tick++;
    p1->o.current_tick = p2->o.current_tick = g->current_tick;
    p1->lowest_tick = p2->lowest_tick = -1;
    fighter_t *pl[2] = { p1, p2 };
    if (port_priority(g) != 1) { pl[0] = p2; pl[1] = p1; }

    fighter_tick_before(pl[0]); fighter_tick_before(pl[1]);
    fighter_update_advantage(pl[0]); fighter_update_advantage(pl[1]);
    fighter_tick(pl[0]); fighter_tick(pl[1]);
    resolve_same_x(g);
    resolve_collisions(g, pl[0], pl[1], 0);
    apply_hitboxes(g, pl[0], pl[1]);
    if ((p1->o.state_interruptable || p1->dummy_interruptable) && !p1->busy_interrupt) fighter_reset_combo(p2);
    if ((p2->o.state_interruptable || p2->dummy_interruptable) && !p2->busy_interrupt) fighter_reset_combo(p1);
}

bool game_should_end(const game_t *g)
{
    return g->current_tick > g->time || g->p[0]->hp <= 0 || g->p[1]->hp <= 0;
}
