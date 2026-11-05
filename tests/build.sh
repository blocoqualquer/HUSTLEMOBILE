#!/bin/sh
# uso: tests/build.sh <teste> -> /tmp/<teste>   (compila o motor completo + dados gerados do Ninja)
cd "$(dirname "$0")/.."
SRC="src/game.c src/fighter.c src/combat.c src/parry.c src/states_common.c src/states_basic.c src/states_misc.c src/states_proj.c src/proj_ninja.c src/states_ninja.c src/ninja.c src/engine.c src/hitstate.c src/hitbox.c src/projectile.c"
gcc -O1 -Wall -o /tmp/$1 tests/$1.c $SRC src/gen/ninja*.c -lm
