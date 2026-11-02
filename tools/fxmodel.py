"""Modelo de referencia (Python) da matematica do original, usado para gerar os vetores de teste do fx.h.
Confirmado contra a lib FixedMath real (Godot headless): parse = mais proximo/par; mul = piso; div = trunca; round = longe de zero;
vec_len = isqrt(raw)<<8; normalized_vec = trunca(x*65536/len)."""
from fractions import Fraction
import math
def rhe(fr):
    fl = math.floor(fr); d = fr - fl
    if d < Fraction(1, 2): return fl
    if d > Fraction(1, 2): return fl + 1
    return fl if fl % 2 == 0 else fl + 1
def parse(s): return rhe(Fraction(s) * 65536)
def mul(a, b): return (a * b) >> 16
def div(a, b): return int(Fraction(a * 65536, b)) if (a < 0) == (b < 0) or a == 0 else -int(Fraction(abs(a) * 65536, abs(b)))
def rnd(a): return (a + 32768) >> 16 if a >= 0 else -((-a + 32768) >> 16)
def vlen(x, y): return math.isqrt((x * x + y * y) >> 16) << 8
def norm(x, y):
    l = vlen(x, y)
    if l == 0: return 0, 0
    return int(Fraction(x * 65536, l)), int(Fraction(y * 65536, l))
