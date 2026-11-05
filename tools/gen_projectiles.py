#!/usr/bin/env python3
"""Converte as cenas de projetil de um personagem e gera src/gen/<char>_<Cena>.c
uso: gen_projectiles.py <ROOT> <dir do personagem rel. ao ROOT> <id> <pasta tmp>"""
import json, os, subprocess, sys, glob
root, chardir, cid, tmp = sys.argv[1:5]
conv = '/home/claude/yomi-psp/tools/convert_char.py'
gen = os.path.join(os.path.dirname(__file__), 'gen_c_data.py')
os.makedirs(tmp, exist_ok=True)
out = []
for t in sorted(glob.glob(os.path.join(root, chardir, '**', '*.tscn'), recursive=True)):
    rel = os.path.relpath(t, root)
    if '/projectiles/' not in '/' + rel and 'projectile' not in rel.lower(): continue
    name = os.path.basename(t)[:-5]
    j = os.path.join(tmp, f'{cid}_{name}.json')
    r = subprocess.run([sys.executable, conv, root, rel, j], capture_output=True, text=True)
    if r.returncode or not os.path.exists(j): continue
    d = json.load(open(j))
    rootn = d['nodes'].get('.') or {}
    if 'immunity_susceptible' not in rootn.get('props', {}): continue          # so BaseProjectile
    c = os.path.join('src/gen', f'{cid}_{name}.c')
    r = subprocess.run([sys.executable, gen, j, f'{cid}_{name}', c, root], capture_output=True, text=True)
    print(name, '->', c, '|', (r.stdout.strip().splitlines() or [r.stderr.strip()[-120:]])[-1])
