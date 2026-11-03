# Your Only Move Is HUSTLE → PSP: plano do port completo

Licença do código original: uso livre desde que se credite o criador original (colocar nos créditos do jogo e no README).

## 1. O que existe hoje (medido no zip)

- Godot 3.x, 53.125 linhas de GDScript em 501 arquivos, 330 cenas `.tscn`, 1.793 PNGs, 237 áudios.
- Núcleo da simulação: `game.gd` (1.867 linhas), `BaseChar.gd` (3.005), `BaseObj.gd` (1.314), `Hitbox.gd` + `SweptHitbox.gd` + `HitboxData.gd` (~1.050), `StateMachine.gd` (245), `CharState.gd` (600).
- Estados comuns: 33 arquivos, ~2.400 linhas (Idle, Jump, Dash, Parry, Grab, HurtAerial…).
- Personagens (linhas de GDScript): Ninja/stickman 2.140, Robo 3.037, Wizard 2.535, Mutant 1.962, Swordandgun 3.450. A pasta `alien` não tem scripts.
- Os golpes são nós dentro das `.tscn` (ex.: `NinjaGuy.tscn` tem 9.139 linhas e 265 recursos) com propriedades exportadas, mais um script por golpe com lógica própria.
- A matemática é determinística em ponto fixo, com valores escritos como string (`"0"`, `"6"`, `fixed.div(...)`).
- A lib nativa `tbfg.dll`/`tbfg.so` (Rust, crate `fixed`) veio sem código-fonte. **Confirmado rodando-a no Godot 3.5.3 headless:** o ponto fixo tem 16 bits fracionários (1/3 = 21845/65536, 0.1 = 6554/65536) e raw de 64 bits. Os números aparecem como string com até 5 casas. A raiz quadrada é aproximada (√2 = 92672/65536 em vez de 92682), então o port copia o resultado da lib em vez de usar a raiz exata.
- O Godot 3.5.3 headless roda aqui (precisa de `entry/Server.64` no `lib/lib.tres`). Isso dá um "oráculo": dá para gravar traces do jogo original e comparar com o port.

## 2. Alvo PSP

- Tela 480×272 (o jogo original é 640×360, então escala de 0,75 ou recorte).
- CPU MIPS 333 MHz, 32 MB de RAM (64 MB no slim), 2 MB de VRAM.
- Texturas em potência de 2, de preferência 8 bits com paleta (CLUT) e swizzle.
- Controles: direcional, analógico, 4 botões de ação, ombros L/R.

## 3. Arquitetura proposta

- **Motor em C99** com PSPSDK (`sceGu` para vídeo, `sceAudio` para som, `sceCtrl` para controle).
- **Pipeline de dados em Python** que roda no PC: lê as `.tscn`, as spriteframes e as `.import`, e gera binários compactos (golpes, hitboxes, frames, timings, paletas).
- **Lógica dos golpes**: parte vira dados interpretados (a maioria dos golpes só muda propriedades), parte é portada à mão para C (os scripts com comportamento próprio).
- **Simulação separada do render**, em ticks de 60 Hz, igual ao original. Isso preserva o determinismo.
- **Testes de equivalência**: se for possível rodar o Godot 3 headless aqui, gravo traces do original (posição, vida, estado por tick) e comparo com o port. Ainda não verifiquei se o download do Godot é permitido.

## 4. Fases

| Fase | Entrega | Status |
|---|---|---|
| 0 | Toolchain PSP instalado, hello world gera `EBOOT.PBP` | feito |
| 1 | Conversor de assets (PNG → atlas 8 bits swizzled, áudio → PCM 16-bit mono 22 kHz) | feito, testado no PPSSPP pelo usuário |
| 2 | Conversor de `.tscn` para dados | dados resolvidos e validados: as 6 cenas de personagem (herança de `BaseChar.tscn` + padrões dos `export var`) batem em 10.855 de 10.855 valores com o jogo original no Godot headless. Falta só o empacotamento binário, que depende do formato que o motor em C vai ler |
| 3 | Núcleo da simulação em C: ponto fixo, objetos, máquina de estados, hitbox/hurtbox, colisão, tick | em andamento: prontos `fx.h`, `phys.h`, máquina de estados e tick do objeto (`engine.c`), hitbox/hurtbox/hurtbox de membro (`hitbox.c`, `hitstate.c`) e dados dos 6 personagens gerados em C (`src/gen/`, 641 estados). Falta: `BaseChar`/Fighter (hit_by, parry, block, combo), resolução de colisão e loop do `game.gd` |
| 4 | Estados comuns e `BaseChar` (hit stun, combos, parry, throw, clash, burst, super) | |
| 5 | Renderer: sprites, câmera, stage, HUD, efeitos e partículas | |
| 6 | Áudio | |
| 7 | UI: menus, seleção de personagem, seletor de ação com direcional, DI | |
| 8 | Personagens, um por vez: Ninja → Robo → Wizard → Mutant → Swordandgun (com projéteis) | |
| 9 | CPU (o original é jogo de turnos em rede, então para o PSP precisa de IA ou de 2 jogadores no mesmo console) | |
| 10 | Otimização, memória, fidelidade e pacote final | |

## 5. Fora do escopo no PSP

Steam, lobby e multiplayer online, mod loader, chat, workshop, personalização de trails, compartilhamento de replays. O recurso "ghost" (prévia de 90 frames da jogada) é pesado e pode ficar opcional ou reduzido.

## 6. Riscos

- A fidelidade da simulação depende de ler e portar com cuidado centenas de comentários e casos especiais (há várias correções de ordem de resolução no código).
- Memória: 5 personagens com sprites e áudio precisam de streaming por personagem, sem carregar tudo de uma vez.
- Sem hardware ou emulador testado ainda. Estou tentando compilar o PPSSPP aqui para testar. Se não der, você valida os builds no seu PPSSPP ou no console.
