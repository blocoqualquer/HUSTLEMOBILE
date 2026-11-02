#ifndef GFX_H
#define GFX_H
#include <stdint.h>

typedef struct { uint32_t hash; uint16_t page, x, y, w, h; int16_t ox, oy; uint16_t uw, uh; } SprEntry;

typedef struct {
    int npages;
    uint16_t page_h[16];        /* altura util de cada pagina */
    uint8_t *page_data[16];     /* pixels swizzled, alinhados a 16 bytes */
    uint32_t *clut;             /* 256 x RGBA8888 */
    int nspr;
    SprEntry *spr;              /* ordenado por hash */
} Atlas;

int  atlas_load(Atlas *a, const char *base_path);        /* base sem extensao; carrega .tex e .spi */
void atlas_free(Atlas *a);
const SprEntry *atlas_find(const Atlas *a, uint32_t hash);
uint32_t gfx_fnv1a(const char *s);

void gfx_init(void);
void gfx_begin(void);
void gfx_end(void);
void gfx_bind_page(const Atlas *a, int page);
/* desenha o sprite com a origem original (sem recorte) em (x,y) da tela */
void gfx_draw_sprite(const SprEntry *s, int x, int y, int flip_x);
void gfx_draw_page(const Atlas *a, int page, int x, int y);
#endif
