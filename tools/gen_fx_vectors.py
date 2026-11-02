#!/usr/bin/env python3
"""Gera tests/fx_vectors.txt a partir do modelo de referencia: linhas 'op a b c expected...' em raw Q16."""
import random, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from fxmodel import *
random.seed(2024)
def r(scale): return random.randint(-scale * 65536, scale * 65536)
out = []
for _ in range(3000):
    a, b = r(300), r(300); out.append(f"mul {a} {b} {mul(a, b)}")
for _ in range(3000):
    a, b = r(3000), r(100)
    if b: out.append(f"div {a} {b} {div(a, b)}")
for _ in range(2000):
    a = r(3000); out.append(f"round {a} {rnd(a)}")
for _ in range(3000):
    x, y = r(2000), r(2000); out.append(f"vlen {x} {y} {vlen(x, y)}")
for _ in range(3000):
    x, y = r(300), r(300); nx, ny = norm(x, y); out.append(f"norm {x} {y} {nx} {ny}")
for s in ["0", "1", "-1", "0.5", "0.1", "0.3", "-2.5", "0.33333", "123.45678", "0.00000762939453125", "0.00002288818359375", "0.0000076293945313", "5.99991", "1.5708"]:
    out.append(f"parse {s} {parse(s)}")
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
print(len(out), 'vetores')
