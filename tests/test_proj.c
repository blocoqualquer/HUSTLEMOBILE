/* Projetil: Kunai (DirProjectileDefault) acerta o oponente; bola de fogo (ShurikenDefault) segue dir*speed. */
#include "../src/projectile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern const chardata_t chardata_ninja, chardata_kunai, chardata_sfireball;
static fighter_t A, B;
static state_t SA[200], SB[200];
static void setup(fighter_t *f, fighter_t *op, state_t *st, int id, int x)
{
    const chardata_t *cd = &chardata_ninja;
    memset(f, 0, sizeof *f);
    for (int i = 0; i < cd->nstates; i++) st[i].def = &cd->states[i];
    f->names = cd->names; f->nnames = cd->nnames; f->cd = cd;
    f->o.hurtbox = cd->hurtbox; f->o.collision_box = cd->collision_box;
    phys_init(&f->o.chara);
    f->o.chara.px = FX_INT(x); f->o.chara.facing = id == 1 ? 1 : -1;
    f->o.gravity_enabled = true; f->o.id = id; f->o.uid = id;
    fighter_init(f, op, st, cd->nstates, fighter_name_id(f, "Wait"), id);
    f->o.chara.px = FX_INT(x); f->o.chara.grounded = true;
    obj_update_boxes(&f->o);
}
int main(void)
{
    setup(&A, &B, SA, 1, -100); setup(&B, &A, SB, 2, 40);
    game_t g; game_init(&g, &A, &B);
    for (int t = 0; t < 3; t++) game_tick(&g);
    struct { fxv dir; } dd = { fxv_make(FX_ONE, 0) };
    proj_t *k = proj_spawn(&g, &chardata_kunai, &A.o, 20, -16, true, &dd, sizeof dd, true);
    assert(k && g.nobjs == 1);
    int hit_tick = -1, maxdx = 0;
    int hp0 = B.hp;
    for (int t = 0; t < 120; t++) {
        game_tick(&g);
        if (getenv("DBG")) fprintf(stderr, "[hl=%d bs=%d] t%d kunai x=%d y=%d disabled=%d | B hp=%d %s\n", B.o.hitlag_ticks, B.blockstun_ticks, g.current_tick, phys_x(&k->o.chara), phys_y(&k->o.chara), k->o.disabled, B.hp, B.o.sm.cur->d.name);
        if (hit_tick < 0 && B.hp < hp0) hit_tick = g.current_tick;
        if (phys_x(&k->o.chara) > maxdx) maxdx = phys_x(&k->o.chara);
    }
    printf("kunai: acertou no tick %d, hp B=%d, x max=%d, disabled=%d, estado B=%s\n", hit_tick, B.hp, maxdx, k->o.disabled, B.o.sm.cur->d.name);
    assert(hit_tick > 0 && B.hp < hp0);
    assert(k->o.disabled);               /* clash=0: atravessa e some no limite do palco */
    assert(B.hp == hp0 - 70);            /* so um acerto (hit_objects) */
    /* bola de fogo do Ninja (ShurikenDefault): move_x=9/tick sem speed_modifier; dano 60 e some ao acertar (clash) */
    int hp1 = B.hp;
    for (int i = 0; i < 40; i++) game_tick(&g);        /* B volta ao neutro */
    proj_t *fb = proj_spawn(&g, &chardata_sfireball, &A.o, 20, -16, true, NULL, 0, true);
    assert(fb);
    int hp2 = B.hp;
    for (int i = 0; i < 40; i++) game_tick(&g);
    printf("fireball: hp %d -> %d, disabled=%d x=%d\n", hp2, B.hp, fb->o.disabled, phys_x(&fb->o.chara));
    assert(B.hp < hp2 && fb->o.disabled);
    (void)hp1;
    puts("proj ok");
    return 0;
}
