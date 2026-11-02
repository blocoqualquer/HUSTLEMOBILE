import json, sys
def norm(v):
    if isinstance(v, str):
        try: return float(v)
        except ValueError: return v
    if isinstance(v, bool): return int(v)
    if isinstance(v, (int, float)): return float(v)
    return v
tot = bad = miss = 0
for nm in sys.argv[2:]:
    mine = json.load(open(f'data/chars/{nm}.json'))['nodes']
    real = json.load(open(f'{sys.argv[1]}/dump_{nm}.json'))
    nb = nm_ = 0; shown = 0
    for path, props in real.items():
        key = 'StateMachine/' + path.split('StateMachine/', 1)[1] if 'StateMachine/' in path else path
        m = mine.get(key)
        if m is None:
            nm_ += 1; miss += 1
            if shown < 3: print('  falta no meu:', path); shown += 1
            continue
        for k, v in props.items():
            tot += 1
            mv = m['props'].get(k)
            if norm(mv) != norm(v):
                nb += 1; bad += 1
                if shown < 8: print(f'  {nm} {path}.{k}: meu={mv!r} real={v!r}'); shown += 1
    print(f'{nm}: {len(real)} nos reais, {nm_} ausentes, {nb} valores diferentes')
print('total valores', tot, 'diferentes', bad, 'nos ausentes', miss)
