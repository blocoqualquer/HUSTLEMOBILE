#!/usr/bin/env python3
"""Extrai 'export var' (com padroes), enums e heranca dos scripts GDScript."""
import re, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from tscn import parse_value

_cls_re = re.compile(r'\{\s*"base":\s*"(\w+)",\s*"class":\s*"(\w+)",\s*"language":\s*"GDScript",\s*"path":\s*"res://([^"]+)"\s*\}')
_exp_re = re.compile(r'^export(?:\((.*?)\))?\s+var\s+(\w+)\s*(?::\s*(\w+))?\s*(?:=\s*(.*?))?\s*(?:#.*)?$')
_enum_re = re.compile(r'^enum\s+(\w+)\s*\{([^}]*)\}', re.M)
_ext_re = re.compile(r'^extends\s+(.+?)\s*$', re.M)

class Project:
    def __init__(self, root):
        self.root = root
        txt = open(os.path.join(root, 'project.godot'), encoding='utf-8').read()
        self.classes = {name: dict(base=base, path=path) for base, name, path in _cls_re.findall(txt)}
        self._cache = {}

    def script(self, path):
        if path in self._cache: return self._cache[path]
        full = os.path.join(self.root, path)
        info = dict(path=path, base=None, exports={}, enums={}, consts={})
        self._cache[path] = info
        if not os.path.exists(full): return info
        src = open(full, encoding='utf-8').read()
        m = _ext_re.search(src)
        if m:
            b = m.group(1).strip()
            info['base'] = b[1:-1].replace('res://', '') if b.startswith('"') else b
        for em in _enum_re.finditer(src):
            vals = {}; n = 0
            body = re.sub(r'#[^\n]*', '', em.group(2))
            for item in re.split(r'[,\n]', body):
                item = item.strip()
                if not item: continue
                if '=' in item:
                    k, v = item.split('='); n = int(v.strip()); item = k.strip()
                vals[item] = n; n += 1
            info['enums'][em.group(1)] = vals
        for line in src.split('\n'):
            line = line.rstrip()
            m = _exp_re.match(line)
            if m:
                hint, name, typ, default = m.groups()
                info['exports'][name] = dict(hint=hint, type=typ, default=default)
            else:
                cm = re.match(r'^const\s+(\w+)\s*=\s*(.+?)\s*(?:#.*)?$', line)
                if cm: info['consts'][cm.group(1)] = cm.group(2)
                vm = re.match(r'^var\s+(\w+)\s*(?::\s*(\w+))?\s*(?:=\s*(.*?))?\s*(?:#.*)?$', line)
                if vm and vm.group(1) not in info['exports']:
                    info.setdefault('vars', {})[vm.group(1)] = dict(hint=None, type=vm.group(2), default=vm.group(3))
        return info

    def chain(self, path):
        """lista de scripts do mais base ao mais derivado."""
        out = []; cur = path; seen = set()
        while cur and cur not in seen:
            seen.add(cur)
            s = self.script(cur); out.append(s)
            b = s['base']
            if b is None: break
            if b in self.classes: cur = self.classes[b]['path']
            elif b.endswith('.gd'): cur = b
            else: break
        return list(reversed(out))

    def defaults(self, path):
        ch = self.chain(path)
        enums = {}
        for s in ch: enums.update(s['enums'])
        consts = {}
        for s in ch: consts.update(s['consts'])
        res = {}
        for s in ch:
            for k, e in s.get('vars', {}).items():
                if e['default'] is None and not e['type']: continue
                v = self._eval(e, enums, consts)
                if not (isinstance(v, dict) and 'expr' in v): res.setdefault(k, v)
            for k, e in s['exports'].items():
                if k.startswith('_c_'): continue
                res[k] = self._eval(e, enums, consts)
        return res

    def _eval(self, e, enums, consts):
        d = e['default']
        hint = (e['hint'] or '').split(',')[0].strip()
        if d is None:
            t = e['type'] or hint
            if t in ('int',): return 0
            if t in ('float',): return 0.0
            if t in ('String',): return ''
            if t in ('bool',): return False
            if hint in enums: return 0
            return None
        d = d.strip()
        m = re.fullmatch(r'(?:\w+\.)?(\w+)\.(\w+)', d)
        if m and m.group(1) in enums and m.group(2) in enums[m.group(1)]:
            return enums[m.group(1)][m.group(2)]
        m3 = re.fullmatch(r'(\w+)\.(\w+)\.(\w+)', d)
        if m3 and m3.group(1) in self.classes:
            for sc in self.chain(self.classes[m3.group(1)]['path']):
                if m3.group(2) in sc['enums'] and m3.group(3) in sc['enums'][m3.group(2)]:
                    return sc['enums'][m3.group(2)][m3.group(3)]
        if d in consts: d = consts[d]
        if d.startswith('preload('):
            mm = re.match(r'preload\("res://([^"]+)"\)', d)
            return {'ext': mm.group(1)} if mm else {'expr': d}
        try:
            from tscn import _val, _ws
            v, j = _val(d, 0)
            if _ws(d, j) != len(d): return {'expr': d}
            return v
        except Exception:
            return {'expr': d}

if __name__ == '__main__':
    p = Project(sys.argv[1])
    print(len(p.classes), 'classes globais')
    d = p.defaults(sys.argv[2])
    print(len(d), 'exports'); 
    for k in list(d)[:15]: print(' ', k, '=', d[k])
