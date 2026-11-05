/* Ninja com scripts: Fireball, Uppercut, Summon, StickyBomb, GrapplingHook, Caltrops. */
#include "../src/game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/ninja.h"
extern const chardata_t chardata_ninja;
static fighter_t A, B;
static state_t SA[200], SB[200];
static void setup(fighter_t *f, fighter_t *op, state_t *st, int id, int x)
{
    const chardata_t *cd = &chardata_ninja;
    memset(f, 0, sizeof *f); f->vt = &vt_ninja;
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
    /* Fireball */
    reset(-100, 60);
    xy_data_t fwd = { 100, 0 };
    act(&A, "Fireball", &fwd);
    int spawned = 0;
    for (int i = 0; i < 90; i++) { game_tick(&g); if (g.nobjs > 0) spawned = 1; }
    printf("fireball: spawned=%d hp B=%d nobjs=%d A=%s\n", spawned, B.hp, g.nobjs, nm(&A));
    assert(spawned && B.hp < 1500);

    /* Uppercut corpo a corpo */
    reset(-20, 6);
    act(&A, "Uppercut", &fwd);
    run(30);
    printf("uppercut: hp B=%d estado B=%s cancel_pending=%d\n", B.hp, nm(&B), NINJA(&A)->cancel_pending);
    assert(B.hp < 1500);

    /* Summon kunai */
    reset(-100, 100);
    act(&A, "Summon", &fwd);
    int n0 = 0; for (int i = 0; i < 60; i++) { game_tick(&g); if (g.nobjs > n0) n0 = g.nobjs; }
    printf("summon: max objs=%d can_summon_kunai=%d\n", n0, NINJA(&A)->can_summon_kunai);
    assert(n0 >= 1);

    /* StickyBomb */
    reset(-100, 100);
    act(&A, "StickyBomb", NULL);
    run(25);
    printf("bomb: thrown=%d uid=%d objs=%d\n", NINJA(&A)->bomb_thrown, NINJA(&A)->bomb_uid, g.nobjs);
    assert(NINJA(&A)->bomb_thrown && g.nobjs >= 1);

    /* GrapplingHook */
    reset(-100, 100);
    xy_data_t up = { 100, -100 };
    act(&A, "GrapplingHook", &up);
    run(20);
    printf("hook: uid=%d objs=%d\n", NINJA(&A)->hook_uid, g.nobjs);
    assert(NINJA(&A)->hook_uid >= 0);

    /* Caltrops (super) */
    reset(-100, 100);
    A.supers_available = 3;
    act(&A, "Caltrops", NULL);
    run(20);
    printf("caltrops: objs=%d\n", g.nobjs);
    assert(g.nobjs >= 3);
    puts("ninja ok");
    return 0;
}
