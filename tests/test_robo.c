/* Robo com scripts: Step, Grenade, Missile, voo, ClapJump, Chainsaw. */
#include "../src/game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/robo.h"
extern const chardata_t chardata_robo;
static fighter_t A, B;
static state_t SA[200], SB[200];
static void setup(fighter_t *f, fighter_t *op, state_t *st, int id, int x)
{
    const chardata_t *cd = &chardata_robo;
    memset(f, 0, sizeof *f); f->vt = &vt_robo;
    for (int i = 0; i < cd->nstates; i++) st[i].def = &cd->states[i];
    f->names = cd->names; f->nnames = cd->nnames; f->cd = cd;
    f->o.hurtbox = cd->hurtbox; f->o.collision_box = cd->collision_box;
    phys_init(&f->o.chara);
    f->o.chara.px = FX_INT(x); f->o.chara.facing = id == 1 ? 1 : -1;
    f->o.gravity_enabled = true; f->o.id = id; f->o.uid = id;
    fighter_init(f, op, st, cd->nstates, fighter_name_id(f, "Wait"), id);
    f->o.chara.px = FX_INT(x); f->o.chara.grounded = true; f->sadness_enabled = true;
    obj_update_boxes(&f->o);
}
static game_t g;
static void reset(int ax, int bx) { setup(&A, &B, SA, 1, ax); setup(&B, &A, SB, 2, bx); game_init(&g, &A, &B); for (int t = 0; t < 3; t++) game_tick(&g); }
static void run(int n) { while (n--) game_tick(&g); }
static const char *nm(fighter_t *f) { return f->o.sm.cur->d.name; }

static void act(fighter_t *f, const char *n, void *data) { int s = fighter_name_id(f, n); assert(s >= 0); fighter_select_action(f, s, data, NULL); }
int main(void)
{
    xy_data_t fwd = { 100, 0 };
    /* Step */
    reset(-100, 100);
    int x0 = phys_x(&A.o.chara);
    act(&A, "Step", NULL); run(25);
    printf("step: x %d -> %d estado=%s\n", x0, phys_x(&A.o.chara), nm(&A));
    assert(phys_x(&A.o.chara) > x0 + 10);

    /* Grenade */
    reset(-100, 100);
    xy_data_t gd = { 100, -50 };
    act(&A, "Grenade", &gd); run(20);
    printf("grenade: uid=%d objs=%d\n", ROBO(&A)->grenade_uid, g.nobjs);
    for (int i = 0; i < g.nobjs; i++) printf("  obj %d uid=%d disabled=%d state=%s pos=%d,%d\n", i, g.objs[i]->uid, g.objs[i]->disabled, g.objs[i]->sm.cur ? g.objs[i]->sm.cur->d.name : "?", phys_x(&g.objs[i]->chara), phys_y(&g.objs[i]->chara));
    assert(ROBO(&A)->grenade_uid >= 0 && g.nobjs >= 1);

    /* Missile: 3 misseis */
    reset(-100, 100);
    act(&A, "Missile", &fwd);
    int mx = 0; for (int i = 0; i < 60; i++) { game_tick(&g); if (g.nobjs > mx) mx = g.nobjs; }
    printf("missile: max objs=%d\n", mx);
    assert(mx >= 3);

    /* voo: sobe com extra FLY */
    reset(-100, 100);
    A.o.chara.grounded = false; A.o.chara.py = FX_INT(-40);
    action_extra_t ex; memset(&ex, 0, sizeof ex);
    ex.cx_has = RX_FLY_DIR | RX_FLY_ENABLED; ex.cx_val = RX_FLY_ENABLED; ex.cx_dir = fxv_make(0, FX_INT(-1));
    { int s = fighter_name_id(&A, "Fall"); fighter_select_action(&A, s, NULL, &ex); }
    int y0 = phys_y(&A.o.chara); run(15);
    printf("voo: y %d -> %d flying=%d barra=%d\n", y0, phys_y(&A.o.chara), ROBO(&A)->flying, A.air_option_bar);
    assert(ROBO(&A)->flying || phys_y(&A.o.chara) < y0);

    /* ClapJump perto do oponente */
    reset(-20, 10);
    clap_data_t cd = { { 100, 0 }, 1 };
    act(&A, "ClapJump", &cd); run(40);
    printf("clap: hp B=%d estado A=%s\n", B.hp, nm(&A));

    /* Chainsaw */
    reset(-20, 10);
    saw_data_t sd = { { 100, -100 }, { 100, 100 } };
    act(&A, "Chainsaw", &sd); run(45);
    printf("chainsaw: hp B=%d estado A=%s\n", B.hp, nm(&A));

    puts("robo ok");
    return 0;
}
