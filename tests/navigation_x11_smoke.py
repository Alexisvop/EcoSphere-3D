"""Navegación GPU, recorrido y controles XTEST; solo ventana propia con PID/foco verificados."""
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


options=set(sys.argv[3:])
stress='--stress' in options
validation='--validation' in options
keyboard_only='--keyboard-only' in options
visitor='--visitor-audit' in options
record_before='--record-before' in options
if not stress and not keyboard_only:
    with (directory/'app.log').open('w') as log:
        command=[str(binary),'--validation-smoke' if validation else '--navigation-smoke',str(directory)]
        if '--no-vsync' in options:command.append('--benchmark-no-vsync')
        process=subprocess.Popen(command,cwd='/tmp',env=env,stdout=log,stderr=log)
        try:
            window=own_window(process);float_and_resize(process,window)
            if process.wait(timeout=100)!=0: raise RuntimeError('Navigation GPU smoke failed; see app.log')
        finally: cleanup(process)

if '--gpu-only' not in options:
    with (directory/'keyboard.log').open('w') as log:
        process=subprocess.Popen([str(binary),'--plaza'],cwd='/tmp',env=env,stdout=log,stderr=log)
        try:
            window=own_window(process);float_and_resize(process,window)
            def assert_focus():
                if run('xdotool','getwindowfocus').stdout.strip()!=window: raise RuntimeError('Own focus lost')
                if run('xdotool','getwindowpid',window).stdout.strip()!=str(process.pid): raise RuntimeError('Own PID lost')
            def key(name,seconds=0):
                run('xdotool','windowfocus','--sync',window);time.sleep(.15);assert_focus()
                if seconds:
                    run('xdotool','keydown',name)
                    try: time.sleep(seconds)
                    finally:
                        # Release only the key we injected, even when focus validation aborts the test.
                        try:assert_focus()
                        finally:run('xdotool','keyup',name)
                else: run('xdotool','key',name)
                time.sleep(.15)
            def output(): return (directory/'keyboard.log').read_text()
            def position():
                positions=re.findall(r'Camera state: position=([^ ]+)',output())
                if not positions: raise RuntimeError('No actual camera state')
                return tuple(map(float,positions[-1].split(',')))
            def move(axis,target,negative,positive):
                delta=target-position()[axis]
                if abs(delta)>.15: key(positive if delta>0 else negative,abs(delta)/3)
                if abs(position()[axis]-target)>.6: raise RuntimeError('Actual movement missed waypoint')
            def visitor_controls(zone):
                expected=[]
                with (Path(__file__).resolve().parents[1]/f'docs/assets/{zone.upper()}_INTERACTIONS.csv').open() as f:
                    expected=[r['instance_id'] for r in csv.DictReader(f)]
                start=len(output())
                for _ in expected:key('Tab')
                selected=set(re.findall(r'Content selection: '+zone+r'/([^\s]+)',output()[start:]))
                if selected!=set(expected):raise RuntimeError('Visitor cannot select all educational objects: '+zone)
                key('g');key('l',.15);key('x',.15);key('v',.15);key('g');key('BackSpace')
                key('p')
                if not record_before:
                    run('xdotool','windowmove',window,'20','20');assert_focus()
                    run('magick','import','-window',window,str(directory/f'visitor-{zone}-paused.png'))
                    if run('magick','identify','-format','%wx%h',str(directory/f'visitor-{zone}-paused.png')).stdout!='1280x720':raise RuntimeError('Paused HUD capture clipped')
                key('r');key('p');key('p')
                if not record_before:
                    for marker in (f'Content animations: {zone} playing=0',f'Content animations: {zone} restart',f'Content animations: {zone} playing=1'):
                        if marker not in output()[start:]:raise RuntimeError('Visitor animation control missing: '+marker)
                    if not all(marker in output()[start:] for marker in (' held=1',' held=0')):raise RuntimeError('Visitor take/drop state missing')
                print('VISITOR SELECTION PASS:',zone,len(expected),'objects; P/R '+('recorded before correction' if record_before else 'controls verified'),flush=True)
            def hud_resizes():
                # Window capture is distinguished from the application's glReadPixels evidence.
                for w,h in [(1280,720),(640,360),(320,240),(1280,720)]:
                    run('xdotool','windowsize',window,str(w),str(h))
                    run('xdotool','windowmove',window,'20','20');time.sleep(2.2)
                    geometry=run('xdotool','getwindowgeometry','--shell',window).stdout
                    if f'WIDTH={w}\n' not in geometry or f'HEIGHT={h}\n' not in geometry:raise RuntimeError('Own window resize failed')
                    assert_focus()
                    capture=directory/f'hud-window-{w}x{h}.png'
                    run('magick','import','-window',window,str(capture))
                    if run('magick','identify','-format','%wx%h',str(capture)).stdout!=f'{w}x{h}':raise RuntimeError('Own-window screenshot clipped by compositor')
                print('HUD WINDOW RESIZE PASS: 1280x720, 640x360, 320x240, restored 1280x720; own PID captures',flush=True)
            if '--hud-only' in options:
                hud_resizes();key('Escape')
                if process.wait(timeout=10)!=0:raise RuntimeError('HUD resize audit did not exit cleanly')
                print('HUD ONLY PASS: own window dimensions/captures checked',flush=True)
                sys.exit(0)
            key('Return',.8)
            if 'Zone active:' in output(): raise RuntimeError('Entered from outside portal')
            key('F2');key('F1')
            move(0,6,'a','d');move(2,-14.2,'w','s');move(0,0,'a','d');move(2,-17.2,'w','s')
            key('Return',.8)
            if output().count('Zone active: aquabio')!=1: raise RuntimeError('Enter failed or rebounded')
            if visitor:visitor_controls('aquabio');hud_resizes()
            if '--walk-aquabio' in options:
                start=position()
                if abs(start[1]-1.7)>.001 or abs(start[2]-13)>.1: raise RuntimeError('AquaBio entry camera mismatch')
                key('w',2.0)
                if abs(position()[2]-7)>.6 or abs(position()[1]-1.7)>.001: raise RuntimeError('First-person entry walk missed')
                key('w',6.0)
                if abs(position()[2]+11)>.8 or abs(position()[0])>.05: raise RuntimeError('Main path walk missed')
                key('s',8.0)
                if abs(position()[2]-13)>.9 or abs(position()[1]-1.7)>.001: raise RuntimeError('Main path return walk missed')
                print('AQUABIO WALK PASS: entry (0,1.7,13); 24 m forward/back by real keyboard at 3 m/s; height preserved',flush=True)

            key('h',.8)
            if output().count('Zone active: hub')!=1: raise RuntimeError('H failed or rebounded')
            if abs(position()[2]+14.2)>.01: raise RuntimeError('Return camera not restored')
            key('h')
            if output().count('Zone active: hub')!=1: raise RuntimeError('H reloaded hub')
            if '--walk-ecocycle' in options or visitor:
                move(0,14.2,'d','a');move(2,0,'s','w');move(0,17.2,'d','a')
                key('Return',.8)
                if output().count('Zone active: ecocycle')!=1:raise RuntimeError('EcoCycle Enter failed')
                if abs(position()[2]-13)>.1 or abs(position()[1]-1.7)>.001:raise RuntimeError('EcoCycle camera incorrect')
                if visitor:visitor_controls('ecocycle')
                key('w',2);key('s',2)
                if abs(position()[2]-13)>.6:raise RuntimeError('EcoCycle walk return failed')
                key('h',.8)
                if output().count('Zone active: hub')!=2 or abs(position()[0]-14.2)>.01:raise RuntimeError('EcoCycle H return failed')
                print('ECOCYCLE KEYBOARD PASS: hub Enter EcoCycle; first-person walk at 1.7 m; H restores hub',flush=True)
            if visitor:
                # Transit in free camera above the hub; this is not a pedestrian-clearance claim.
                key('F2');key('e',1.5);move(0,-17.2,'w','s');key('q',1.5);key('F1')
                key('Return',.8)
                if output().count('Zone active: ecoenergy')!=1:raise RuntimeError('EcoEnergy Enter failed')
                visitor_controls('ecoenergy');key('w',2);key('s',2);key('h',.8)
                if output().count('Zone active: hub')!=3:raise RuntimeError('EcoEnergy H return failed')
                print('ECOENERGY KEYBOARD PASS: entry, walk, all six selections, H return',flush=True)
            if stress:
                def memory():
                    status=Path('/proc')/str(process.pid)/'status'
                    return int(re.search(r'VmRSS:\s+(\d+)',status.read_text()).group(1))
                with (directory/'stress.csv').open('w') as data:
                    data.write('visit,rss_kib,returned_hub\n0,'+str(memory())+',true\n')
                    for visit in range(1,61):
                        move(2,-17.2,'s','w')
                        key('Return',.15);key('h',.15)
                        if output().count('Zone active: aquabio')!=visit+1 or output().count('Zone active: hub')!=visit+1:
                            raise RuntimeError('Repeated Enter/H visit missing or rebounded')
                        data.write(str(visit)+','+str(memory())+',true\n');data.flush()
                print('STRESS PASS: 60 further keyboard round trips, RSS samples saved',flush=True)
            key('w',.2);key('Left',.2);key('Escape')
            if process.wait(timeout=10)!=0: raise RuntimeError('ESC not clean')
        finally: cleanup(process)

if not stress and not keyboard_only:
    for text in ['NAVIGATION GPU PASS','Clean exit']:
        if text not in (directory/'app.log').read_text(): raise RuntimeError('Missing GPU result: '+text)
if '--gpu-only' not in options:
    for text in ['Camera mode: free','Camera mode: first-person','ESC: close requested','Clean exit']:
        if text not in (directory/'keyboard.log').read_text(): raise RuntimeError('Missing keyboard result: '+text)
print('NAVIGATION HARNESS PASS: '+('keyboard/resize and ESC' if keyboard_only else 'GPU only' if '--gpu-only' in options else 'GPU/keyboard controls and ESC')+('; integral validation' if validation else ''),flush=True)
