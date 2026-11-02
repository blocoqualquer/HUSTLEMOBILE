#!/usr/bin/env python3
"""Parser de .tscn/.tres do Godot 3 com resolucao de heranca (instance=ExtResource)."""
import re, sys, json, os

class V2(tuple): pass

def parse_value(s):
    s = s.strip()
    val, rest = _val(s, 0)
    return val

_num = re.compile(r'-?(?:\d+\.?\d*(?:e[-+]?\d+)?|\.\d+|inf|nan)', re.I)
_ident = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')

def _ws(s, i):
    while i < len(s) and s[i] in ' \t\r\n': i += 1
    return i

def _val(s, i):
    i = _ws(s, i)
    c = s[i]
    if c == '"':
        j = i + 1; out = []
        while s[j] != '"':
            if s[j] == '\\':
                nx = s[j + 1]; out.append({'n': '\n', 't': '\t', '"': '"', '\\': '\\'}.get(nx, nx)); j += 2
            else:
                out.append(s[j]); j += 1
        return ''.join(out), j + 1
    if c == '[':
        arr = []; i += 1
        while True:
            i = _ws(s, i)
            if s[i] == ']': return arr, i + 1
            v, i = _val(s, i); arr.append(v); i = _ws(s, i)
            if s[i] == ',': i += 1
    if c == '{':
        d = {}; i += 1
        while True:
            i = _ws(s, i)
            if s[i] == '}': return d, i + 1
            k, i = _val(s, i); i = _ws(s, i)
            assert s[i] == ':', s[i - 20:i + 20]
            v, i = _val(s, i + 1); d[k if not isinstance(k, (list, dict)) else str(k)] = v; i = _ws(s, i)
            if s[i] == ',': i += 1
    m = _num.match(s, i)
    if m and (c.isdigit() or c in '-.'):
        t = m.group(0)
        try: return (int(t) if re.fullmatch(r'-?\d+', t) else float(t)), m.end()
        except ValueError: return float(t), m.end()
    m = _ident.match(s, i)
    if m:
        name = m.group(0); j = m.end()
        if name == 'true': return True, j
        if name == 'false': return False, j
        if name == 'null': return None, j
        if name in ('inf', 'nan'): return float(name), j
        j = _ws(s, j)
        if j < len(s) and s[j] == '(':
            args = []; j += 1
            while True:
                j = _ws(s, j)
                if s[j] == ')': break
                v, j = _val(s, j); args.append(v); j = _ws(s, j)
                if s[j] == ',': j += 1
            return {'_t': name, 'a': args}, j + 1
        return name, j
    raise ValueError('valor invalido: ' + s[i:i + 40])

def _logical_lines(text):
    """junta linhas ate fechar aspas/colchetes/chaves."""
    buf = []; depth = 0; inq = False
    for line in text.split('\n'):
        if not buf and not inq and depth == 0 and (line.startswith('[') and not '=' in line.split(']')[0][:2]):
            pass
        buf.append(line)
        k = 0
        while k < len(line):
            ch = line[k]
            if inq:
                if ch == '\\': k += 1
                elif ch == '"': inq = False
            else:
                if ch == '"': inq = True
                elif ch in '[{(': depth += 1
                elif ch in ']})': depth -= 1
            k += 1
        if not inq and depth == 0:
            yield '\n'.join(buf); buf = []
    if buf: yield '\n'.join(buf)

def parse_header(line):
    """'[kind a="x" b=[..] c=ExtResource( 3 )]' -> (kind, {a:..})"""
    body = line.strip()[1:-1]
    m = re.match(r'\s*(\w+)', body)
    kind = m.group(1); i = m.end(); attrs = {}
    while True:
        i = _ws(body, i)
        if i >= len(body): break
        m = re.compile(r'([\w/]+)=').match(body, i)
        if not m: raise ValueError('cabecalho nao entendido: ' + line[:120])
        key = m.group(1)
        v, i = _val(body, m.end())
        if isinstance(v, dict) and v.get('_t') in ('ExtResource', 'SubResource'):
            v = str(v['a'][0])
        attrs[key] = v
    return kind, attrs

def parse_file(path):
    text = open(path, encoding='utf-8').read()
    ext, subs, nodes, res = {}, {}, [], None
    cur = None
    for ll in _logical_lines(text):
        ll = ll.strip('\n')
        if not ll.strip(): continue
        if ll.startswith('['):
            kind, attrs = parse_header(ll)
            if kind == 'ext_resource':
                ext[int(attrs['id'])] = dict(path=attrs['path'].replace('res://', ''), type=attrs.get('type'))
                cur = None
            elif kind == 'sub_resource':
                cur = dict(type=attrs['type'], props={}); subs[int(attrs['id'])] = cur
            elif kind == 'node':
                inst = attrs.get('instance')
                if inst: inst = int(inst)
                cur = dict(name=attrs['name'], type=attrs.get('type'), parent=attrs.get('parent'), instance=inst, props={}, groups=attrs.get('groups'))
                nodes.append(cur)
            elif kind == 'resource':
                cur = dict(type='resource', props={}); res = cur
            else:
                cur = None
            continue
        if cur is None: continue
        k, _, v = ll.partition(' = ')
        if not _:
            k, _, v = ll.partition('=')
        cur['props'][k.strip()] = parse_value(v)
    return dict(ext=ext, subs=subs, nodes=nodes, resource=res)

def ref(v):
    if isinstance(v, dict) and v.get('_t') == 'ExtResource': return ('ext', v['a'][0])
    if isinstance(v, dict) and v.get('_t') == 'SubResource': return ('sub', v['a'][0])
    return None

def node_path(n):
    if n['parent'] is None: return '.'
    return n['name'] if n['parent'] == '.' else n['parent'] + '/' + n['name']

def resolve_scene(root, rel, _cache=None):
    """devolve {path: {name,type,props,script}} com heranca resolvida; props contem refs ext/sub ja resolvidas ao caminho."""
    sc = parse_file(os.path.join(root, rel))
    out = {}
    order = []
    for n in sc['nodes']:
        p = node_path(n)
        if n['instance'] is not None and n['parent'] is None or (n['instance'] is not None):
            base_rel = sc['ext'][n['instance']]['path']
            base = resolve_scene(root, base_rel)
            # instancia: copia tudo sob o caminho p
            for bp, bn in base['nodes'].items():
                newp = p if bp == '.' else (p + '/' + bp if p != '.' else bp)
                out[newp] = dict(bn, props=dict(bn['props']))
                if newp not in order: order.append(newp)
            if p in out:
                out[p]['props'].update(_conv(n['props'], sc, root))
                out[p]['name'] = n['name']
                out[p]['file'] = rel
            continue
        if n['type'] is None and p in out:   # override de no herdado
            out[p]['props'].update(_conv(n['props'], sc, root))
            if 'script' in n['props']:
                pass
            continue
        out[p] = dict(name=n['name'], type=n['type'], props=_conv(n['props'], sc, root), file=rel, parent=n['parent'])
        order.append(p)
    return dict(nodes={p: out[p] for p in order}, scene=sc)

def _conv(props, sc, root):
    def cv(v):
        r = ref(v)
        if r and r[0] == 'ext': return {'ext': sc['ext'][r[1]]['path']}
        if r and r[0] == 'sub': return {'sub': r[1], 'sub_type': sc['subs'][r[1]]['type'], 'sub_props': {k: cv(x) for k, x in sc['subs'][r[1]]['props'].items()}}
        if isinstance(v, list): return [cv(x) for x in v]
        if isinstance(v, dict) and '_t' not in v: return {k: cv(x) for k, x in v.items()}
        return v
    return {k: cv(v) for k, v in props.items()}

if __name__ == '__main__':
    r = resolve_scene(sys.argv[1], sys.argv[2])
    print(len(r['nodes']), 'nos')
    if len(sys.argv) > 3:
        json.dump(r['nodes'], open(sys.argv[3], 'w'), indent=1, default=str)
