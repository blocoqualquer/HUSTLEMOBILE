#!/bin/sh
# regenera os dados C do Robo (personagem + projeteis)
ROOT=$(ls -d /tmp/claude-0/-home-claude/*/scratchpad/src/YOMIHUSTLE)
T=/tmp/claude-0/-home-claude/4c7c7c98-8b35-5ecd-bd2a-e80c8ecfedc5/scratchpad
cd "$(dirname "$0")/.."
python3 tools/gen_c_data.py /home/claude/yomi-psp/data/chars/robo.json robo src/gen/robo.c $ROOT
python3 tools/gen_projectiles.py $ROOT characters/robo robo $T/proj_robo
