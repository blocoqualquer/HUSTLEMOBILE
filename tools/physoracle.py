import subprocess, os, json
G=os.environ.get('GODOT_ORACLE_DIR','.')  # pasta com o binario Godot 3.5.3 headless e proj/ contendo phys_probe.gd
def run(lines):
    open(f'{G}/ph_in.txt','w').write('\n'.join(lines)+'\n')
    env=dict(os.environ, PH_IN=f'{G}/ph_in.txt', PH_OUT=f'{G}/ph_out.txt')
    p=subprocess.run([f'{G}/Godot_v3.5.3-stable_linux_headless.64','--path',f'{G}/proj','-s','phys_probe.gd'],env=env,capture_output=True,text=True,timeout=300)
    return open(f'{G}/ph_out.txt').read(), p.stderr+p.stdout
