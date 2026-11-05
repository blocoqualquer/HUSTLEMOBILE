/* Scripts raiz de projeteis do Ninja (stickman). */
#include "projectile.h"

/* Kunai.gd: ao ser atingido por um lutador, desaparece */
static void kunai_hit_by(obj_t *self, hitbox_t *hb)
{
    if (hb->host && hb->host->is_fighter) proj_disable(self);
}
const proj_vt_t vtp_characters_stickman_projectiles_Kunai = { .hit_by = kunai_hit_by };
