#!/usr/bin/env python3
"""Resolve a cena de um personagem (com heranca de BaseChar.tscn) e grava JSON intermediario:
nos com script recebem os padroes dos `export var` + os valores sobrescritos na cena.
uso: convert_char.py <raiz_do_jogo> <cena.tscn> <saida.json>"""
import sys, os, json, collections
sys.path.insert(0, os.path.dirname(__file__))
from tscn import resolve_scene
from gdschema import Project

def find_scripts(props):
    s = props.get('script')
    return s['ext'] if isinstance(s, dict) and 'ext' in s else None

def convert(root, scene, out):
    proj = Project(root)
    r = resolve_scene(root, scene)
    nodes = r['nodes']
    res = {}
    unresolved = collections.Counter()
    for path, n in nodes.items():
        props = dict(n['props'])
        sp = find_scripts(props)
        merged = {}
        if sp and sp.endswith('.gd'):
            merged.update(proj.defaults(sp))
            for k, v in merged.items():
                if isinstance(v, dict) and 'expr' in v: unresolved[(sp, k)] += 1
        merged.update({k: v for k, v in props.items() if k not in ('script', '__meta__')})
        entry = dict(name=n['name'], type=n.get('type'), script=sp, props=merged)
        res[path] = entry
    # animacoes de sprite: qualquer propriedade 'frames' com sub_type SpriteFrames
    anims = {}
    for path, n in nodes.items():
        fr = n['props'].get('frames')
        if isinstance(fr, dict) and fr.get('sub_type') == 'SpriteFrames':
            for a in fr['sub_props'].get('animations', []):
                anims.setdefault(path, {})[a['name']] = dict(
                    loop=a.get('loop', True), speed=a.get('speed', 5.0),
                    frames=[f['ext'] if isinstance(f, dict) and 'ext' in f else None for f in a['frames']])
    json.dump(dict(nodes=res, animations=anims), open(out, 'w'), indent=0, default=str)
    st = [p for p in res if p.startswith('StateMachine/') and p.count('/') == 1]
    print(f'{os.path.basename(scene)}: {len(res)} nos, {len(st)} estados, {sum(len(v) for v in anims.values())} animacoes, '
          f'{len(unresolved)} padroes nao avaliados')
    return unresolved

if __name__ == '__main__':
    u = convert(*sys.argv[1:4])
    if len(sys.argv) > 4:
        for (s, k), c in sorted(u.items())[:40]: print('  ?', s, k)
