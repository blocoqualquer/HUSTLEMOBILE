#include "../src/fighter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static fighter_t a, b;
static void reset(void){
    memset(&a,0,sizeof a); memset(&b,0,sizeof b);
    fighter_t *p[2]={&a,&b};
    for(int i=0;i<2;i++){ fighter_t*f=p[i]; f->opponent=p[1-i]; f->max_health=1500; f->hp=f->trail_hp=1500;
        f->burst_enabled=f->sadness_enabled=true; f->min_di_scaling=FX_ONE; f->max_di_scaling=FX_INT(6); f->di_combo_limit=15; f->di_modifier=FX_ONE;
        f->damage_taken_modifier=f->global_damage_modifier=FX_ONE; f->parry_combo_scaling=f->burst_parry_combo_scaling=FXC_PARRY_COMBO_SCALING; }
}
int main(void){
    assert(fighter_get_combo_stale(0)==FX_ONE);
    assert(fighter_get_combo_stale(15)==FXC_MIN_STALE_MODIFIER);
    assert(fighter_get_combo_stale(99)==FXC_MIN_STALE_MODIFIER);
    for(int i=0;i<15;i++) assert(fighter_get_combo_stale(i)>fighter_get_combo_stale(i+1));
    reset();
    fighter_take_damage(&a,100,0,FX_ONE,0,FX_ONE,false,false);
    assert(a.hp==1400); assert(b.combo_damage==100);
    /* ganho de super do atacante: 100 de dano => 100 de medidor (divisor 1), staling 0 hits, current_tick 0 => sem 0.85 */
    assert(b.super_meter==100 && b.supers_available==0);
    assert(a.super_meter==33);
    /* combo de 5 hits escala o dano */
    reset(); b.combo_count=5;
    fighter_take_damage(&a,100,0,FX_ONE,0,FX_ONE,false,false);
    assert(a.hp>1400 && a.hp<1500);
    /* nunca morre em armadura; chega a 0 */
    reset(); a.hp=10; fighter_take_damage(&a,500,0,FX_ONE,0,FX_ONE,false,false); assert(a.hp==0);
    reset(); a.hp=10; a.has_hyper_armor=true; fighter_take_damage(&a,500,0,FX_ONE,0,FX_ONE,false,false); assert(a.hp==1);
    /* super: 125 = 1 barra */
    reset(); fighter_gain_super_meter_raw(&a,130); assert(a.supers_available==1 && a.super_meter==5);
    /* DI scaling: 0 hits => 1.0; 15 hits => 6.0 */
    reset(); assert(fighter_get_di_scaling(&a,false,0)==FX_ONE);
    b.combo_count=15; assert(fighter_get_di_scaling(&a,false,0)==FX_INT(6));
    /* penalidade acima do maximo zera super e liga penalty_ticks */
    reset(); a.penalty=74; a.supers_available=2; a.sadness_enabled=true; fighter_add_penalty(&a,5,true);
    assert(a.penalty_ticks==PENALTY_TICKS && a.supers_available==0);
    puts("fighter ok"); return 0; }
