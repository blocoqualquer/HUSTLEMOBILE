#!/bin/sh
# regenera todos os dados C do Ninja (personagem + projeteis)
ROOT=$(ls -d /tmp/claude-0/-home-claude/*/scratchpad/src/YOMIHUSTLE)
T=/tmp/claude-0/-home-claude/4c7c7c98-8b35-5ecd-bd2a-e80c8ecfedc5/scratchpad
cd "$(dirname "$0")/.."
python3 tools/gen_c_data.py /home/claude/yomi-psp/data/chars/stickman.json ninja src/gen/ninja.c $ROOT
python3 tools/gen_projectiles.py $ROOT characters/stickman ninja $T/proj_ninja
