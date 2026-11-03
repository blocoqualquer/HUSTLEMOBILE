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
static int test_hitbox(void)
{
    /* estado "Attack": hitbox ativa do tick 3 ao 4 (start_tick=3, active_ticks=2); hurtbox muda no tick 2 por 2 ticks */
    static const hitbox_def_t hb = { .x = 10, .y = -10, .w = 8, .h = 8, .activated = 1, .damage = 5, .hitstun_ticks = 20, .combo_hitstun_ticks = -1,
                                     .hitlag_ticks = 4, .victim_hitlag = -1, .combo_victim_hitlag = -1, .start_tick = 3, .active_ticks = 2, .hits_vs_standing = 1 };
    static const hurtstate_def_t hs = { .x = 0, .y = -5, .w = 4, .h = 4, .start_tick = 2, .active_ticks = 2 };
    static state_def_t ad = { .name = "Attack", .anim_length = 8, .fallback_state = 0, .sfx_tick = -1, .force_tick = -1, .projectile_tick = -1,
                              .timed_spawn_particle_tick = -1, .state_screenshake_tick = -1, .flip_frame = -1, .ticks_per_frame = 1, .sprite_anim = -1,
                              .hb = &hb, .nhb = 1, .hurt = &hs, .nhurt = 1 };
    static state_t st[1]; static obj_t a;
    st[0].def = &ad;
    phys_init(&a.chara); a.gravity_enabled = true; a.can_update_sprite = true;
    a.hurtbox = (box_t){ 0, -20, 10, 20, 0, 0, 1 };
    obj_init(&a, st, 1, 0);
    int active_ticks_seen = 0, first_active = -1, hurt_small = 0;
    for (int t = 0; t < 7; t++) {
        obj_tick(&a);
        if (st[0].hbr[0].active) { active_ticks_seen++; if (first_active < 0) first_active = st[0].current_tick; }
        if (a.hurtbox.w == 4) hurt_small++;
    }
    assert(first_active == 2);        /* ativa quando current_tick+1 == start_tick (ao rodar o tick 2) e ja conta o primeiro tick */
    assert(active_ticks_seen == 2);   /* active_ticks=2 -> ativa no tick 3 e 4, depois desativa */
    assert(hurt_small == 3);          /* HurtboxState: comeca no tick 2, tick() so encerra quando contador > active_ticks (2) => 3 ticks */
    assert(a.hurtbox.w == 10);        /* e a hurtbox volta ao normal */
    /* colisao: oponente em x=12 */
    static obj_t b; phys_init(&b.chara); b.hurtbox = (box_t){ 0, -20, 10, 20, 12, 0, 1 }; b.sm.cur = NULL;
    st[0].hbr[0].box.pos_x = 0; st[0].hbr[0].box.pos_y = 0; st[0].hbr[0].box.facing = 1;
    assert(hitbox_overlaps(&st[0].hbr[0], &b.hurtbox));
    b.hurtbox.pos_x = 60; assert(!hitbox_overlaps(&st[0].hbr[0], &b.hurtbox));
    return 0;
}

int main(void)
{
    test_hitbox();
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
