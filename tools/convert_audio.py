#!/usr/bin/env python3
"""Converte WAVs em bancos PCM 16-bit mono 22050 Hz para o PSP.

uso: convert_audio.py <raiz_do_jogo> <saida_dir>
Bancos: common, ui, stickman, robo, wizard, mutant, swordandgun  -> <saida>/<nome>.snd (YAU1)
Musica: sound/music/bg1.mp3 -> <saida>/bg1.pcm (PCM cru, para streaming)
YAU1: magic, u32 n, n * (u32 hash, u32 offset_em_amostras, u32 n_amostras), PCM s16 LE
"""
import sys, os, glob, struct, subprocess, json
RATE = 22050

def fnv1a(s):
    h = 0x811C9DC5
    for b in s.encode():
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h

def decode(path, rate=RATE):
    return subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 's16le', '-ac', '1', '-ar', str(rate), '-'],
                          capture_output=True, check=True).stdout

BANKS = {
    'common': ['sound/common'], 'ui': ['sound/ui'],
    'stickman': ['characters/stickman'], 'robo': ['characters/robo'], 'wizard': ['characters/wizard'],
    'mutant': ['characters/mutant'], 'swordandgun': ['characters/swordandgun'],
}

def main(root, out):
    os.makedirs(out, exist_ok=True)
    manifest = {}
    for bank, dirs in BANKS.items():
        files = []
        for d in dirs:
            for ext in ('wav', 'ogg'):
                files += glob.glob(os.path.join(root, d, '**', '*.' + ext), recursive=True)
        files = sorted(files)
        entries = []; blob = bytearray()
        for p in files:
            name = os.path.relpath(p, root).replace('\\', '/')
            pcm = decode(p)
            if len(pcm) % 2: pcm += b'\0'
            entries.append((fnv1a(name), len(blob) // 2, len(pcm) // 2, name))
            blob += pcm
        entries.sort()
        with open(os.path.join(out, bank + '.snd'), 'wb') as f:
            f.write(b'YAU1'); f.write(struct.pack('<I', len(entries)))
            for h, o, n, _ in entries: f.write(struct.pack('<III', h, o, n))
            f.write(blob)
        manifest[bank] = [dict(name=e[3], hash=e[0], off=e[1], n=e[2]) for e in entries]
        print(f'{bank}: {len(entries)} sons, {len(blob)/1024:.0f} KB')
    mus = os.path.join(root, 'sound/music/bg1.mp3')
    if os.path.exists(mus):
        pcm = decode(mus)
        open(os.path.join(out, 'bg1.pcm'), 'wb').write(pcm)
        print(f'bg1.pcm: {len(pcm)/1048576:.1f} MB ({len(pcm)/2/RATE:.0f} s)')
    json.dump(manifest, open(os.path.join(out, 'audio.json'), 'w'), indent=0)

if __name__ == '__main__':
    main(*sys.argv[1:3])
