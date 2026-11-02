# YOMI HUSTLE PSP (port em andamento)

Port do **Your Only Move Is HUSTLE** (Godot 3) para PSP, em C com PSPSDK.

**Créditos:** o jogo e o código-fonte originais pertencem ao criador original de Your Only Move Is HUSTLE.
A licença do código original permite o uso em outros projetos desde que o criador original seja creditado.
Este repositório não contém assets do jogo: eles são gerados a partir do código-fonte original pelos conversores em `tools/`.

## Estado
- Prontas: toolchain, conversor de sprites e de áudio, visualizador de atlas no PSP, conversor das cenas dos personagens (validado contra o jogo original no Godot headless).
- A simulação do jogo ainda NÃO foi portada. Veja `PLANO_PORT_PSP.md`.

## Compilar
    export PSPDEV=/caminho/do/pspdev; export PATH=$PATH:$PSPDEV/bin
    psp-cmake -B build . && make -C build

## Gerar os dados (precisa do código-fonte original extraído; Python 3, Pillow, numpy, ffmpeg)
    python3 tools/convert_sprites.py <raiz_do_jogo> <personagem> data/gfx
    python3 tools/convert_audio.py   <raiz_do_jogo> data/snd
    python3 tools/convert_char.py    <raiz_do_jogo> characters/stickman/NinjaGuy.tscn data/chars/stickman.json
Personagens: stickman, robo, wizard, mutant, swordandgun. O Godot headless para comparar com o original está em `tools/oracle/README.md`.

## Testar
Copie `EBOOT.PBP` e a pasta `data/` para `PSP/GAME/YOMIHUSTLE` (ou abra no PPSSPP).
No visualizador: cima/baixo troca de personagem, esquerda/direita troca de página, L/R navega pelos sprites, O mostra um sprite, X mostra a página.
