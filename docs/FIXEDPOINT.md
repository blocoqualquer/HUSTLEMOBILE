# Ponto fixo do YOMI HUSTLE

A lib original (`tbfg`, Rust, crate `fixed` 1.19) expõe `FixedMath` ao GDScript, e os números viajam como **strings**.
Rodando-a no Godot 3.5.3 headless (ver `tools/oracle/README.md`) e comparando milhares de entradas aleatórias, confirmei:

| Operação | Regra | Amostras testadas |
|---|---|---|
| representação | Q48.16: raw inteiro de 64 bits, 16 bits fracionários | |
| parse de decimal | mais próximo, empate para par | 6.000 strings |
| `mul` | piso de `a*b/65536` (shift aritmético) | 12.000, 1 divergência de 1 LSB |
| `div` | trunca para zero | 6.000, 4 divergências |
| `round` | meio para longe de zero | amostras manuais |
| `vec_len` / `vec_dist` | `isqrt((x²+y²)>>16) << 8` (resolução grossa de 256 raw) | 3.000, 0 divergências |
| `normalized_vec` | `trunc(x*65536 / len)` com o `len` acima | 2.000 aleatórios, 0 divergências |
| `normalized_vec_times` | `mul(normalized, k)` | amostras manuais |
| `lerp_string` | `a + mul(b-a, t)` | amostras manuais |
| `deg2rad` | `floor(raw * π/180)` | amostras manuais |

## Diferenças conhecidas (aceitas)
- **Trigonometria**: o original erra até ~2 LSB em seno/cosseno e até ~0,005 rad no atan. O port usa libm e arredonda; a diferença é imperceptível no jogo.
- **Impressão dos números**: a lib imprime o raw 39321 (0,59999) como `0.6`, o que não é o menor decimal que reconverte ao mesmo raw. Não afeta o port (que guarda o raw), mas faz o oráculo ter ruído de ±1 LSB ao comparar strings.
- `normalized_vec(3,4)` mostra `0.6`/`0.8`, mas `normalized_vec_times(3,4,10)` dá `5.99991`: o raw interno é truncado (39321), como no modelo.

## Testes
`python3 tools/gen_fx_vectors.py tests/fx_vectors.txt` gera 14 mil vetores a partir de `tools/fxmodel.py`;
`gcc -O2 -o test_fx tests/test_fx.c -lm && ./test_fx` confere `src/fx.h`.
