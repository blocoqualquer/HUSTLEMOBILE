import sys, os, random, json; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from physoracle import run
from physmodel import *
INIT=["new","call set_gravity 0.8","call set_ground_friction 2.5","call set_air_friction 0.2","call set_max_ground_speed 15","call set_max_air_speed 10","call set_max_fall_speed 15"]
def rs(): 
    v=random.randint(-300,300)/random.choice([1,10,100]); return f"{v:.5f}".rstrip('0').rstrip('.')
def gen_seq(n, ops):
    L=[]
    for _ in range(n):
        op=random.choice(ops)
        if op=='set_position': L.append(f"call set_position i{random.randint(-300,300)} i{random.randint(-300,0)}")
        elif op=='set_vel': L.append(f"call set_vel {rs()} {rs()}")
        elif op in('apply_force','move_directly','apply_force_relative','move_directly_relative'): L.append(f"call {op} i{random.randint(-12,12)} i{random.randint(-12,12)}")
        elif op in('apply_force_str','move_directly_str','apply_force_relative_str','move_directly_relative_str'): L.append(f"call {op} {rs()} {rs()}")
        elif op=='set_facing': L.append(f"call set_facing i{random.choice([-1,1])}")
        elif op=='set_gravity_modifier': L.append(f"call set_gravity_modifier {random.choice(['1','0.5','2','1.5','0.25'])}")
        elif op in('apply_x_fric','apply_y_fric'): L.append(f"call {op} {random.choice(['0.1','0.5','0.25','1','2'])}")
        elif op=='add_pushback': L.append(f"call add_pushback {random.randint(0,12)}")
        elif op=='apply_pushback': L.append(f"call apply_pushback i{random.choice([-1,1])}")
        elif op=='set_grounded': L.append(f"call set_grounded {random.choice(['true','false'])}")
        elif op=='set_snap_to_ground': L.append(f"call set_snap_to_ground {random.choice(['true','false','true','true'])}")
        elif op=='apply_grav_custom': L.append(f"call apply_grav_custom {random.choice(['0.5','1','3'])} {random.choice(['5','10','20'])}")
        else: L.append(f"call {op}")
    return L
def apply_model(m, line):
    p=line.split(); name=p[1]; args=[a[1:] if a.startswith('i') and a[1:].lstrip('-').isdigit() else a for a in p[2:]]
    getattr(m,name)(*args)
def compare(ops, nseq=60, nlen=25, seed=1, verbose=3):
    random.seed(seed); seqs=[gen_seq(nlen,ops) for _ in range(nseq)]
    L=[]
    for sq in seqs: L+=INIT+sq
    out,log=run(L)
    # separar por NEW
    blocks=[]; cur=None
    for l in out.split('\n'):
        if l=='NEW': cur=[]; blocks.append(cur)
        elif l.startswith('DATA') and cur is not None: cur.append(json.loads(l[5:])['object_data'])
    bad=0; shown=0; steps=0
    for sq,blk in zip(seqs,blocks):
        m=Phys(); 
        for line in INIT[1:]: apply_model(m,line)
        # blk tem 1 DATA por 'call'(INIT[1:] + sq)
        for i,line in enumerate(sq):
            apply_model(m,line); steps+=1
            d=blk[len(INIT)-1+i]; ms=m.snap_state()
            gv=tuple(Fraction(d[k])*65536 for k in ('vel_x','vel_y'))
            got=dict(pos=(d['position_x'],d['position_y']),vel=gv,g=d['grounded'],f=d['facing'])
            okv=all(abs(a-b)<Fraction(25,10) for a,b in zip(gv,ms['vel']))
            if not (okv and got['pos']==ms['pos'] and got['g']==ms['g'] and got['f']==ms['f']):
                bad+=1
                if shown<verbose:
                    shown+=1; print('DIVERGE apos:',line); print('  oraculo',got); print('  modelo ',ms)
                    print('  historico:', sq[max(0,i-9):i])
                break
    print(f'{len(seqs)} sequencias, {bad} divergiram ({steps} passos)')
    return bad
