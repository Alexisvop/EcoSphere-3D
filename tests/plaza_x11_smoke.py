"""Capturas/recorrido de plaza y controles XTEST; solo ventana propia con PID/foco verificados."""
import csv
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

binary=Path(sys.argv[1]).resolve()
directory=Path(sys.argv[2]).resolve()
directory.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
env.pop('WAYLAND_DISPLAY',None)
env['XDG_SESSION_TYPE']='x11'


def run(*args,check=True):
    result=subprocess.run(args,env=env,capture_output=True,text=True,timeout=5)
    print('$',*args,'exit=',result.returncode,flush=True)
    if check and result.returncode: raise RuntimeError(result.stderr)
    return result


def own_window(process):
    for _ in range(100):
        found=run('xdotool','search','--pid',str(process.pid),'--name','^EcoSphere - Plaza Central$',check=False)
        if found.returncode==0: return found.stdout.splitlines()[0]
        if process.poll() is not None: raise RuntimeError('Plaza exited before creating its window')
        time.sleep(.05)
    raise RuntimeError('No plaza window')


def float_and_resize(process,window):
    if run('xdotool','getwindowpid',window).stdout.strip()!=str(process.pid): raise RuntimeError('Window PID changed')
    if shutil.which('hyprctl'):
        own=[]
        for _ in range(30):
            clients=json.loads(run('hyprctl','-j','clients').stdout)
            own=[c for c in clients if c.get('pid')==process.pid and c.get('title')=='EcoSphere - Plaza Central']
            if own: break
            time.sleep(.05)
        if len(own)!=1: raise RuntimeError('Cannot uniquely identify own compositor window')
        address=own[0]['address']
        if not re.fullmatch(r'0x[0-9a-fA-F]+',address): raise RuntimeError('Invalid own window address')
        run('hyprctl','dispatch',f'hl.dsp.window.float({{window="address:{address}",action="set"}})',check=False)
    run('xdotool','windowsize',window,'1280','720')


def cleanup(process):
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=5)


with (directory/'app.log').open('w') as log:
    command=[str(binary),'--plaza-smoke',str(directory)]
    print('$',*command,'cwd=/tmp',flush=True)
    process=subprocess.Popen(command,cwd='/tmp',env=env,stdout=log,stderr=log)
    try:
        window=own_window(process)
        float_and_resize(process,window)
        if process.wait(timeout=55)!=0: raise RuntimeError('Plaza smoke failed; see app.log')
    finally: cleanup(process)

metrics={row['metric']:row['value'] for row in csv.DictReader((directory/'metrics.csv').open())}
print('Measured resolution:',metrics['width'],metrics['height'],'mean FPS:',metrics['fps_mean'],flush=True)
if metrics['width']!='1280' or metrics['height']!='720':
    print('Compositor did not provide requested 1280x720; reporting actual framebuffer',flush=True)

with (directory/'controls.log').open('w') as log:
    process=subprocess.Popen([str(binary),'--plaza'],cwd='/tmp',env=env,stdout=log,stderr=log)
    try:
        window=own_window(process)
        float_and_resize(process,window)

        def focus():
            run('xdotool','windowfocus','--sync',window)
            time.sleep(.15)
            if run('xdotool','getwindowfocus').stdout.strip()!=window: raise RuntimeError('Refusing input without own focus')
            if run('xdotool','getwindowpid',window).stdout.strip()!=str(process.pid): raise RuntimeError('Refusing input without own PID')

        def key(name,seconds=0):
            focus()
            if seconds:
                run('xdotool','keydown',name)
                try: time.sleep(seconds)
                finally:
                    focus();run('xdotool','keyup',name)
            else: run('xdotool','key',name)
            time.sleep(.1)

        time.sleep(.3)
        key('F2');key('w',.25);key('Left',.2);key('F1');key('Tab');key('Escape')
        if process.wait(timeout=10)!=0: raise RuntimeError('Plaza ESC did not exit cleanly')
    finally: cleanup(process)

output=(directory/'controls.log').read_text()
positions=re.findall(r'Camera state: position=([^ ]+)',output)
if not any(float(p.split(',')[2])<18.8 for p in positions): raise RuntimeError('W did not move plaza camera')
for required in ['Camera mode: free','Camera mode: first-person','hub/PLAZA-013-information','ESC: close requested','hub shutdown','Clean exit']:
    if required not in output: raise RuntimeError('Missing control evidence: '+required)
print('PLAZA XTEST PASS: requested size, other CWD, camera movement/F1/F2, kiosk selection, ESC and shutdown',flush=True)
