/* Fluxos de jogo: pulo, dash, bloqueio/parry, agarrao -> arremesso. */
#include "../src/game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern const chardata_t chardata_ninja;
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
    f->o.chara.px = FX_INT(x); f->o.chara.grounded = true; f->sadness_enabled = true;
    obj_update_boxes(&f->o);
}
static game_t g;
static void reset(int ax, int bx) { setup(&A, &B, SA, 1, ax); setup(&B, &A, SB, 2, bx); game_init(&g, &A, &B); for (int t = 0; t < 3; t++) game_tick(&g); }
static void run(int n) { while (n--) game_tick(&g); }
static const char *nm(fighter_t *f) { return f->o.sm.cur->d.name; }
static int id(const char *n) { return fighter_name_id(&A, n); }
static void act(fighter_t *f, const char *n, void *data) { int s = fighter_name_id(f, n); assert(s >= 0); fighter_select_action(f, s, data, NULL); }
int main(void)
{
    /* 1. pulo: sobe e volta ao chao */
    reset(-100, 100);
    xy_data_t up = { 0, -100 };
    act(&A, "Jump", &up);
    int maxh = 0, landed = -1;
    for (int t = 0; t < 120; t++) { game_tick(&g); if (getenv("DBG") && t < 30) fprintf(stderr,"t%d %s tick=%d y=%d vy=%lld gr=%d\n",t,nm(&A),A.o.sm.cur->current_tick,phys_y(&A.o.chara),(long long)A.o.chara.vy,A.o.chara.grounded); int y = -phys_y(&A.o.chara); if (y > maxh) maxh = y; if (landed < 0 && t > 10 && A.o.chara.grounded) landed = t; }
    printf("pulo: altura max %d, aterrissou no tick %d, estado final %s\n", maxh, landed, nm(&A));
    assert(maxh > 40 && landed > 10);
    assert(A.o.sm.cur->id == id("Wait"));

    /* 2. dash para frente move o lutador */
    reset(-100, 100);
    xy_data_t fwd = { 100, 0 };
    int x0 = phys_x(&A.o.chara);
    act(&A, "DashStartup", &fwd);
    run(20);
    printf("dash: x %d -> %d, estado %s\n", x0, phys_x(&A.o.chara), nm(&A));
    assert(phys_x(&A.o.chara) > x0 + 20);

    /* 3. bloqueio: B faz ParryHigh antes do golpe e nao toma dano total (chip) */
    reset(-20, 6);
    act(&B, "ParryHigh", NULL);
    act(&A, "QuickSlash", NULL);
    run(40);
    printf("bloqueio: hp B=%d (chip), A super=%d, B estado=%s\n", B.hp, A.super_meter, nm(&B));
    assert(B.hp > 1380);

    /* 4. agarrao: A agarra B (Grab) e o arremessa */
    reset(-20, 6);
    xy_data_t gd = { 1, 0 };
    act(&A, "Grab", &gd);
    int grabbed = 0, hp_min = B.hp;
    for (int t = 0; t < 80; t++) { game_tick(&g); if (B.o.sm.cur->id == B.st_grabbed) grabbed = 1; if (B.hp < hp_min) hp_min = B.hp; }
    printf("agarrao: grabbed=%d, hp B=%d, A=%s B=%s\n", grabbed, hp_min, nm(&A), nm(&B));
    assert(grabbed);
    assert(hp_min < 1500);
    puts("flows ok");
    return 0;
}
