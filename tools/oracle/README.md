# Oráculo: rodar o jogo original no Godot 3.5.3 headless

1. Baixe `Godot_v3.5.3-stable_linux_headless.64` (github.com/godotengine/godot/releases/tag/3.5.3-stable).
2. Copie o projeto original para uma pasta de trabalho e ajuste a cópia:
   - `lib/lib.tres`: adicione `entry/Server.64 = "res://lib/tbfg.so"` (a lib nativa só tem entradas Windows/X11).
   - crie `stubs/Steam.gd` (`extends Node`) e registre `Steam="*res://stubs/Steam.gd"` em `[autoload]` (o GodotSteam não existe no Godot oficial).
3. Importe os assets: `godot --path <proj> --editor` SEM `--quit` (com `--quit` não importa nada); espere a pasta `.import` encher (~4 mil arquivos, 1-2 min) e mate o processo.
4. Rode scripts: `DUMP_SCENE=res://characters/stickman/NinjaGuy.tscn DUMP_OUT=/tmp/d.json godot --path <proj> -s dump.gd`.
5. `tools/compare_oracle.py <pasta_dos_dumps> stickman robo ...` compara com `data/chars/*.json`.
