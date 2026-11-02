#include "gfx.h"
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define SCR_W 480
#define SCR_H 272
#define BUF_W 512

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct { int16_t u, v; int16_t x, y, z; } Vtx;   /* GU_TEXTURE_16BIT | GU_VERTEX_16BIT */
#define VTX_FMT (GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D)

uint32_t gfx_fnv1a(const char *s)
{
    uint32_t h = 0x811C9DC5u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 0x01000193u;
    return h;
}

static int read_all(const char *path, uint8_t **out, long *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
    *out = (uint8_t *)memalign(16, (*size + 15) & ~15L);
    if (!*out) { fclose(f); return -1; }
    if (fread(*out, 1, *size, f) != (size_t)*size) { fclose(f); free(*out); return -1; }
    fclose(f);
    return 0;
}

static int next_pow2(int v) { int p = 8; while (p < v) p <<= 1; return p; }

int atlas_load(Atlas *a, const char *base)
{
    char path[256];
    uint8_t *buf; long size;
    memset(a, 0, sizeof *a);

    snprintf(path, sizeof path, "%s.tex", base);
    if (read_all(path, &buf, &size)) return -1;
    if (memcmp(buf, "YTX1", 4)) { free(buf); return -2; }
    uint16_t np, pw;
    memcpy(&np, buf + 4, 2); memcpy(&pw, buf + 6, 2);
    if (np > 16 || pw != 512) { free(buf); return -3; }
    a->npages = np;
    a->clut = (uint32_t *)memalign(16, 1024);
    memcpy(a->clut, buf + 10, 1024);
    long o = 10 + 1024;
    for (int i = 0; i < np; i++) {
        uint16_t h; memcpy(&h, buf + o, 2); o += 2;
        a->page_h[i] = h;
        int p2 = next_pow2(h);
        a->page_data[i] = (uint8_t *)memalign(16, 512 * p2);
        memset(a->page_data[i], 0, 512 * p2);
        memcpy(a->page_data[i], buf + o, 512 * h);
        sceKernelDcacheWritebackRange(a->page_data[i], 512 * p2);
        o += 512 * h;
    }
    sceKernelDcacheWritebackRange(a->clut, 1024);
    free(buf);

    snprintf(path, sizeof path, "%s.spi", base);
    if (read_all(path, &buf, &size)) return -4;
    if (memcmp(buf, "YSP1", 4)) { free(buf); return -5; }
    uint32_t n; memcpy(&n, buf + 4, 4);
    a->nspr = (int)n;
    a->spr = (SprEntry *)malloc(n * sizeof(SprEntry));
    const uint8_t *p = buf + 8;
    for (uint32_t i = 0; i < n; i++, p += 22) {   /* formato <IHHHHHhhHH = 4+5*2+2*2+2*2 = 22 bytes */
        SprEntry *e = &a->spr[i];
        memcpy(&e->hash, p, 4);
        memcpy(&e->page, p + 4, 2); memcpy(&e->x, p + 6, 2); memcpy(&e->y, p + 8, 2);
        memcpy(&e->w, p + 10, 2);  memcpy(&e->h, p + 12, 2);
        memcpy(&e->ox, p + 14, 2); memcpy(&e->oy, p + 16, 2);
        memcpy(&e->uw, p + 18, 2); memcpy(&e->uh, p + 20, 2);
    }
    free(buf);
    return 0;
}

void atlas_free(Atlas *a)
{
    for (int i = 0; i < a->npages; i++) free(a->page_data[i]);
    free(a->clut); free(a->spr);
    memset(a, 0, sizeof *a);
}

const SprEntry *atlas_find(const Atlas *a, uint32_t hash)
{
    int lo = 0, hi = a->nspr - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a->spr[mid].hash == hash) return &a->spr[mid];
        if (a->spr[mid].hash < hash) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

void gfx_init(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_5650, (void *)0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)0x88000, BUF_W);   /* 16 bits: 512*272*2 = 0x44000 por buffer */
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void gfx_begin(void)
{
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(0xFF402020);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void gfx_end(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void gfx_bind_page(const Atlas *a, int page)
{
    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(32, a->clut);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);                       /* 1 = swizzled */
    sceGuTexImage(0, 512, next_pow2(a->page_h[page]), 512, a->page_data[page]);
}

static void quad(int u0, int v0, int u1, int v1, int x0, int y0, int x1, int y1)
{
    Vtx *v = (Vtx *)sceGuGetMemory(2 * sizeof(Vtx));
    v[0] = (Vtx){ (int16_t)u0, (int16_t)v0, (int16_t)x0, (int16_t)y0, 0 };
    v[1] = (Vtx){ (int16_t)u1, (int16_t)v1, (int16_t)x1, (int16_t)y1, 0 };
    sceGuDrawArray(GU_SPRITES, VTX_FMT, 2, 0, v);
}

void gfx_draw_sprite(const SprEntry *s, int x, int y, int flip_x)
{
    if (!s || s->page == 0xFFFF) return;
    int dx = x + s->ox, dy = y + s->oy;
    if (flip_x) quad(s->x + s->w, s->y, s->x, s->y + s->h, dx, dy, dx + s->w, dy + s->h);
    else        quad(s->x, s->y, s->x + s->w, s->y + s->h, dx, dy, dx + s->w, dy + s->h);
}

void gfx_draw_page(const Atlas *a, int page, int x, int y)
{
    quad(0, 0, 512, a->page_h[page], x, y, x + 512, y + a->page_h[page]);
}
