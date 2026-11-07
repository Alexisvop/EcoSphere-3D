"""Opt-in Linux/X11 check; sends keys only to its own EcoSphere window."""
import os
from pathlib import Path
import re
import subprocess
import sys
import time

binary=Path(sys.argv[1]).resolve()
log=Path(sys.argv[2]).resolve()
env=os.environ.copy();env.pop('WAYLAND_DISPLAY',None);env['XDG_SESSION_TYPE']='x11'

def xdo(*args):
    return subprocess.check_output(['xdotool',*args],env=env,text=True,timeout=5).strip()

with log.open('w') as output:
    app=subprocess.Popen([str(binary),'--plaza'],env=env,stdout=output,stderr=output)
    try:
        window=None
        for _ in range(150):
            found=subprocess.run(['xdotool','search','--pid',str(app.pid),'--name','^EcoSphere'],env=env,capture_output=True,text=True,timeout=5)
            if found.returncode==0 and 'sRGB:' in log.read_text():
                window=found.stdout.splitlines()[0];break
            if app.poll() is not None:raise RuntimeError('Application exited before input test')
            time.sleep(.1)
        assert window is not None and xdo('getwindowpid',window)==str(app.pid)
        xdo('windowactivate','--sync',window)
        xdo('keydown','--window',window,'F4');time.sleep(1.1);xdo('keyup','--window',window,'F4');time.sleep(.15)
        for key in ['F4','F3','F3','1','2','3','4','5','0','6','0','6','Escape']:
            xdo('key','--window',window,key);time.sleep(.1)
        assert app.wait(timeout=10)==0
    finally:
        if app.poll() is None:app.terminate();app.wait(timeout=5)
text=log.read_text()
rows=[dict(re.findall(r'(direct|directional|point|reflection|skybox|regression|HUD)=(\d)',line)) for line in text.splitlines() if line.startswith('Lighting state:')]
assert len(rows)==11, 'Unexpected events (including possible autorepeat)'
assert rows[0]['HUD']=='0' and rows[1]['HUD']=='1'
fields=['direct','directional','point','reflection','skybox']
for step in range(5):
    assert all(rows[2+step][field]==('0' if index<=step else '1') for index,field in enumerate(fields)), 'Incorrect 1-5 mapping'
assert all(rows[7][field]=='1' for field in ['direct','directional','point','reflection','skybox'])
assert rows[8]['regression']=='1' and rows[9]['regression']=='1' and rows[10]['regression']=='0'
assert 'FPS overlay: off' in text and 'FPS overlay: on' in text and 'Clean exit' in text
print('NATIVE INPUT PASS: F4 hold/repeat; F3 independent; 1-5 actual settings; 0 resets; 6 independent; clean exit')
