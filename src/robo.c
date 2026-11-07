/* Robo: Robot.gd (hooks do lutador) + scripts de estados em states_robo.c */
#include "robo.h"
#include "states.h"
#include <string.h>
#include <stdlib.h>

_Static_assert(sizeof(robo_t) <= FIGHTER_CH_SIZE, "robo_t excede a area privada do lutador");

#define FLY_FORCE FXC(1.65)
#define MAX_FWD_FLIGHT FXC(9)
#define MAX_BACK_FLIGHT FXC(5)
#define MAX_FLY_UP_SPEED FXC(-6)
#define MAGNET_MIN_BOMB_DIST 64
#define MAGNET_MIN_TICKS 10
#define FLY_TICKS 25
#define GROUND_POUND_MIN_HEIGHT (-48)
#define LOIC_METER 1000
#define START_LOIC_METER 500
#define LOIC_GAIN 5
#define MAGNET_TICKS 100
#define MAGNET_STRENGTH FXC(1.6)
#define MAGNET_BOMB_STRENGTH FXC(1.0)
#define BOUNCE_SPEED 10
#define ARMOR_STARTUP_TICKS 3
#define WC_EXTRA_ARMOR_STARTUP_TICKS 2
#define MAGNETIZE_OPP_BLOCKED_MOD FXC(0.75)
#define MAGNETIZE_BOMB_INC FXC(0.1)
#define DRAIN_AIR_BAR 20
#define GAIN_AIR_BAR 3
#define GAIN_AIR_BAR_ON_HIT 300
#define MIN_AIR_BAR 80
#define FLY_GRAV FXC(0.05)
#define FLY_MAX_FALL FXC(100.0)
#define HONK_COOLDOWN 30

static inline fx fxi(int v) { return FX_INT(v); }
#define OBJ_OF(uid) ((uid) >= 0 ? game_obj_by_uid(eng_game_get(), (uid)) : NULL)

__attribute__((weak)) void robo_grenade_refresh(obj_t *g) { (void)g; }
__attribute__((weak)) void robo_grenade_activate(obj_t *g) { (void)g; }
__attribute__((weak)) bool robo_grenade_active(obj_t *g) { (void)g; return false; }
__attribute__((weak)) void robo_orbital_deactivate(obj_t *o) { (void)o; }
__attribute__((weak)) bool robo_orbital_active(obj_t *o) { (void)o; return false; }
__attribute__((weak)) bool robo_orbital_firing(obj_t *o) { (void)o; return false; }
__attribute__((weak)) bool robo_orbital_deactivating(obj_t *o) { (void)o; return false; }

static void r_init(fighter_t *f)
{
    robo_t *r = ROBO(f);
    memset(r, 0, sizeof *r);
    r->can_gain_ground_pound = r->can_unlock_gratuitous = r->can_flamethrower = true;
    r->armor_pips = 1; r->loic_meter = START_LOIC_METER;
    r->grenade_uid = r->orbital_uid = r->flame_touching_uid = -1;
    f->num_air_movements = 0; f->lose_one_air_option_in_neutral = true;
    f->air_option_bar_max = ROBO_AIR_BAR_MAX; f->air_option_bar = ROBO_AIR_BAR_MAX;
    if (f->infinite_resources) { r->can_loic = true; r->loic_meter = LOIC_METER; }
}

void robo_stop_flying(fighter_t *f) { ROBO(f)->flying = false; ROBO(f)->fly_fx_started = false; }

void robo_add_armor_pip(fighter_t *f)
{
    robo_t *r = ROBO(f);
    r->armor_pips++;
    if (r->armor_pips > 1) r->armor_pips = 1;
}

void robo_start_magnetizing(fighter_t *f)
{
    robo_t *r = ROBO(f);
    r->magnet_ticks_left = MAGNET_TICKS;
    r->magnetize_bomb_strength = 0;
    obj_t *g = OBJ_OF(r->grenade_uid);
    if (g) robo_grenade_refresh(g);
    r->magnet_installed = false;
}

static void magnetize(fighter_t *f)
{
    robo_t *r = ROBO(f);
    phys_t *me = &f->o.chara;
    if (r->magnetize_opponent) {
        phys_t *op = &f->opponent->o.chara;
        fxv dir = fxv_norm(fxv_make(fxi(phys_x(op) - phys_x(me)), fxi(phys_y(op) - phys_y(me))));
        fx st = MAGNET_STRENGTH;
        if (r->magnetize_opponent_blocked) st = fx_mul(st, MAGNETIZE_OPP_BLOCKED_MOD);
        fxv force = fxv_mul(dir, -st);
        fx d = fx_vec_dist(fxi(phys_x(me)), fxi(phys_y(me)), fxi(phys_x(op)), fxi(phys_y(op)));
        if (fx_round(d) < MAGNET_MIN_BOMB_DIST && (MAGNET_TICKS - r->magnet_ticks_left) > MAGNET_MIN_TICKS) r->magnet_ticks_left = 1;
        phys_force(op, force.x, op->grounded ? force.y : 0);
    }
    obj_t *g = OBJ_OF(r->grenade_uid);
    if (g) {
        phys_t *gp = &g->chara;
        fxv dir = fxv_norm(fxv_make(fxi(phys_x(gp) - phys_x(me)), fxi(phys_y(gp) - phys_y(me))));
        fxv force = fxv_mul(dir, -fx_mul(MAGNET_BOMB_STRENGTH, r->magnetize_bomb_strength));
        fx d = fx_vec_dist(fxi(phys_x(me)), fxi(phys_y(me)), fxi(phys_x(gp)), fxi(phys_y(gp)));
        if (fx_round(d) < MAGNET_MIN_BOMB_DIST && (MAGNET_TICKS - r->magnet_ticks_left) > MAGNET_MIN_TICKS) r->magnet_ticks_left = 1;
        phys_force(gp, force.x, force.y);
    } else r->magnet_ticks_left = 1;
    r->magnetize_bomb_strength += MAGNETIZE_BOMB_INC;
    if (r->magnetize_bomb_strength > FX_ONE || r->magnetize_opponent) r->magnetize_bomb_strength = FX_ONE;
}

static void r_on_got_hit_by_fighter(fighter_t *f) { if (ROBO(f)->armor_active) ROBO(f)->got_hit = true; }
static void r_on_got_parried(fighter_t *f) { robo_stop_flying(f); ROBO(f)->can_ground_pound = false; }
static void r_on_got_hit(fighter_t *f) { fighter_gain_air_option_bar(f, GAIN_AIR_BAR_ON_HIT / (f->combo_count > 1 ? f->combo_count : 1)); }
static void r_on_launched(fighter_t *f)
{
    robo_t *r = ROBO(f);
    obj_t *o = OBJ_OF(r->orbital_uid);
    if (r->orbital_uid >= 0 && o) { robo_orbital_deactivate(o); r->orbital_strike_out = false; r->orbital_uid = -1; }
}
static bool r_has_armor(fighter_t *f) { robo_t *r = ROBO(f); return r->armor_active && r->super_armor_installed && !state_is_hurt(f->o.sm.cur); }
static bool r_has_autoblock_armor(fighter_t *f) { return ROBO(f)->armor_active && !state_is_hurt(f->o.sm.cur); }
static bool r_can_perfect_parry(fighter_t *f) { return !ROBO(f)->flying; }
static int r_meter_gain_modified(fighter_t *f, int amount)
{
    robo_t *r = ROBO(f);
    obj_t *p = OBJ_OF(r->orbital_uid);
    if (p && robo_orbital_firing(p)) return amount / 2;
    return amount;
}
static void r_on_incr_combo(fighter_t *f) { if (f->combo_count == 0) ROBO(f)->landed_move = true; }

static bool r_apply_grav(fighter_t *f)
{
    robo_t *r = ROBO(f);
    phys_t *c = &f->o.chara;
    if (!r->flying || (c->vy < 0 && r->fly_x != 0 && r->fly_y >= 0)) return false;
    phys_grav_custom(c, FLY_GRAV, FLY_MAX_FALL);
    return true;
}

static void r_process_extra(fighter_t *f, const action_extra_t *x)
{
    robo_t *r = ROBO(f);
    phys_t *c = &f->o.chara;
    if (!f->has_extra) return;
    if (x->cx_has & RX_HONK) r->honking = (x->cx_val & RX_HONK) != 0;
    r->force_fly = false;
    bool can_fly = !f->busy_interrupt;
    bool force_in = (x->cx_has & RX_FORCE_FLY) && (x->cx_val & RX_FORCE_FLY);
    if ((x->cx_has & RX_FLY_DIR) && (!c->grounded || force_in) && can_fly) {
        if ((x->cx_has & RX_FLY_ENABLED) && (x->cx_val & RX_FLY_ENABLED) && f->air_option_bar > 0) {
            int dx = (int)fx_round(x->cx_dir.x), dy = (int)fx_round(x->cx_dir.y);
            bool same = !r->flying || (r->fly_x == dx && r->fly_y == dy);
            if (!r->flying || !same) r->start_fly = true;
            r->flying = true; r->fly_x = dx; r->fly_y = dy;
            if (x->cx_has & RX_FORCE_FLY) r->force_fly = (x->cx_val & RX_FORCE_FLY) != 0;
        } else if ((x->cx_has & RX_FLY_ENABLED) && !(x->cx_val & RX_FLY_ENABLED)) {
            r->flying = false;
            if (r->fly_fx_started) r->fly_fx_started = false;
        }
    }
    if ((x->cx_has & RX_LOIC_DIR) && !f->busy_interrupt) r->loic_dir = (int)fx_round(x->cx_dir2.x);
    if ((x->cx_has & RX_ARMOR) && r->armor_pips > 0) {
        r->buffer_armor = (x->cx_val & RX_ARMOR) != 0;
        if (r->buffer_armor && strcmp(f->o.sm.cur->d.name, "WhiffInstantCancel") == 0) r->armor_startup_ticks += WC_EXTRA_ARMOR_STARTUP_TICKS;
    }
    if ((x->cx_has & RX_NADE) && (x->cx_val & RX_NADE) && r->grenade_uid >= 0) {
        obj_t *n = OBJ_OF(r->grenade_uid);
        if (n && !robo_grenade_active(n)) {
            robo_grenade_activate(n);
            if ((x->cx_has & RX_BOUNCE) && (x->cx_dir3.x != 0 || x->cx_dir3.y != 0)) {
                fxv force = obj_xy_to_dir(x->cx_dir3.x, x->cx_dir3.y, fxi(BOUNCE_SPEED));
                phys_reset_momentum(&n->chara);
                phys_force(&n->chara, force.x, force.y);
            }
        }
    }
    if ((x->cx_has & RX_PULL) && (x->cx_val & RX_PULL) && r->magnet_installed && !r->buffer_armor) robo_start_magnetizing(f);
    if (x->cx_has & RX_DRIVE_CANCEL) r->buffer_drive_cancel = (x->cx_val & RX_DRIVE_CANCEL) != 0;
}

extern const state_vt_t vt_characters_robo_Roll;

static void r_tick(fighter_t *f)
{
    robo_t *r = ROBO(f);
    phys_t *c = &f->o.chara;
    state_t *cs = f->o.sm.cur;
    if (r->propel_friction_ticks > 0 && --r->propel_friction_ticks == 0) c->gfric = f->cd->ground_friction;
    if (r->got_hit) {
        r->armor_pips = 0; r->got_hit = false; r->buffer_armor = false;
        if (r->armor_active) r->super_armor_installed = false;
        r->armor_active = false;
        r->armor_startup_ticks = 0;
    } else if (r->armor_startup_ticks > 0) {
        if (--r->armor_startup_ticks == 0) { r->armor_active = true; r->buffer_armor = false; }
    }
    if (cs->d.is_grab && f->feinting && r->armor_active) f->feinting = false;
    if (r->honking) { r->honking = false; r->honking_cooldown = HONK_COOLDOWN; }
    if (f->combo_count == 0) r->nade_explosion_in_combo = false;
    else if (r->honking_cooldown > 0) { r->honking_cooldown--; if (f->combo_count > 0) r->honking_cooldown = 0; }

    if (r->magnet_ticks_left > 0) { magnetize(f); r->magnet_ticks_left--; }
    else { r->magnetize_opponent = false; r->magnetize_opponent_blocked = false; }
    if (r->landed_move) {
        if (!state_is_hurt(cs)) robo_add_armor_pip(f);
        r->landed_move = false;
    }
    if (c->grounded && !r->force_fly) {
        r->flying = false; r->fly_fx_started = false;
        if (f->air_option_bar < MIN_AIR_BAR) f->air_option_bar = MIN_AIR_BAR;
        fighter_gain_air_option_bar(f, GAIN_AIR_BAR);
    }
    if (fighter_is_in_hurt_state(f, false)) robo_stop_flying(f);
    if (r->flying && (!xp_int(&cs->d, "can_fly", 1) || cs->d.vt == &vt_characters_robo_Roll)) r->flying = false;
    if (r->start_fly && r->flying) { r->fly_ticks_left = FLY_TICKS; r->fly_fx_started = true; r->start_fly = false; }

    if (r->flying && (!c->grounded || r->force_fly)) {
        fxv fly_force = fxv_norm_times(fxv_make(fxi(r->fly_x), fxi(r->fly_y)), FLY_FORCE);
        int odir = fighter_opponent_dir(f);
        if (r->force_fly && fly_force.y >= 0) fly_force.y = 0;
        fx up = fly_force.y, upmod = FX_ONE;
        fx ratio = fx_div(fxi(f->air_option_bar), fxi(f->air_option_bar_max));
        if (fly_force.y < 0) { upmod = ratio; if (upmod < FXC(0.3)) upmod = FXC(0.3); }
        if (r->fly_y < 0 && c->vy > 0) c->vy = 0;
        if (r->fly_y > 0 && c->vy < 0) c->vy = fx_mul(c->vy, FXC(0.75));
        phys_force(c, fly_force.x, fx_mul(up, fx_mul(upmod, FXC(0.4))));
        fx maxh = fx_sign(c->vx) != odir ? MAX_BACK_FLIGHT : MAX_FWD_FLIGHT;
        if (c->vy < MAX_FLY_UP_SPEED) c->vy = MAX_FLY_UP_SPEED;
        if (fx_abs(c->vx) > maxh && fx_sign(c->vx) == r->fly_x) c->vx = fx_mul(maxh, fxi(fx_sign(c->vx)));
        if (fx_vec_len(fly_force.x, fly_force.y) == 0) { c->vx = fx_mul(c->vx, FXC(0.90)); c->vy = fx_mul(c->vy, FXC(0.90)); }
        fighter_drain_air_option_bar(f, DRAIN_AIR_BAR);
        if (--r->fly_ticks_left <= 0) robo_stop_flying(f);
        else if (f->o.current_tick % 2 == 0) {
            if (r->fly_x != 0) { if (r->fly_x != odir) fighter_add_penalty(f, 1, false); }
            else if (r->fly_y == -1) fighter_add_penalty(f, 1, false);
        }
    }

    if (r->loic_meter < LOIC_METER && !r->loic_draining) {
        obj_t *ob = OBJ_OF(r->orbital_uid);
        bool gain = true;
        if (ob && robo_orbital_active(ob)) gain = false;
        if (f->opponent->combo_count > 0) gain = false;
        if (gain) r->loic_meter += LOIC_GAIN;
        if (f->infinite_resources) { r->loic_meter = LOIC_METER; r->can_loic = true; }
    }
    if (r->loic_meter >= LOIC_METER && f->supers_available > 1) { r->can_loic = true; r->loic_meter = LOIC_METER; }
    else r->can_loic = false;

    if (r->buffer_reset_ground_pound) { r->buffer_reset_ground_pound = false; r->can_ground_pound = false; }
    if (c->grounded && !cs->started_in_air) { r->buffer_reset_ground_pound = true; r->can_gain_ground_pound = true; }
    if (!r->can_ground_pound && r->can_gain_ground_pound && phys_y(c) < GROUND_POUND_MIN_HEIGHT && !fighter_is_in_hurt_state(f, false)) {
        r->can_ground_pound = true; r->can_gain_ground_pound = false;
    }
    if (r->buffer_drive_cancel) { r->buffer_drive_cancel = false; r->drive_cancel = true; }
    if (f->air_option_bar == 0 && r->flying) robo_stop_flying(f);
}

static void r_on_state_exited(fighter_t *f, state_t *s)
{
    robo_t *r = ROBO(f);
    (void)s;
    if (r->buffer_armor) { r->armor_startup_ticks += ARMOR_STARTUP_TICKS; r->armor_pips = 0; }
    else { if (r->armor_active) r->super_armor_installed = false; r->armor_active = false; }
}
static void r_on_state_interruptable(fighter_t *f)
{
    robo_t *r = ROBO(f);
    if (r->armor_active) r->super_armor_installed = false;
    r->armor_active = false;
}
static void r_on_attack_blocked(fighter_t *f) { fighter_gain_air_option_bar(f, GAIN_AIR_BAR_ON_HIT / 4); ROBO(f)->can_ground_pound = false; }
static void r_on_blocked_melee_attack(fighter_t *f) { robo_stop_flying(f); }
static void r_on_hit_something(fighter_t *f, obj_t *victim)
{
    if (victim->is_fighter) fighter_gain_air_option_bar(f, GAIN_AIR_BAR_ON_HIT / (f->combo_count > 1 ? f->combo_count : 1));
}
static void r_reset_combo(fighter_t *f) { ROBO(f)->used_earthquake_grab = false; }
static void r_on_use_burst(fighter_t *f) { f->air_option_bar = f->air_option_bar_max; }

const fighter_vt_t vt_robo = {
    .init = r_init, .tick = r_tick, .process_extra = r_process_extra, .apply_grav = r_apply_grav,
    .on_state_interruptable = r_on_state_interruptable, .on_got_parried = r_on_got_parried, .on_got_hit = r_on_got_hit,
    .reset_combo = r_reset_combo, .has_armor = r_has_armor, .has_autoblock_armor = r_has_autoblock_armor,
    .can_perfect_parry = r_can_perfect_parry, .on_got_hit_by_fighter = r_on_got_hit_by_fighter, .on_launched = r_on_launched,
    .on_state_exited = r_on_state_exited, .on_attack_blocked = r_on_attack_blocked, .on_blocked_melee_attack = r_on_blocked_melee_attack,
    .on_hit_something = r_on_hit_something, .meter_gain_modified = r_meter_gain_modified, .on_incr_combo = r_on_incr_combo,
    .on_use_burst = r_on_use_burst,
};
