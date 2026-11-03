import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fxmodel import *
from fractions import Fraction
import math
ONE=65536
def P(s): return parse(str(s))
def rnd_away(a):  # raw -> int, meio para longe de zero
    return (a+32768)>>16 if a>=0 else -((-a+32768)>>16)
class Phys:
    MIN_VEL = parse("0.0001")
    def __init__(s):
        s.px=s.py=s.vx=s.vy=0; s.fx=s.fy=0; s.grav=0; s.gmod=ONE
        s.gf=s.af=s.mgs=s.mas=s.mfs=0
        s.grounded=True; s.facing=1; s.snap=True; s.pb=0
    # --- setters
    def set_gravity(s,v): s.grav=P(v)
    def set_ground_friction(s,v): s.gf=P(v)
    def set_air_friction(s,v): s.af=P(v)
    def set_max_ground_speed(s,v): s.mgs=P(v)
    def set_max_air_speed(s,v): s.mas=P(v)
    def set_max_fall_speed(s,v): s.mfs=P(v)
    def set_gravity_modifier(s,v): s.gmod=P(v)
    def set_position(s,x,y): s.px=int(x)*ONE; s.py=int(y)*ONE
    def set_position_str(s,x,y): s.px=P(x); s.py=P(y)
    def set_vel(s,x,y): s.vx=P(x); s.vy=P(y)
    def set_facing(s,f): s.facing=int(f)
    def set_grounded(s,b): s.grounded=(b in (True,'true'))
    def set_snap_to_ground(s,b): s.snap=(b in (True,'true'))
    def reset_momentum(s): s.vx=s.vy=0; s.fx=s.fy=0
    def update_grounded(s): s.grounded = s.py>=0
    # --- forces
    def _addf(s,x,y):
        if s.grounded and y>0: y=0
        s.fx+=x; s.fy+=y
    def apply_force(s,x,y): s._addf(int(x)*ONE,int(y)*ONE)
    def apply_force_str(s,x,y): s._addf(P(x),P(y))
    def apply_force_relative(s,x,y): s._addf(int(x)*ONE*s.facing,int(y)*ONE)
    def apply_force_relative_str(s,x,y): s._addf(P(x)*s.facing,P(y))
    def apply_grav(s):
        if not s.grounded and s.vy < s.mfs: s.fy += mul(s.grav, s.gmod)
    def apply_grav_custom(s,g,fall):
        if not s.grounded and s.vy < P(fall): s.fy += mul(P(g), s.gmod)
    def _limit(s):
        lim = s.mgs if s.grounded else s.mas
        if s.vx > lim: s.vx = lim
        if s.vx < -lim: s.vx = -lim
        if not s.grounded and s.vy > s.mfs: s.vy = s.mfs
    def _integrate(s):
        s.px += s.vx; s.py += s.vy
        if s.py >= 0:
            s.grounded=True
            if s.vy>0: s.vy=0
            if s.snap: s.py=0
        else: s.grounded=False
    def _take_forces(s):
        s.vx+=s.fx; s.vy+=s.fy; s.fx=s.fy=0
    def apply_forces(s):
        s._take_forces()
        s._limit(); s._integrate()
    def apply_forces_no_limit(s):
        s.vx+=s.fx; s.vy+=s.fy; s.fx=s.fy=0
        s.grounded = s.py>=0
        if s.snap:
            if s.grounded and s.vy>0: s.vy=0
            s.px += s.vx; s.py += s.vy
            if s.py > 0: s.py = 0
        else:
            s.px += s.vx; s.py += s.vy
            if s.grounded and s.vy>0: s.vy=0
    def apply_x_fric(s,f):
        if abs(s.vx) > P(f): s.vx = mul(s.vx, ONE-P(f))
    def apply_y_fric(s,f):
        if abs(s.vy) > P(f): s.vy = mul(s.vy, ONE-P(f))
    def apply_fric(s):
        if s.grounded: fr,m = s.gf,s.mgs
        else: fr,m = s.af,s.mas
        if abs(s.vx) > s.MIN_VEL:
            q=abs(s.vx)*fr//m; s.vx = s.vx - (q if s.vx>0 else -q)
    def _clampy(s):
        if s.snap and s.py > 0: s.py = 0
    def move_directly(s,x,y): s.px+=int(x)*ONE; s.py+=int(y)*ONE; s._clampy()
    def move_directly_str(s,x,y): s.px+=P(x); s.py+=P(y); s._clampy()
    def move_directly_relative(s,x,y): s.px+=int(x)*ONE*s.facing; s.py+=int(y)*ONE; s._clampy()
    def move_directly_relative_str(s,x,y): s.px+=P(x)*s.facing; s.py+=P(y); s._clampy()
    def add_pushback(s,v):
        if s.pb < 10*ONE: s.pb += P(v)
    def reset_pushback(s): s.pb=0
    def apply_pushback(s,d):
        s.px += s.pb*int(d)
        s.pb = max(s.pb - P("0.55"), 0)
    # --- leitura
    def snap_state(s):
        return dict(pos=(rnd_away(s.px),rnd_away(s.py)), vel=(s.vx,s.vy), g=s.grounded, f=s.facing)
