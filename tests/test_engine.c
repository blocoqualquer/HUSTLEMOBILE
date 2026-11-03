/* Teste de sanidade do nucleo: maquina de estados, fallback por anim_length, tick_shared e fisica por estado. */
#include <stdio.h>
#include <assert.h>
#include "../src/engine.h"

static int enter_count;
static sid wait_tick(obj_t *o, state_t *s) { (void)o; return s->current_tick == 5 ? 2 : SID_NONE; }   /* Wait -> Run no tick 5 */
static sid run_enter(obj_t *o, state_t *s) { (void)o; (void)s; enter_count++; return SID_NONE; }
static sid jump_frame(obj_t *o, state_t *s, int t) { (void)s; if (t == 2) phys_force(&o->chara, FX_INT(0), FX_INT(-6)); return SID_NONE; }
static const state_vt_t vt_wait = { .tick = wait_tick }, vt_run = { .enter = run_enter }, vt_jump = { .frame = jump_frame };

static state_def_t defs[4] = {
  { .name="Wait", .vt=&vt_wait, .anim_length=10, .fallback_state=0, .apply_fric=1, .loop_animation=1, .ticks_per_frame=2, .sprite_anim=0, .sprite_anim_length=4, .sfx_tick=-1, .force_tick=-1, .projectile_tick=-1, .timed_spawn_particle_tick=-1, .state_screenshake_tick=-1, .flip_frame=-1, .refresh_loop=1 },
  { .name="Jump", .vt=&vt_jump, .anim_length=20, .fallback_state=0, .apply_grav=1, .apply_forces=1, .sfx_tick=-1, .force_tick=-1, .projectile_tick=-1, .timed_spawn_particle_tick=-1, .state_screenshake_tick=-1, .flip_frame=-1, .ticks_per_frame=1, .sprite_anim=-1 },
  { .name="Run", .vt=&vt_run, .anim_length=3, .fallback_state=1, .sfx_tick=-1, .force_tick=-1, .projectile_tick=-1, .timed_spawn_particle_tick=-1, .state_screenshake_tick=-1, .flip_frame=-1, .ticks_per_frame=1, .sprite_anim=-1 },
};
int main(void)
{
    static state_t st[3]; static obj_t o;
    for (int i = 0; i < 3; i++) st[i].def = &defs[i];
    phys_init(&o.chara); o.chara.grav = fx_parse("0.8"); o.chara.max_fall = FX_INT(15); o.chara.max_aspeed = FX_INT(10); o.chara.max_gspeed = FX_INT(15);
    o.gravity_enabled = true; o.can_update_sprite = true;
    obj_init(&o, st, 3, 0);
    assert(obj_state(&o)->def == &defs[0] && obj_state(&o)->current_tick == -1);
    obj_tick(&o); assert(obj_state(&o)->current_tick == 0 && o.sprite_frame == 0);
    for (int i = 0; i < 5; i++) obj_tick(&o);              /* ticks 1..5: tick 5 pede Run */
    assert(obj_state(&o)->current_tick == 5);
    obj_tick(&o);                                          /* troca para Run (enter_count=1) e ja roda o tick 0 */
    assert(obj_state(&o)->def == &defs[2] && enter_count == 1 && obj_state(&o)->current_tick == 0);
    for (int i = 0; i < 3; i++) obj_tick(&o);              /* Run acaba no tick 3 -> fallback Jump */
    obj_tick(&o);
    assert(obj_state(&o)->def == &defs[1]);
    int up = 0, landed = 0;
    for (int i = 0; i < 40; i++) {
        obj_tick(&o);
        if (o.chara.py < 0) up = 1;
        if (up && obj_state(&o)->def == &defs[1] && obj_state(&o)->current_tick >= 17 && o.chara.grounded && o.chara.py == 0) landed = 1;
    }
    assert(up);        /* o frame 2 do Jump aplicou a forca para cima */
    assert(landed);    /* e a gravidade trouxe de volta ao chao antes do fim da animacao */
    printf("engine ok (estado=%s, game_tick=%d)\n", obj_state(&o)->def->name, o.game_tick);
    return 0;
}
