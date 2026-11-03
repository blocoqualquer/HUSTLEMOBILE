"""Gera tests/phys_vectors.txt a partir do modelo (tools/physmodel.py): sequencias aleatorias de operacoes + estado esperado."""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from physfuzz import gen_seq, apply_model, INIT
from physmodel import Phys
OPS=['set_position','set_vel','apply_force','apply_force_str','apply_grav','apply_forces','apply_forces_no_limit','update_grounded','set_facing','apply_force_relative','move_directly','move_directly_str','apply_x_fric','apply_y_fric','reset_momentum','set_grounded','set_snap_to_ground','set_gravity_modifier','apply_grav_custom','apply_fric','add_pushback','apply_pushback','reset_pushback','move_directly_relative','move_directly_relative_str','apply_force_relative_str']
random.seed(7)
out=[]
for _ in range(400):
    m=Phys(); out.append('NEW')
    for l in INIT[1:]: apply_model(m,l)
    for l in gen_seq(30,OPS):
        apply_model(m,l); out.append(l[5:])
        out.append('S %d %d %d %d %d %d %d %d %d %d'%(m.px,m.py,m.vx,m.vy,m.fx,m.fy,m.grounded,m.facing,m.snap,m.pb))
open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','tests','phys_vectors.txt'),'w').write('\n'.join(out)+'\n')
