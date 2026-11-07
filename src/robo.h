/* Robo (Robot.gd): estado privado do personagem. */
#ifndef ROBO_H
#define ROBO_H
#include "fighter.h"
#include "projectile.h"

typedef struct {
    bool loic_draining, landed_move, fly_fx_started, start_fly, can_ground_pound, can_gain_ground_pound, buffer_reset_ground_pound;
    bool orbital_strike_out, can_loic, got_hit, armor_active, buffer_armor, can_unlock_gratuitous, can_flamethrower;
    bool nade_explosion_in_combo, hit_with_loic, magnet_installed, honking, used_earthquake_grab, started_magnet_in_initiative;
    bool force_fly, magnetize_opponent, magnetize_opponent_blocked, drive_cancel, buffer_drive_cancel, super_armor_installed;
    bool flying;                                  /* flying_dir != null */
    int fly_x, fly_y;
    int armor_pips, fly_ticks_left, kill_process_super_level, loic_meter, magnet_ticks_left, honking_cooldown, armor_startup_ticks;
    int step_count, loic_dir, propel_friction_ticks;
    int grenade_uid, orbital_uid, flame_touching_uid;      /* uid ou -1 */
    fx magnetize_bomb_strength;
} robo_t;

#define ROBO(f) ((robo_t *)(void *)(f)->ch)
extern const fighter_vt_t vt_robo;

/* bits de extras (action_extra_t.cx_has/cx_val): cx_dir = fly_dir, cx_dir2.x = loic_dir, cx_dir3 = bounce */
enum { RX_FLY_DIR = 1 << 0, RX_FLY_ENABLED = 1 << 1, RX_FORCE_FLY = 1 << 2, RX_ARMOR = 1 << 3, RX_NADE = 1 << 4, RX_BOUNCE = 1 << 5,
       RX_PULL = 1 << 6, RX_DRIVE_CANCEL = 1 << 7, RX_HONK = 1 << 8, RX_LOIC_DIR = 1 << 9 };

/* dados de acao */
typedef struct { xy_data_t dir; int autoo; } clap_data_t;          /* ClapJump {Direction, Auto} */
typedef struct { xy_data_t start, end; } saw_data_t;                /* Chainsaw {Start, End} */
typedef struct { int self_, has_dir; xy_data_t dir, delay; } loic_data_t;   /* LOIC {Self, Dir?, Delay} */
#define ROBO_AIR_BAR_MAX 1000
void robo_stop_flying(fighter_t *f);
void robo_start_magnetizing(fighter_t *f);
void robo_add_armor_pip(fighter_t *f);
/* implementados em proj_robo.c (fracos aqui) */
void robo_grenade_refresh(obj_t *g);
void robo_grenade_activate(obj_t *g);
bool robo_grenade_active(obj_t *g);
void robo_orbital_deactivate(obj_t *o);
bool robo_orbital_active(obj_t *o);
bool robo_orbital_firing(obj_t *o);
bool robo_orbital_deactivating(obj_t *o);
#endif
