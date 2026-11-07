/* Scripts de estados do Robo (characters/robo/states). */
#include "robo.h"
#include "states.h"
#include <string.h>
#include <stdlib.h>

#define F(o) ((o)->fighter)
static inline fx fxi(int v) { return FX_INT(v); }
static sid by_name(fighter_t *f, const char *n) { return fighter_name_id(f, n); }
static int moves_used(fighter_t *f, const char *name) __attribute__((unused));
static int moves_used(fighter_t *f, const char *name)
{
    int id = by_name(f, name);
    for (int i = 0; i < f->nmoves_used; i++) if (f->moves_used[i].name_id == id) return f->moves_used[i].count;
    return -1;
}
static void limit_speed(phys_t *c, fx limit)
{
    if (fx_vec_len(c->vx, c->vy) > limit) { fxv n = fxv_norm_times(fxv_make(c->vx, c->vy), limit); c->vx = n.x; c->vy = n.y; }
}
static bool is_prev(obj_t *o, const char *n) { state_t *p = obj_prev_state(o); return p && strcmp(p->d.name, n) == 0; }

/* ---------- Roll (extends RollStartup) e RobotState ---------- */
const state_vt_t vt_characters_robo_Roll = { VT_ROLL };
const state_vt_t vt_characters_robo_RobotState = { 0 };

/* ---------- Step: ustate 0 startup_lag ---------- */
static sid step_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o); robo_t *r = ROBO(f);
    if (!is_prev(o, "Step") && !is_prev(o, "Return")) r->step_count = 0;
    if (!is_prev(o, "Return")) r->step_count++;
    if (f->combo_count <= 0) s->ustate[0] += 3;
    if (f->reverse_state) { s->cs.beats_backdash = false; s->ustate[0] += 2; fighter_add_penalty(f, 10, false); }
    else s->cs.beats_backdash = true;
    return SID_NONE;
}
static sid step_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o);
    switch (t) {
    case 0: if (xp_int(&s->d, "charged", 0)) f->has_projectile_armor = true; break;
    case 5: if (!f->reverse_state) phys_move_rel(&o->chara, fxi(6), 0); break;
    case 6: s->cs.self_interruptable = false; s->cs.next_state_on_hold = false; s->cs.next_state_on_hold_on_opponent_turn = false;
            phys_move_rel(&o->chara, fxi(15), 0); break;
    case 9: s->cs.self_interruptable = true; s->cs.next_state_on_hold = true; s->cs.next_state_on_hold_on_opponent_turn = true; break;
    }
    return SID_NONE;
}
static sid step_tick(obj_t *o, state_t *s)
{
    if (s->current_tick >= 1 && s->ustate[0] > 0) { s->ustate[0]--; s->current_tick = 1; }
    if (xp_int(&s->d, "charged", 0)) phys_apply_forces_no_limit(&o->chara); else obj_apply_forces(o);
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_Step = { .enter = step_enter, .frame = step_frame, .tick = step_tick };

/* ---------- Landing (extends Landing): ustate 0 has_hitbox ---------- */
extern const chardata_t chardata_robo_LandingShockwave;
static sid rl_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o); robo_t *r = ROBO(f);
    if (t != 0) return land_frame(o, s, t);
    land_frame(o, s, t);
    state_t *prev = obj_prev_state(o);
    fx speed = f->last_aerial_vel.y;
    bool has_hitbox = !(prev && prev->cs.busy_interrupt_type == 1) && !(prev && state_is_hurt(prev));
    s->ustate[0] = has_hitbox;
    hitbox_t *hb = s->d.nhb > 0 ? &s->hbr[0] : NULL;
    bool hvg = r->can_ground_pound && speed > FXC(5.0) && has_hitbox;
    int damage = (int)fx_round(fx_mul(speed, fxi(10)));
    if (damage < 10) damage = 10;
    damage = (int)fx_round(fx_mul(FXC(0.3), fxi(damage)));
    if (hb) hb->hits_vs_grounded = hvg;
    if (hvg) {
        proj_t *l = proj_spawn(eng_game_get(), &chardata_robo_LandingShockwave, o, -16, -1, true, NULL, 0, true);
        proj_t *rr = proj_spawn(eng_game_get(), &chardata_robo_LandingShockwave, o, 16, -1, true, NULL, 0, true);
        if (rr) { rr->o.chara.facing = 1; rr->ix = damage; rr->force_x = speed; }
        if (l) { l->o.chara.facing = -1; l->ix = damage; l->force_x = speed; }
    }
    if (hb) hb->damage = damage;
    return SID_NONE;
}
static sid rl_tick(obj_t *o, state_t *s)
{
    sid r = land_tick(o, s);
    if (s->ustate[0]) s->cs.interruptible_on_opponent_turn = false;
    return r;
}
const state_vt_t vt_characters_robo_states_Landing = { .frame = rl_frame, .tick = rl_tick };

/* ---------- Drive* ---------- */
static sid df_enter(obj_t *o, state_t *s)
{
    if (strcmp(s->d.name, "DriveForward") == 0 && is_prev(o, "DriveBackward")) { s->ustate[0] = 6; phys_reset_momentum(&o->chara); }
    return SID_NONE;
}
static sid df_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o); (void)f;
    int dir = xp_int(&s->d, "dir", 1);
    fx speed = xp_fx(&s->d, "SPEED", FXC(3.0));
    if (s->ustate[0] > 0) { s->ustate[0]--; s->current_tick = 0; }
    if (s->ustate[0] <= 0 && dir != 0) phys_force_rel(&o->chara, fx_mul(speed, fxi(dir)), 0);
    phys_apply_forces_no_limit(&o->chara);
    limit_speed(&o->chara, fxi(20));
    return SID_NONE;
}
static sid df_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o); robo_t *r = ROBO(f);
    int dir = xp_int(&s->d, "dir", 1);
    if (t == 0) { if (dir == 1) f->has_projectile_armor = true; else if (f->combo_count <= 0) fighter_add_penalty(f, 5, false); }
    else if (t == 1 && dir == 1) {
        hitbox_t *hb = s->d.nhb > 0 ? &s->hbr[0] : NULL;
        if (r->drive_cancel) { if (hb) hb->cancellable = false; s->d.fallback_state = by_name(f, "UnDriveCancel"); s->d.anim_length = 20; }
        else { s->d.fallback_state = by_name(f, "DriveIdle"); if (hb) hb->cancellable = true; s->d.anim_length = 12; }
    } else if (t == 5) f->has_projectile_armor = false;
    return SID_NONE;
}
static void df_parried(obj_t *o, state_t *s) { s->cs.interruptible_on_opponent_turn = false; o->hitlag_ticks += 3; }
const state_vt_t vt_characters_robo_states_DriveForward = { .enter = df_enter, .frame = df_frame, .tick = df_tick, .got_perfect_parried = df_parried };

static sid dj_frame(obj_t *o, state_t *s, int t)
{
    if (t != 0) return SID_NONE;
    const xy_data_t *d = (const xy_data_t *)s->data;
    fxv dir = obj_xy_to_dir(fxi(d ? d->x : 0), 0, fxi(-10));
    fx tt = fx_div(dir.x - 0, fxi(-10));                        /* inverse_lerp(0, -10, v) */
    fx amount = fx_lerp(fxi(-5), fxi(-10), tt);
    phys_force_rel(&o->chara, FXC(5.0), amount);
    phys_move(&o->chara, 0, fxi(-1));
    o->chara.grounded = false;
    return SID_NONE;
}
static sid dj_tick(obj_t *o, state_t *s)
{
    phys_t *c = &o->chara;
    phys_x_fric(c, FXC(0.015));
    phys_grav_custom(c, FXC(0.6), fxi(16));
    phys_apply_forces_no_limit(c);
    if (s->current_tick > 1 && c->grounded) return by_name(F(o), "Landing");
    if (s->current_tick == 20) cs_enable_interrupt(F(o), s, true, false);
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_DriveJump = { .frame = dj_frame, .tick = dj_tick };

static sid dw_frame(obj_t *o, state_t *s, int t)
{
    bool boost = xp_int(&s->d, "boost", 0);
    if (t == 1 && boost && F(o)->initiative) o->invulnerable = true;
    else if (t == 6) o->invulnerable = false;
    return SID_NONE;
}
static sid dw_tick(obj_t *o, state_t *s) { (void)s; phys_apply_forces_no_limit(&o->chara); phys_x_fric(&o->chara, FXC(0.15)); return SID_NONE; }
const state_vt_t vt_characters_robo_states_DriveWheelie = { .frame = dw_frame, .tick = dw_tick };

static sid dd_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t == 0) o->projectile_invulnerable = true; else if (t == 10) o->projectile_invulnerable = false;
    return SID_NONE;
}
static sid dd_tick(obj_t *o, state_t *s)
{
    phys_apply_forces_no_limit(&o->chara);
    phys_x_fric(&o->chara, s->current_tick < 8 ? FXC(0.12) : FXC(0.07));
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_DriveDrift = { .frame = dd_frame, .tick = dd_tick };

/* ---------- Plummet: ustate 0 descending, 1 apply_gravity ---------- */
static sid pl_frame(obj_t *o, state_t *s, int t)
{
    (void)o;
    if (t == 0) { s->ustate[0] = 0; s->ustate[1] = 0; } else if (t == 3) s->ustate[0] = 1;
    return SID_NONE;
}
static sid pl_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o); robo_t *r = ROBO(f); phys_t *c = &o->chara;
    if (s->ustate[0]) {
        c->vx = fx_mul(c->vx, FXC(0.05)); c->vy = fxi(13);
        s->ustate[0] = 0;
        if (r->flying) r->fly_x = 0;
    }
    if (c->vy < 0) s->ustate[1] = 1;
    if (!r->flying && s->ustate[1]) obj_apply_grav(o);
    obj_apply_forces(o);
    if (c->grounded) return by_name(f, "Landing");
    return SID_NONE;
}
static void pl_parried(obj_t *o, state_t *s) { (void)s; o->hitlag_ticks += 4; }
const state_vt_t vt_characters_robo_states_Plummet = { .frame = pl_frame, .tick = pl_tick, .got_perfect_parried = pl_parried };

/* ---------- Propel ---------- */
static sid pr_enter(obj_t *o, state_t *s) { s->cs.next_state_on_hold = !ROBO(F(o))->buffer_drive_cancel; return SID_NONE; }
static sid pr_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t == 0) { o->chara.gfric = FXC(0.2); ROBO(F(o))->propel_friction_ticks = 20; }
    return SID_NONE;
}
static sid pr_tick(obj_t *o, state_t *s) { if (s->current_tick > 3) phys_force_rel(&o->chara, FXC(1), 0); return SID_NONE; }
const state_vt_t vt_characters_robo_states_Propel = { .enter = pr_enter, .frame = pr_frame, .tick = pr_tick };

/* ---------- ClapJump: ustate 0 jumping, 1 auto_lag; ufx 0 grav, 1 max_fall ---------- */
static sid cj_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o); phys_t *c = &o->chara;
    const clap_data_t *d = (const clap_data_t *)s->data;
    if (t == 0) {
        s->ustate[1] = 4;
        bool g = c->grounded;
        s->ufx[0] = g ? FXC(0.58) : FXC(0.80);
        s->ufx[1] = g ? FXC(3.0) : FXC(8.0);
        c->grounded = false;
        s->ustate[0] = 1;
        hitbox_t *hb = s->d.nhb > 0 ? &s->hbr[0] : NULL;
        if (hb) {
            if (moves_used(f, s->d.name) < 0) { hb->victim_hitlag = hb->def->hitlag_ticks + 15; hb->ground_bounce = false; }
            else { hb->victim_hitlag = hb->def->hitlag_ticks; hb->ground_bounce = true; }
            hb->grounded_hit_state = by_name(f, "HurtAerial");
            hb->combo_victim_hitlag = hb->victim_hitlag;
            hb->combo_scaling_amount = d && d->autoo ? 2 : 1;
        }
    } else if (t == 1) phys_move_rel(c, fxi(-10), 0);
    else if (t == 2) { phys_move_rel(c, fxi(1), fxi(-3)); o->throw_invulnerable = true; }
    else if (t == 3) {
        fxv force = fxv_norm_times(fxv_make(FX_ONE, FXC(-0.25)), fxi(8));
        if (d && d->dir.x * c->facing == -1) force.x = fx_mul(force.x, FXC(-0.55));
        phys_force_rel(c, force.x, force.y);
    } else if (t == 7) o->throw_invulnerable = false;
    return SID_NONE;
}
static sid cj_tick(obj_t *o, state_t *s)
{
    const clap_data_t *d = (const clap_data_t *)s->data;
    phys_grav_custom(&o->chara, s->ufx[0], s->ufx[1]);
    if (s->ustate[0] && s->current_tick >= 10 && s->ustate[1] > 0) { s->current_tick = 9; if (d && d->autoo) s->ustate[1]--; }
    if (s->ustate[0] && s->current_tick > 1 && o->chara.grounded) s->ustate[0] = 0;
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_ClapJump = { .frame = cj_frame, .tick = cj_tick };

/* ---------- GroundSlam (extends ThrowState): ustate 0 = released (ThrowState); 4 is_in_combo, 5 single_hit_wait ---------- */
static void gs_slam(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    hitbox_t *nc = &s->hbr[0], *hb = &s->hbr[1];
    if (!s->ustate[4]) eng_hitbox_hit(nc, &f->opponent->o); else eng_hitbox_hit(hb, &f->opponent->o);
    hitbox_deactivate(nc); hitbox_deactivate(hb);
}
static sid gs_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o); phys_t *c = &o->chara;
    bool single = xp_int(&s->d, "single_hit", 0);
    switch (t) {
    case 0: phys_force_rel(c, fxi(-4), 0); s->ustate[4] = f->combo_count != 0; if (single) s->current_tick = 31; break;
    case 7: phys_move_rel(c, fxi(-16), 0); break;
    case 16: phys_force_rel(c, fxi(6), 0); break;
    case 21: gs_slam(o, s); phys_move_rel(c, fxi(16), 0); break;
    case 28: phys_force_rel(c, fxi(-4), 0); phys_move_rel(c, fxi(-16), 0); break;
    case 32:
        if (single) { s->ustate[5] = 2; c->facing = -c->facing; }
        break;
    case 36: phys_force_rel(c, fxi(6), 0); break;
    case 39: phys_move_rel(c, fxi(16), 0); break;
    case 40: gs_slam(o, s); th_release_now(f, s); break;
    case 44: phys_force_rel(c, fxi(-4), 0); break;
    }
    return SID_NONE;
}
static sid gs_tick(obj_t *o, state_t *s)
{
    (void)o;
    if (s->ustate[5] > 0) { s->ustate[5]--; s->current_tick = 32; }
    return SID_NONE;
}
static void gs_exit(obj_t *o, state_t *s) { th_exit(o, s); s->ustate[4] = 0; }
const state_vt_t vt_characters_robo_states_GroundSlam = { .enter = th_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = gs_exit, .frame = gs_frame, .tick = gs_tick };

/* ---------- Chainsaw: ufx 0..3 start/end xy; ustate 0 last_has, 1 lx, 2 ly ---------- */
static sid saw_enter(obj_t *o, state_t *s)
{
    const saw_data_t *d = (const saw_data_t *)s->data;
    int st = xp_int(&s->d, "chainsaw_start_tick", 10), en = xp_int(&s->d, "chainsaw_end_tick", 19);
    for (int i = 0; i < s->d.nhb && i < 2; i++) { s->hbr[i].start_tick = st + 1; s->hbr[i].active_ticks = en - st + 1; }
    s->cs.iasa_on_hit = en;
    if (!d) return SID_NONE;
    fxv sv = obj_xy_to_dir(fxi(d->start.x), fxi(d->start.y), FX_ONE), ev = obj_xy_to_dir(fxi(d->end.x), fxi(d->end.y), FX_ONE);
    if (o->chara.facing < 0) { sv.y = -sv.y; ev.y = -ev.y; }
    s->ufx[0] = sv.x; s->ufx[1] = sv.y; s->ufx[2] = ev.x; s->ufx[3] = ev.y;
    return SID_NONE;
}
static sid saw_frame(obj_t *o, state_t *s, int t)
{
    (void)o;
    if (t == 0) { s->ustate[0] = 0; s->cs.iasa_at = 34; }
    return SID_NONE;
}
static sid saw_tick(obj_t *o, state_t *s)
{
    int st = xp_int(&s->d, "chainsaw_start_tick", 10), en = xp_int(&s->d, "chainsaw_end_tick", 19);
    int ct = s->current_tick;
    if (ct < st || ct > en || s->d.nhb < 2) return SID_NONE;
    int face = o->chara.facing;
    fx t = fx_abs(fx_div(fxi(st - ct), fxi(en - st)));
    fx ang = fx_lerp_angle(fx_vec_to_angle(s->ufx[0], s->ufx[1]), fx_vec_to_angle(s->ufx[2], s->ufx[3]), t);
    fxv p = fx_angle_to_vec(ang);
    fxv h1 = fxv_mul(p, fxi(48));
    fxv h2 = fxv_div(h1, fxi(2));
    int cx = o->hurtbox.x, cy = o->hurtbox.y;
    hitbox_t *a = &s->hbr[0], *b = &s->hbr[1];
    a->box.x = face * ((int)fx_round(h1.x) + cx);
    a->box.y = ((int)(fx_floor(h1.y) >> FX_SHIFT) * face + cy) - 6;
    b->box.x = face * ((int)fx_round(h2.x) + cx);
    b->box.y = ((int)(fx_floor(h2.y) >> FX_SHIFT) * face + cy) - 6;
    fxv kb = fxv_make(FX_ONE, -FX_ONE);
    if (s->ustate[0]) kb = fxv_norm(fxv_make(fxi(a->box.x - s->ustate[1]), fxi(a->box.y - s->ustate[2])));
    s->ustate[0] = 1; s->ustate[1] = a->box.x; s->ustate[2] = a->box.y;
    a->dir_x = b->dir_x = kb.x; a->dir_y = b->dir_y = kb.y;
    return SID_NONE;
}
static void saw_exit(obj_t *o, state_t *s) { (void)o; s->ustate[0] = 0; }
const state_vt_t vt_characters_robo_states_Chainsaw = { .enter = saw_enter, .frame = saw_frame, .tick = saw_tick, .exit = saw_exit };

/* ---------- LOIC: data {Self, Dir?, Delay} ---------- */
static sid loic_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o); robo_t *r = ROBO(f);
    const loic_data_t *d = (const loic_data_t *)s->data;
    r->loic_draining = true; r->can_loic = false;
    s->ustate[0] = d ? d->self_ : 0;
    if (d && d->has_dir) r->loic_dir = d->dir.x;
    return SID_NONE;
}
static void loic_process(obj_t *o, state_t *s, obj_t *pr)
{
    fighter_t *f = F(o); robo_t *r = ROBO(f);
    const loic_data_t *d = (const loic_data_t *)s->data;
    bool self_ = s->ustate[0];
    phys_set_pos(&pr->chara, fxi(phys_x(self_ ? &o->chara : &f->opponent->o.chara)), 0);
    fx t = fx_div(fxi(d ? d->delay.x : 0), fxi(100));
    int mn = self_ ? 20 : 14;
    t = fx_mul(t, fxi(85 - mn)) + fxi(mn);
    PROJ(pr)->ix = (int)fx_round(t);       /* aim_ticks */
    PROJ(pr)->iy = self_;
    r->orbital_strike_out = true; r->orbital_uid = pr->uid;
}
static bool loic_usable(fighter_t *f, state_t *s, bool base) { (void)s; return base && !ROBO(f)->orbital_strike_out && ROBO(f)->can_loic; }
const state_vt_t vt_characters_robo_states_LOIC = { .enter = loic_enter, .process_projectile = loic_process, .usable = loic_usable };

/* ---------- KillProcessLanding ---------- */
static sid kpl_enter(obj_t *o, state_t *s)
{
    fighter_t *f = F(o); int lv = ROBO(f)->kill_process_super_level;
    if (s->d.nhb > 0) {
        hitbox_t *hb = &s->hbr[0];
        hb->damage = 150 + 75 * lv;
        hb->damage_in_combo = 150 + 30 * lv;
        hb->minimum_damage = 20 * lv;
    }
    o->invulnerable = true;
    ROBO(f)->super_armor_installed = true;
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_KillProcessLanding = { .enter = kpl_enter };

/* ---------- KillProcessSpin ---------- */
static sid kps_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    phys_grav_custom(&o->chara, FXC(0.66), fxi(20));
    if (o->chara.grounded) {
        f->throw_pos_x = xp_int(&s->d, "release_throw_pos_x", 0);
        f->throw_pos_y = xp_int(&s->d, "release_throw_pos_y", 0);
        int gx = phys_x(&o->chara) + f->throw_pos_x * o->chara.facing, gy = phys_y(&o->chara) + f->throw_pos_y;
        phys_set_pos(&f->opponent->o.chara, fxi(gx), fxi(gy));
        fighter_update_facing(f->opponent);
        return by_name(f, "KillProcessLanding");
    }
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_KillProcessSpin = { .tick = kps_tick };

/* ---------- Magnetize ---------- */
static sid mg_frame(obj_t *o, state_t *s, int t) { (void)s; if (t == 0) robo_start_magnetizing(F(o)); return SID_NONE; }
static sid mg_tick(obj_t *o, state_t *s) { if (s->current_tick == 7 && F(o)->combo_count > 0) cs_enable_interrupt(F(o), s, true, false); return SID_NONE; }
static bool mg_usable(fighter_t *f, state_t *s, bool base) { (void)s; return base && ROBO(f)->magnet_ticks_left <= 0 && ROBO(f)->grenade_uid >= 0; }
const state_vt_t vt_characters_robo_states_Magnetize = { .frame = mg_frame, .tick = mg_tick, .usable = mg_usable };

/* ---------- DisableLoic ---------- */
static sid dl_enter(obj_t *o, state_t *s)
{
    (void)s; robo_t *r = ROBO(F(o));
    obj_t *ob = r->orbital_uid >= 0 ? game_obj_by_uid(eng_game_get(), r->orbital_uid) : NULL;
    if (ob) robo_orbital_deactivate(ob);
    return SID_NONE;
}
static bool dl_usable(fighter_t *f, state_t *s, bool base)
{
    (void)s; robo_t *r = ROBO(f);
    obj_t *ob = r->orbital_uid >= 0 ? game_obj_by_uid(eng_game_get(), r->orbital_uid) : NULL;
    return base && ob && robo_orbital_active(ob) && !robo_orbital_deactivating(ob);
}
const state_vt_t vt_characters_robo_states_DisableLoic = { .enter = dl_enter, .usable = dl_usable };

/* ---------- Fall (extends Fall) ---------- */
static sid rf_enter(obj_t *o, state_t *s) { (void)o; s->cs.iasa_at = 11; return SID_NONE; }
static sid rf_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    sid r = fall_tick(o, s);
    if (ROBO(f)->flying) {
        s->cs.iasa_at = 10;
        if (s->current_tick > 0 && s->current_tick % 10 == 0) cs_enable_interrupt(f, s, true, false);
    }
    if ((s->current_tick > 1 || is_prev(o, "Fall")) && o->chara.grounded) return by_name(f, "Landing");
    return r;
}
const state_vt_t vt_characters_robo_states_Fall = { .enter = rf_enter, .tick = rf_tick };

/* ---------- Taunt (extends Taunt) ---------- */
static sid rt_enter(obj_t *o, state_t *s)
{
    s->cs.next_state_on_hold = false; s->cs.next_state_on_hold_on_opponent_turn = false;
    s->d.anim_length = 45;
    if (!o->chara.grounded) { s->d.anim_length = 11; s->cs.next_state_on_hold = true; s->cs.next_state_on_hold_on_opponent_turn = true; }
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_Taunt = { .enter = rt_enter, .frame = taunt_frame };

/* ---------- estados menores ---------- */
static sid aft_enter(obj_t *o, state_t *s) { (void)s; (void)o; return SID_NONE; }
static sid aft_enter2(obj_t *o, state_t *s)
{
    th_enter(o, s);
    s->ustate[7] = F(o)->combo_count != 0;
    return aft_enter(o, s);
}
const state_vt_t vt_characters_robo_states_AirForwardThrow = { .enter = aft_enter2, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit };

static sid ags_enter_shared(obj_t *o, state_t *s)
{
    fighter_t *f = F(o); robo_t *r = ROBO(f);
    r->kill_process_super_level = f->supers_available;
    if (r->kill_process_super_level == 9) r->kill_process_super_level = f->combo_count <= 0 ? 200 : 11;
    sid res = cs_enter_shared(o, s);
    for (int i = f->supers_available; i > 0; i--) fighter_use_super_bar(f);
    return res;
}
static sid ags_frame(obj_t *o, state_t *s, int t)
{
    fighter_t *f = F(o); phys_t *c = &o->chara; (void)s;
    if (t == 0) { ROBO(f)->flying = false; ROBO(f)->fly_ticks_left = 0; }
    else if (t == 1) { if (f->initiative) o->invulnerable = true; o->projectile_invulnerable = true; }
    else if (t == 9) o->invulnerable = false;
    else if (t == 14) { c->vx = fx_mul(c->vx, FXC(0.5)); c->vy = fx_mul(c->vy, FXC(0.5)); }
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_AirGrabSuper = { .enter_shared = ags_enter_shared, .frame = ags_frame };

static sid cg_enter(obj_t *o, state_t *s) { (void)o; s->ustate[0] = s->data ? *(const int *)s->data : 0; return SID_NONE; }
static sid cg_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t == 1) phys_move_rel(&o->chara, fxi(5), 0);
    else if (t == 2) o->throw_invulnerable = true;
    else if (t == 7) o->throw_invulnerable = false;
    return SID_NONE;
}
static void cg_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)s; (void)victim; (void)hb; fighter_t *f = F(o);
    if (f->combo_count <= 0) { f->combo_proration = 12; f->combo_proration_set = true; }
}
const state_vt_t vt_characters_robo_states_CommandGrab = { .enter = cg_enter, .frame = cg_frame, .hit_something = cg_hit };

static sid dg_frame(obj_t *o, state_t *s, int t)
{
    if (t != 0) return SID_NONE;
    bool reverse = s->data && *(const int *)s->data;
    sid target = by_name(F(o), reverse ? "TryCatchGroundSlam" : "DisembowelGrabFollowup");
    for (int i = 0; i < s->d.nhb && i < 4; i++) {
        hitbox_t *h = &s->hbr[i];
        if (h->def->is_throwbox) h->throw_state = target; else h->followup_state = target;
    }
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_DisembowelGrab = { .frame = dg_frame };

static sid eqt_frame(obj_t *o, state_t *s, int t) { (void)s; if (t == 0) ROBO(F(o))->used_earthquake_grab = true; return SID_NONE; }
const state_vt_t vt_characters_robo_states_EarthquakeThrow = { .enter = th_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit, .frame = eqt_frame };

static sid fdc_tick(obj_t *o, state_t *s) { if (s->current_tick == 3 && F(o)->turn_frames < 10) s->current_tick = 2; return SID_NONE; }
const state_vt_t vt_characters_robo_states_FastDriveCancel = { .tick = fdc_tick };

static bool ft_usable(fighter_t *f, state_t *s, bool base) { (void)s; return base && ROBO(f)->can_flamethrower; }
static void ft_process(obj_t *o, state_t *s, obj_t *pr) { (void)s; (void)pr; ROBO(F(o))->can_flamethrower = false; }
const state_vt_t vt_characters_robo_states_Flamethrower = { .usable = ft_usable, .process_projectile = ft_process };

static sid lar_frame(obj_t *o, state_t *s, int t)
{
    (void)s;
    if (t == 4 && F(o)->initiative) o->aerial_attack_immune = true; else if (t == 11) o->aerial_attack_immune = false;
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_Lariat = { .frame = lar_frame };

/* ---------- Grenade: data xy ---------- */
static sid gr_enter(obj_t *o, state_t *s)
{
    (void)o; const xy_data_t *d = (const xy_data_t *)s->data;
    s->d.projectile_pos_y = (d && d->y > 0) ? 16 : -32;
    return SID_NONE;
}
static void gr_process(obj_t *o, state_t *s, obj_t *pr)
{
    const xy_data_t *d = (const xy_data_t *)s->data;
    fxv force = obj_xy_to_dir(fxi(d ? d->x : 0), fxi(d ? d->y : 0), FXC(10.5));
    pr->chara.grounded = false;
    phys_force(&pr->chara, force.x, force.y);
    ROBO(F(o))->grenade_uid = pr->uid;
}
static bool gr_usable(fighter_t *f, state_t *s, bool base) { (void)s; return base && ROBO(f)->grenade_uid < 0; }
const state_vt_t vt_characters_robo_Grenade = { .enter = gr_enter, .process_projectile = gr_process, .usable = gr_usable };

/* ---------- Missile (3 misseis): ustate 0 missiles_left ---------- */
static bool ms_arc(state_t *s) { const xy_data_t *d = (const xy_data_t *)s->data; return d && d->y < 0; }
static bool ms_up(state_t *s) { const xy_data_t *d = (const xy_data_t *)s->data; return ms_arc(s) && d->x == 0; }
static sid ms_enter(obj_t *o, state_t *s) { (void)o; s->ustate[0] = 3; return SID_NONE; }
static void ms_pos(obj_t *o, state_t *s, int *x, int *y) { (void)o; if (ms_up(s)) { *x = -5; *y = s->d.projectile_pos_y - 20; } }
static bool ms_force(obj_t *o, state_t *s)
{
    if (!ms_up(s)) return false;
    if (s->d.force_speed != 0) phys_force_rel(&o->chara, 0, s->d.force_speed);
    return true;
}
static const void *ms_scene(obj_t *o, state_t *s) { (void)o; return ms_arc(s) ? xp_ptr(&s->d, "arc_projectile_scene") : NULL; }
static void ms_process(obj_t *o, state_t *s, obj_t *pr)
{
    int idx = 3 - s->ustate[0];
    PROJ(pr)->ix = idx; PROJ(pr)->iy = ms_up(s); PROJ(pr)->idata = o->chara.facing;
    if (--s->ustate[0] > 0) s->current_tick = 1;
}
const state_vt_t vt_characters_robo_Missile = { .enter = ms_enter, .projectile_pos = ms_pos, .timed_force = ms_force, .projectile_scene = ms_scene, .process_projectile = ms_process };

/* ---------- SuperJump (extends Jump) ---------- */
static sid sj_frame(obj_t *o, state_t *s, int t)
{
    sid r = jump_frame(o, s, t);
    fighter_t *f = F(o);
    if (t == 7) f->has_projectile_armor = true;
    else if (t == 8) f->air_movements_left = 3;
    else if (t == 38) f->has_projectile_armor = false;
    return r;
}
const state_vt_t vt_characters_robo_SuperJump = { .enter = jump_enter, .frame = sj_frame, .tick = jump_tick };
const state_vt_t vt_characters_robo_states_AirGrab = { .enter = grab_enter, .frame = grab_frame, .tick = grab_tick };

static sid gp_frame(obj_t *o, state_t *s, int t)
{
    if (t == 0 && s->d.nhb > 3) s->hbr[3].active_ticks = ROBO(F(o))->used_earthquake_grab ? 0 : 3;
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_GroundPound = { .frame = gp_frame };
const state_vt_t vt_characters_robo_states_ClapLand = { 0 };

/* ---------- CornerCarry ---------- */
static sid ccg_enter(obj_t *o, state_t *s) { (void)s; fighter_t *f = F(o); if (f->reverse_state && f->combo_count <= 0) fighter_add_penalty(f, 25, true); return SID_NONE; }
static sid ccg_frame(obj_t *o, state_t *s, int t)
{
    (void)s; fighter_t *f = F(o); robo_t *r = ROBO(f);
    if (t == 0 && r->flying) r->fly_x = 0;
    else if (t == 15) f->has_projectile_armor = false;
    return SID_NONE;
}
static sid ccg_tick(obj_t *o, state_t *s)
{
    phys_apply_forces_no_limit(&o->chara);
    if (s->current_tick < 15) phys_force_rel(&o->chara, FXC(1.5), 0);
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_CornerCarryGrab = { .enter = ccg_enter, .frame = ccg_frame, .tick = ccg_tick };

static sid cch_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o); phys_t *c = &o->chara;
    phys_force_rel(c, fxi(2), 0);
    phys_apply_forces_no_limit(c);
    if (s->current_tick > 240 || (f->combo_count > 0 && s->current_tick > 20)) return by_name(f, "CornerCarryRelease");
    if (abs(phys_x(c) - f->stage_width * c->facing) < 64) {
        phys_set_pos(c, fxi(f->stage_width * c->facing - 64 * c->facing), fxi(phys_y(c)));
        return by_name(f, "CornerCarryRelease");
    }
    if (f->penalty > 0) f->penalty = 0;
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_CornerCarryHold = { .enter = th_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit, .tick = cch_tick };
const state_vt_t vt_characters_robo_states_CornerCarryRelease = { .enter = th_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit };

static sid ofg_frame(obj_t *o, state_t *s, int t)
{
    (void)o; if (t == 0) s->cs.update_facing_on_exit = false; else if (t == 8) s->cs.update_facing_on_exit = true;
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_OverflowGrab = { .frame = ofg_frame };
static sid ofu_frame(obj_t *o, state_t *s, int t)
{
    (void)s; fighter_t *f = F(o);
    if (t == 0) { ROBO(f)->flying = false; if (f->air_movements_left < f->num_air_movements) f->air_movements_left++; }
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_OverflowFollowup = { .enter = th_enter, .frame_shared = th_frame_shared, .tick_shared = th_tick_shared, .tick_after = th_tick_after, .exit = th_exit, .frame = ofu_frame };

static sid ret_frame(obj_t *o, state_t *s, int t) { (void)s; if (t == 3) phys_move_rel(&o->chara, fxi(-12), 0); return SID_NONE; }
static sid ret_tick(obj_t *o, state_t *s) { (void)s; obj_apply_forces(o); return SID_NONE; }
const state_vt_t vt_characters_robo_states_Return = { .frame = ret_frame, .tick = ret_tick };

static void sb_hit(obj_t *o, state_t *s, obj_t *victim, hitbox_t *hb)
{
    (void)s; (void)victim; (void)hb;
    fxv f = fxv_norm_times(fxv_norm(fxv_make(-FX_ONE, FX_ONE)), fxi(6));
    phys_force_rel(&o->chara, f.x, f.y);
}
const state_vt_t vt_characters_robo_states_SawBlade = { .hit_something = sb_hit };

static sid sk_tick(obj_t *o, state_t *s)
{
    if (s->current_tick == 3) { s->cs.self_hit_cancellable = F(o)->initiative; if (F(o)->initiative) s->current_tick = 5; }
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_SideKick = { .tick = sk_tick };

/* ---------- Vacuum (states/Missile.gd): WindBox puxa o oponente ---------- */
static sid vac_tick(obj_t *o, state_t *s)
{
    fighter_t *f = F(o);
    if (s->current_tick <= 15) {
        box_t wb = { 143, -23, 132, 20, phys_x(&o->chara), phys_y(&o->chara), o->chara.facing };
        if (box_overlaps(&f->opponent->o.hurtbox, &wb)) {
            fx fxv_ = fx_mul(fxi(fighter_opponent_dir(f->opponent)), FXC(0.45));
            phys_force(&f->opponent->o.chara, fxv_, 0);
        }
    }
    return SID_NONE;
}
const state_vt_t vt_characters_robo_states_Missile = { .tick = vac_tick };
