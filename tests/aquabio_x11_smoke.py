"""AquaBio y regresiones GPU de 6B; X11 solo sobre ventanas propias verificadas por PID."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

binary=Path(sys.argv[1]).resolve();directory=Path(sys.argv[2]).resolve()
directory.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env.pop('WAYLAND_DISPLAY',None);env['XDG_SESSION_TYPE']='x11'
def run(*args):
    return subprocess.check_output(args,env=env,text=True,timeout=5).strip()

def smoke(destination,args,expected):
    destination.mkdir(parents=True,exist_ok=True)
    command=[str(binary),*args]
    with (destination/'app.log').open('w') as log:
        process=subprocess.Popen(command,cwd='/tmp',env=env,stdout=log,stderr=log)
        try:
            window=None
            for _ in range(100):
                found=subprocess.run(['xdotool','search','--pid',str(process.pid),'--name','^EcoSphere'],env=env,capture_output=True,text=True,timeout=5)
                if found.returncode==0:
                    window=found.stdout.splitlines()[0];break
                if process.poll() is not None:raise RuntimeError('Demo exited before creating its window')
                time.sleep(.05)
            if window is None:raise RuntimeError('Own window not found')
            if run('xdotool','getwindowpid',window)!=str(process.pid):raise RuntimeError('Own PID mismatch')
            if shutil.which('hyprctl'):
                clients=[]
                for _ in range(30):
                    clients=[c for c in json.loads(run('hyprctl','-j','clients')) if c.get('pid')==process.pid]
                    if clients:break
                    time.sleep(.05)
                if len(clients)!=1:raise RuntimeError('Own compositor window not unique')
                address=clients[0]['address']
                if not re.fullmatch(r'0x[0-9a-fA-F]+',address):raise RuntimeError('Invalid own address')
                run('hyprctl','dispatch',f'hl.dsp.window.float({{window="address:{address}",action="set"}})')
            run('xdotool','windowsize',window,'1280','720')
            if process.wait(timeout=100)!=0:raise RuntimeError('GPU smoke failed; see '+str(destination/'app.log'))
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=5)
    text=(destination/'app.log').read_text()
    for marker in (expected,'Clean exit'):
        if marker not in text:raise RuntimeError('Missing result: '+marker)
    print('GPU PASS:',*args,'cwd=/tmp; own PID',flush=True)

if '--regressions' in sys.argv[3:]:
    for module in ('ecoenergy','aquabio','ecocycle'):
        target=directory/('legacy-'+module)
        smoke(target,['--module',module,'--smoke',str(target/'frame.ppm')],'IMPORTED SMOKE PASS')
        target=directory/('content-'+module)
        smoke(target,['--module',module,'--content-smoke',str(target)],'CONTENT SMOKE PASS')
    for option,marker in (('lighting','LIGHTING'),('interaction','INTERACTION SMOKE PASS'),('plaza','PLAZA SMOKE PASS')):
        target=directory/option
        smoke(target,['--'+option+'-smoke',str(target)],marker)
    print('REGRESSIONS PASS: 9/9 GPU modes',flush=True)
else:
    installation='ecoenergy' if '--ecoenergy' in sys.argv[3:] else 'ecocycle' if '--ecocycle' in sys.argv[3:] else 'aquabio'
    command=['--'+installation+'-smoke',str(directory)]
    if '--no-vsync' in sys.argv[3:]:command.append('--benchmark-no-vsync')
    smoke(directory,command,installation.upper()+' SMOKE PASS')
    if 'framebuffer=1280x720' not in (directory/'app.log').read_text():raise RuntimeError('AquaBio framebuffer must be 1280x720')
    print(installation.upper()+' X11 PASS: controlled 600-frame benchmark separated from readback/video')
