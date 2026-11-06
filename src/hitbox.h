/* CollisionBox / Hitbox / HurtboxState / LimbHurtbox (ver CollisionBox.gd, Hitbox.gd, HurtboxState.gd, LimbHurtbox.gd).
 * Caixas sao AABB inteiras (x,y relativos ao objeto, w/h meia-largura/meia-altura), espelhadas por facing. */
#ifndef HITBOX_H
#define HITBOX_H
#include "fx.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct { int x, y, w, h; int pos_x, pos_y; int facing; } box_t;     /* facing: +1 direita, -1 esquerda */
typedef struct { int x1, x2, y1, y2; } aabb_t;

static inline int box_x_facing(const box_t *b) { return b->facing >= 0 ? b->x : -b->x; }
static inline aabb_t box_aabb(const box_t *b)
{
    int x = box_x_facing(b);
    aabb_t a = { x - b->w + b->pos_x, x + b->w + b->pos_x, b->y - b->h + b->pos_y, b->y + b->h + b->pos_y };
    return a;
}
static inline void box_update_position(box_t *b, int x, int y) { b->pos_x = x; b->pos_y = y; }
static inline bool box_overlaps(const box_t *a, const box_t *b)
{
    if (a->w == 0 && a->h == 0) return false;
    if (b->w == 0 && b->h == 0) return false;
    aabb_t p = box_aabb(a), q = box_aabb(b);
    return !(p.x1 > q.x2 || p.x2 < q.x1 || p.y1 > q.y2 || p.y2 < q.y1);
}
static inline int box_center_x(const box_t *b) { aabb_t a = box_aabb(b); return (a.x1 + a.x2) / 2; }   /* '/' de int do Godot trunca */
static inline int box_center_y(const box_t *b) { aabb_t a = box_aabb(b); return (a.y1 + a.y2) / 2; }

enum { HB_NORMAL, HB_FLIP, HB_THROWHIT, HB_OFFENSIVE_BURST, HB_BURST, HB_NOHITSTUN, HB_DETECT };
enum { HEIGHT_HIGH, HEIGHT_MID, HEIGHT_LOW };

/* exports de Hitbox.gd + ThrowBox.gd. Preenchido por tools/gen_c_data.py (inicializadores designados). */
typedef struct {
    int x, y, w, h;
    uint8_t is_throwbox, activated;
    int damage, damage_in_combo, minimum_damage; fx chip_damage_modifier;
    int hitbox_type, hitstun_ticks, combo_hitstun_ticks, hitlag_ticks, victim_hitlag, combo_victim_hitlag, damage_proration;
    uint8_t scale_combo; int combo_scaling_amount;
    uint8_t cancellable, increment_combo, hits_otg, hits_vs_standing, hits_vs_grounded, hits_vs_aerial, can_counter_hit;
    fx di_modifier, sdi_modifier, meter_gain_modifier; int parry_meter_gain;
    uint8_t ignore_armor; int followup_state; uint8_t force_grounded, can_clash, hits_vs_dizzy, beats_grab, hits_projectiles;
    uint8_t ignore_projectile_armor, allowed_to_hit_own_team, guard_break; int guard_break_proration;
    uint8_t block_punishable, parriable, block_cancel_allowed; int plus_frames, hit_height, priority, group, misc;
    int screenshake_amount, screenshake_frames; uint8_t bump_on_whiff, rumble, bass_on_whiff;
    fx dir_x, dir_y, knockback; uint8_t launch_reversible, vacuum, send_away_from_center;
    fx block_pushback_modifier; uint8_t block_pushback_reversible; fx block_reverse_pushback_modifier, pushback_x;
    int grounded_hit_state, aerial_hit_state, minimum_grounded_frames;                 /* sid, -1 = nenhum */
    uint8_t knockdown, knockdown_extends_hitstun, hard_knockdown, disable_collision, air_ground_bounce, ground_bounce;
    fx ground_bounce_knockback_modifier; uint8_t wall_slam;
    int start_tick, active_ticks; uint8_t always_on, looping; int loop_active_ticks, loop_inactive_ticks;
    int throw_state;                                                                    /* sid do estado do alvo (ThrowBox) */
    int sfx_hit, sfx_whiff, sfx_bass;                                                   /* ids de som (ou -1) */
    int hit_particle, whiff_particle;                                                   /* ids de efeito (ou -1) */
} hitbox_def_t;

typedef struct { int x, y, w, h, start_tick, active_ticks; uint8_t endless; } hurtstate_def_t;   /* HurtboxState e LimbHurtbox */

#define HB_MAX_HIT_OBJECTS 6
typedef struct obj_s obj_t;
typedef struct hitbox_s {
    const hitbox_def_t *def;
    box_t box;
    int damage, hitstun_ticks, combo_hitstun_ticks, victim_hitlag, combo_victim_hitlag, priority;   /* mutaveis em runtime pelos scripts */
    uint8_t cancellable, hit_cancellable_dummy, block_cancel_allowed, block_punishable;   /* mutaveis em runtime */
    fx knockback, dir_x, dir_y; int hit_height;           /* mutaveis em runtime (Stackriken, Caltrops) */
    bool active, enabled, played_whiff_sound;
    int tick;
    int hit_objects[HB_MAX_HIT_OBJECTS]; int nhit;       /* uid dos objetos ja atingidos */
    struct hitbox_s **group; int ngroup;                 /* hitboxes do mesmo grupo no estado (inclui este) */
    obj_t *host;
    uint8_t hits_vs_grounded;         /* mutavel em runtime (DiveKick) */
    int throw_state;                  /* copia mutavel (Grab troca o estado de arremesso em runtime) */
} hitbox_t;

void hitbox_init(hitbox_t *h, const hitbox_def_t *d, obj_t *host);
void hitbox_activate(hitbox_t *h);
void hitbox_deactivate(hitbox_t *h);
void hitbox_tick(hitbox_t *h);
bool hitbox_already_hit(const hitbox_t *h, int uid);          /* em qualquer hitbox do grupo */
void hitbox_save_hit(hitbox_t *h, int uid);                   /* grava em todo o grupo */
void hitbox_reset_hit_objects(hitbox_t *h);
bool hitbox_overlaps(const hitbox_t *h, const box_t *b);

#endif
