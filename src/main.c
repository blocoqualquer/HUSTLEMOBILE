/* Fase 1: visualizador de atlas. Esquerda/direita troca de pagina, cima/baixo troca de personagem,
 * X mostra a pagina inteira, O mostra o sprite escolhido. L/R navega pelos sprites. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <string.h>
#include <stdio.h>
#include "gfx.h"

PSP_MODULE_INFO("YOMIHUSTLE", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

static int exit_cb(int a, int b, void *c) { (void)a; (void)b; (void)c; sceKernelExitGame(); return 0; }
static int cb_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int id = sceKernelCreateCallback("exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}

static const char *CHARS[] = { "stickman", "robo", "wizard", "mutant", "swordandgun" };

int main(int argc, char **argv)
{
    (void)argc;
    int th = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);

    char root[200];
    strncpy(root, argv[0], sizeof root - 1); root[sizeof root - 1] = 0;
    char *slash = strrchr(root, '/');
    if (slash) *slash = 0; else strcpy(root, ".");

    gfx_init();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    Atlas atlas; int cur = -1, page = 0, idx = 0, show_page = 1, ok = 0;
    SceCtrlData pad, prev; memset(&prev, 0, sizeof prev);
    int want = 0;
    for (;;) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~prev.Buttons; prev = pad;
        if (pressed & PSP_CTRL_UP)    want = (want + 4) % 5;
        if (pressed & PSP_CTRL_DOWN)  want = (want + 1) % 5;
        if (pressed & PSP_CTRL_LEFT)  page--;
        if (pressed & PSP_CTRL_RIGHT) page++;
        if (pressed & PSP_CTRL_LTRIGGER) idx--;
        if (pressed & PSP_CTRL_RTRIGGER) idx++;
        if (pressed & PSP_CTRL_CROSS)  show_page = 1;
        if (pressed & PSP_CTRL_CIRCLE) show_page = 0;

        if (want != cur) {
            if (ok) atlas_free(&atlas);
            char base[300]; snprintf(base, sizeof base, "%s/data/gfx/%s", root, CHARS[want]);
            ok = (atlas_load(&atlas, base) == 0); cur = want; page = 0; idx = 0;
        }
        gfx_begin();
        if (ok) {
            if (page < 0) page = atlas.npages - 1;
            if (page >= atlas.npages) page = 0;
            if (idx < 0) idx = atlas.nspr - 1;
            if (idx >= atlas.nspr) idx = 0;
            if (show_page) {
                gfx_bind_page(&atlas, page);
                gfx_draw_page(&atlas, page, -16, -120 + 0);
            } else {
                const SprEntry *s = &atlas.spr[idx];
                if (s->page != 0xFFFF) {
                    gfx_bind_page(&atlas, s->page);
                    gfx_draw_sprite(s, 176, 72, 0);
                }
            }
        }
        gfx_end();
    }
    return 0;
}
