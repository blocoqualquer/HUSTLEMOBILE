# Física de objeto (substitui o `FGObject` nativo)

Implementação: `src/phys.h`. Modelo de referência em Python: `tools/physmodel.py`. Foi obtido por caixa-preta, rodando a lib
original no Godot 3.5.3 headless (`tools/oracle/phys_probe.gd`, `tools/physoracle.py`) e comparando com `tools/physfuzz.py`
(fuzzer diferencial: sequências aleatórias de operações, estado comparado a cada passo). Resultado final: ~99,5% das sequências
idênticas; as raras diferenças são de 1–3 LSB (≤5e-5) em `apply_fric` (provável arredondamento interno da lib).

Regras confirmadas (valores em Q48.16):
- `apply_force*`: no chão, a componente y positiva (para baixo) é descartada **na chamada**.
- `apply_grav`: só no ar e com `vy < max_fall`; soma `mul(grav, gmod)` à força.
- `apply_forces`: v += f; limita |vx| (limite do chão se a flag `grounded` *anterior* era verdadeira, senão do ar) e `vy` ≤ max_fall (no ar);
  move; se `py >= 0` → grounded, `vy` zera só se `vy > 0`, `py = 0` se snap; senão não-grounded.
- `apply_forces_no_limit`: grounded = (py ≥ 0) *antes* de mover; com snap zera vy>0 antes e prende py ≤ 0; sem snap move e depois zera vy>0.
- `apply_x/y_fric(f)`: só age se |v| > f; v = mul(v, 1 − f) (f ≥ 1 inverte o sinal).
- `apply_fric`: se |vx| > 0,0001: vx −= trunc(|vx|·fric/vmax) (fric/vmax do modo chão ou ar atual).
- `move_directly*`: soma à posição; com snap, py>0 vira 0. `*_relative` multiplica x por facing.
- `add_pushback(v)`: só soma se pushback < 10; `apply_pushback(dir)`: x += pushback·dir; pushback −= 0,55 (mínimo 0).
- `reset_momentum` zera velocidade **e** forças acumuladas. `position_x/y` = round (meio para longe de zero).

Testes: `python3 tools/gen_phys_vectors.py` gera `tests/phys_vectors.txt` (do modelo); `gcc tests/test_phys.c -lm` reproduz bit a bit em C.
