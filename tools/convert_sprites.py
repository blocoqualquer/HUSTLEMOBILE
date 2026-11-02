#!/usr/bin/env python3
"""Converte os sprites de um personagem em atlas PSP (8 bits, paleta CLUT, swizzled).

uso: convert_sprites.py <raiz_do_jogo> <personagem> <saida_dir>
Gera:  <saida>/<char>.tex   (YTX1: paleta RGBA8888 + paginas 8-bit swizzled)
       <saida>/<char>.spi   (YSP1: indice de sprites ordenado por hash FNV-1a)
       <saida>/<char>.json  (manifesto legivel, para depuracao)
"""
import sys, os, glob, json, struct
import numpy as np
from PIL import Image

PAGE_W = 512
PAGE_H = 512
PAD = 1

def fnv1a(s):
    h = 0x811C9DC5
    for b in s.encode():
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h

def swizzle8(pix):
    """pix: array (H, W) uint8 -> bytes na ordem swizzled do PSP (blocos 16x8)."""
    h, w = pix.shape
    assert w % 16 == 0 and h % 8 == 0
    a = pix.reshape(h // 8, 8, w // 16, 16).transpose(0, 2, 1, 3)
    return a.tobytes()

def main(root, char, out):
    base = os.path.join(root, 'characters', char)
    files = sorted(p for p in glob.glob(base + '/**/*.png', recursive=True)
                   if 'actionicons' not in p)
    sprites = []
    for p in files:
        im = Image.open(p).convert('RGBA')
        bb = im.getbbox()
        name = os.path.relpath(p, root).replace('\\', '/')
        if bb is None:
            sprites.append(dict(name=name, img=None, ox=0, oy=0, uw=im.width, uh=im.height))
            continue
        crop = im.crop(bb)
        if crop.width + PAD > PAGE_W or crop.height + PAD > PAGE_H:
            # imagem maior que uma pagina: fatia em pedacos de 256 px, nomeados name#i (i = linha*cols + coluna)
            S = 256
            cols = (crop.width + S - 1) // S
            rows = (crop.height + S - 1) // S
            for r in range(rows):
                for c in range(cols):
                    piece = crop.crop((c * S, r * S, min(crop.width, (c + 1) * S), min(crop.height, (r + 1) * S)))
                    sprites.append(dict(name=f'{name}#{r * cols + c}', img=piece,
                                        ox=bb[0] + c * S, oy=bb[1] + r * S, uw=im.width, uh=im.height))
            continue
        sprites.append(dict(name=name, img=crop, ox=bb[0], oy=bb[1], uw=im.width, uh=im.height))

    # paleta: indice 0 = transparente
    colors = {}
    for s in sprites:
        if s['img'] is None: continue
        a = np.array(s['img']).reshape(-1, 4)
        for c in np.unique(a[a[:, 3] > 0], axis=0):
            colors.setdefault(tuple(int(v) for v in c), None)
    pal = sorted(colors)
    if len(pal) > 255:
        raise SystemExit(f'{char}: {len(pal)} cores (>255); precisa quantizar')
    cmap = {c: i + 1 for i, c in enumerate(pal)}

    # empacotamento por prateleiras, ordenado por altura
    order = sorted([s for s in sprites if s['img'] is not None],
                   key=lambda s: (-s['img'].height, -s['img'].width))
    pages = []  # cada pagina: lista de prateleiras
    placed = []
    cur = dict(shelves=[], y=0)
    pages.append(cur)
    def place(s, page):
        w, h = s['img'].size
        w += PAD; h += PAD
        for sh in page['shelves']:
            if h <= sh['h'] and sh['x'] + w <= PAGE_W:
                x, y = sh['x'], sh['y']; sh['x'] += w; return x, y
        if page['y'] + h <= PAGE_H and w <= PAGE_W:
            sh = dict(x=w, y=page['y'], h=h); page['shelves'].append(sh)
            x, y = 0, page['y']; page['y'] += h; return x, y
        return None
    for s in order:
        if s['img'].width + PAD > PAGE_W or s['img'].height + PAD > PAGE_H:
            raise SystemExit(f"{s['name']}: sprite maior que a pagina {PAGE_W}x{PAGE_H}")
        r = place(s, cur)
        if r is None:
            cur = dict(shelves=[], y=0); pages.append(cur)
            r = place(s, cur)
        s['page'] = len(pages) - 1; s['x'], s['y'] = r

    bufs = [np.zeros((PAGE_H, PAGE_W), np.uint8) for _ in pages]
    for s in order:
        a = np.array(s['img']).reshape(-1, 4)
        idx = np.array([cmap[tuple(int(v) for v in c)] if c[3] > 0 else 0 for c in a], np.uint8)
        w, h = s['img'].size
        bufs[s['page']][s['y']:s['y'] + h, s['x']:s['x'] + w] = idx.reshape(h, w)

    # altura util de cada pagina (multiplo de 8) para economizar memoria
    heights = []
    for pg in pages:
        used = max(pg['y'], 8)
        heights.append(((used + 7) // 8) * 8)

    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, char + '.tex'), 'wb') as f:
        f.write(b'YTX1')
        f.write(struct.pack('<HHH', len(pages), PAGE_W, 0))
        pal_bytes = bytearray(256 * 4)
        for c, i in cmap.items():
            pal_bytes[i * 4:i * 4 + 4] = bytes(c)
        f.write(pal_bytes)
        for b, h in zip(bufs, heights):
            f.write(struct.pack('<H', h))
            f.write(swizzle8(b[:h]))

    entries = sorted(sprites, key=lambda s: fnv1a(s['name']))
    with open(os.path.join(out, char + '.spi'), 'wb') as f:
        f.write(b'YSP1'); f.write(struct.pack('<I', len(entries)))
        for s in entries:
            if s['img'] is None:
                f.write(struct.pack('<IHHHHHhhHH', fnv1a(s['name']), 0xFFFF, 0, 0, 0, 0, 0, 0, s['uw'], s['uh']))
            else:
                w, h = s['img'].size
                f.write(struct.pack('<IHHHHHhhHH', fnv1a(s['name']), s['page'], s['x'], s['y'], w, h,
                                    s['ox'], s['oy'], s['uw'], s['uh']))
    json.dump([dict(name=s['name'], hash=fnv1a(s['name']), page=s.get('page'), x=s.get('x'), y=s.get('y'),
                    w=s['img'].width if s['img'] else 0, h=s['img'].height if s['img'] else 0,
                    ox=s['ox'], oy=s['oy'], uw=s['uw'], uh=s['uh']) for s in entries],
              open(os.path.join(out, char + '.json'), 'w'), indent=0)
    kb = sum(512 * h for h in heights) / 1024
    print(f'{char}: {len(entries)} sprites, {len(pal)} cores, {len(pages)} paginas, {kb:.0f} KB')

if __name__ == '__main__':
    main(*sys.argv[1:4])
