/* Integracao: dois ninjas, P1 ataca P2 e o dano/combo/hitstun fluem pelo motor. */
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
    f->o.chara.px = FX_INT(x); f->o.chara.grounded = true;
    obj_update_boxes(&f->o);
}
int main(void)
{
    setup(&A, &B, SA, 1, -20); setup(&B, &A, SB, 2, 6);
    const chardata_t *cd = &chardata_ninja;
    int atk = -1;
    for (int i = 0; i < cd->nstates; i++) {
        const state_def_t *d = &cd->states[i];
        if (d->nhb > 0 && d->cs && d->cs->air_type == 0 && d->hb[0].start_tick > 0 && d->hb[0].start_tick < 12 && d->hb[0].damage > 0 && d->hb[0].w > 0 && !d->hb[0].is_throwbox) { atk = i; break; }
    }
    assert(atk >= 0);
    printf("ataque: %s (start %d dmg %d)\n", cd->states[atk].name, cd->states[atk].hb[0].start_tick, cd->states[atk].hb[0].damage);
    game_t g; game_init(&g, &A, &B);
    for (int t = 0; t < 5; t++) game_tick(&g);
    fighter_select_action(&A, atk, NULL, NULL);
    int hit_tick = -1, maxcombo = 0, hp_after_hit = 0;
    for (int t = 0; t < 60; t++) {
        game_tick(&g);
        if (getenv("DBG")) { state_t *c=A.o.sm.cur; fprintf(stderr,"[cc=%d vis=%d inc=%d] t%d A:%s tick=%d x=%d nhb=%d act=%d | B:%s hp=%d x=%d\n",A.combo_count,A.visible_combo_count,A.opponent->o.combo_count,g.current_tick,c->d.name,c->current_tick,phys_x(&A.o.chara),c->d.nhb,c->d.nhb?c->hbr[0].active:-1,B.o.sm.cur->d.name,B.hp,phys_x(&B.o.chara)); if(c->d.nhb&&c->hbr[0].active){aabb_t a=box_aabb(&c->hbr[0].box),b=box_aabb(&B.o.hurtbox);fprintf(stderr,"  hb [%d %d %d %d] hurt [%d %d %d %d] enabled=%d hits_g=%d hits_a=%d standing=%d\n",a.x1,a.x2,a.y1,a.y2,b.x1,b.x2,b.y1,b.y2,c->hbr[0].enabled,c->hbr[0].def->hits_vs_grounded,c->hbr[0].def->hits_vs_aerial,c->hbr[0].def->hits_vs_standing);} }
        if (hit_tick < 0 && B.hp < B.max_health) { hit_tick = g.current_tick; hp_after_hit = B.hp; }
        if (A.combo_count > maxcombo) maxcombo = A.combo_count;
    }
    printf("hit no tick %d, hp B=%d, combo=%d, estado B=%s, super A=%d\n", hit_tick, B.hp, A.combo_count, B.o.sm.cur->d.name, A.super_meter);
    assert(hit_tick > 0);
    assert(B.hp < B.max_health);
    assert(maxcombo >= 1);
    assert(A.super_meter > 0 || A.supers_available > 0);
    assert(B.hp == hp_after_hit || B.hp < hp_after_hit);
    puts("game ok");
    return 0;
}
