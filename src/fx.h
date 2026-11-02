/* Ponto fixo Q48.16 do YOMI HUSTLE (a lib original FixedMath, em Rust, usa 16 bits fracionarios e raw de 64 bits).
 * Semantica confirmada contra a lib original (ver docs/FIXEDPOINT.md):
 *   parse decimal -> arredonda para o mais proximo (empate para par); mul -> piso (>>16); div -> trunca para zero;
 *   round -> meio para longe de zero; vec_len -> isqrt(raw) << 8; normalized_vec -> trunca; normalized_vec_times -> mul(n, k).
 * Trigonometria: usa libm e arredonda; a lib original erra ate ~2 LSB (sen/cos) e ~0,005 rad (atan) vs o exato. */
#ifndef FX_H
#define FX_H
#include <stdint.h>
#include <math.h>

typedef int64_t fx;
#define FX_SHIFT 16
#define FX_ONE   ((fx)1 << FX_SHIFT)
#define FX_HALF  (FX_ONE >> 1)

#define FX_INT(i)    ((fx)(i) * FX_ONE)
#define FX_RAW(r)    ((fx)(r))

static inline fx fx_add(fx a, fx b) { return a + b; }
static inline fx fx_sub(fx a, fx b) { return a - b; }
static inline fx fx_mul(fx a, fx b) { return (a * b) >> FX_SHIFT; }                 /* piso */
static inline fx fx_div(fx a, fx b) { return (a * FX_ONE) / b; }                    /* trunca para zero; b != 0 */
static inline fx fx_abs(fx a) { return a < 0 ? -a : a; }
static inline int fx_sign(fx a) { return (a > 0) - (a < 0); }
static inline fx fx_floor(fx a) { return a & ~(FX_ONE - 1); }                       /* parte inteira, ainda em Q16 */
static inline fx fx_ceil(fx a) { return fx_floor(a + FX_ONE - 1); }
static inline int64_t fx_round(fx a)                                                /* meio para longe de zero, devolve inteiro */
{
    return a >= 0 ? (a + FX_HALF) >> FX_SHIFT : -((-a + FX_HALF) >> FX_SHIFT);
}
static inline int64_t fx_to_int(fx a) { return a >> FX_SHIFT; }                     /* piso */
static inline fx fx_lerp(fx a, fx b, fx t) { return a + fx_mul(b - a, t); }          /* lerp_string */
static inline fx fx_powu(fx x, unsigned n)                                           /* multiplicacoes repetidas, com piso a cada passo */
{
    fx r = FX_ONE;
    while (n--) r = fx_mul(r, x);
    return r;
}

static inline uint64_t fx_isqrt64(uint64_t n)
{
    uint64_t r = 0, bit = (uint64_t)1 << 62;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
        bit >>= 2;
    }
    return r;
}

/* sqrt grosseira do original: isqrt do raw, deslocada 8 bits (resolucao 1/256 em raiz => multiplos de 256 raw) */
static inline fx fx_sqrt(fx a) { return a <= 0 ? 0 : (fx)(fx_isqrt64((uint64_t)a) << 8); }
static inline fx fx_vec_len_sq(fx x, fx y) { return fx_mul(x, x) + fx_mul(y, y); }
static inline fx fx_vec_len(fx x, fx y) { return (fx)(fx_isqrt64((uint64_t)((x * x + y * y) >> FX_SHIFT)) << 8); }
static inline fx fx_vec_dist(fx x1, fx y1, fx x2, fx y2) { return fx_vec_len(x2 - x1, y2 - y1); }

typedef struct { fx x, y; } fxv;
static inline fxv fxv_make(fx x, fx y) { fxv v = { x, y }; return v; }
static inline fxv fxv_add(fxv a, fxv b) { return fxv_make(a.x + b.x, a.y + b.y); }
static inline fxv fxv_sub(fxv a, fxv b) { return fxv_make(a.x - b.x, a.y - b.y); }
static inline fxv fxv_mul(fxv a, fx k) { return fxv_make(fx_mul(a.x, k), fx_mul(a.y, k)); }
static inline fxv fxv_div(fxv a, fx k) { return fxv_make(fx_div(a.x, k), fx_div(a.y, k)); }
static inline fx fxv_dot(fxv a, fxv b) { return fx_mul(a.x, b.x) + fx_mul(a.y, b.y); }
static inline fxv fxv_round(fxv a) { return fxv_make(FX_INT(fx_round(a.x)), FX_INT(fx_round(a.y))); }
static inline fxv fxv_norm(fxv a)
{
    fx l = fx_vec_len(a.x, a.y);
    if (l == 0) return fxv_make(0, 0);
    return fxv_make((a.x * FX_ONE) / l, (a.y * FX_ONE) / l);
}
static inline fxv fxv_norm_times(fxv a, fx k) { return fxv_mul(fxv_norm(a), k); }

/* angulos em radianos Q16 */
static inline fx fx_deg2rad(fx deg) { return (fx)floor((double)deg * 0.017453292519943295); }
static inline fxv fx_angle_to_vec(fx ang)
{
    double a = (double)ang / 65536.0;
    return fxv_make((fx)floor(cos(a) * 65536.0 + 0.5), (fx)floor(sin(a) * 65536.0 + 0.5));
}
static inline fx fx_vec_to_angle(fx x, fx y) { return (fx)floor(atan2((double)y, (double)x) * 65536.0 + 0.5); }
static inline fxv fx_rotate_vec(fxv v, fx ang)
{
    fxv cs = fx_angle_to_vec(ang);
    return fxv_make(fx_mul(v.x, cs.x) - fx_mul(v.y, cs.y), fx_mul(v.x, cs.y) + fx_mul(v.y, cs.x));
}

/* le "-12.345" etc. Arredonda para o mais proximo (empate para par), igual ao original. Aceita ate 18 casas. */
static inline fx fx_parse(const char *s)
{
    int neg = 0;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    int64_t ip = 0;
    while (*s >= '0' && *s <= '9') ip = ip * 10 + (*s++ - '0');
    uint64_t num = 0, den = 1; int digits = 0;
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9' && digits < 18) { num = num * 10 + (uint64_t)(*s++ - '0'); den *= 10; digits++; }
    }
    /* frac_raw = num * 65536 / den, arredondado para par; num*65536 cabe em 64 bits (num < 10^18 -> usa 128 bits) */
    unsigned __int128 n = (unsigned __int128)num * 65536u;
    uint64_t q = (uint64_t)(n / den), rem = (uint64_t)(n % den);
    if (rem * 2 > den || (rem * 2 == den && (q & 1))) q++;
    fx r = (fx)ip * FX_ONE + (fx)q;
    return neg ? -r : r;
}
#endif
