"""Optional native X11 input test; sends events only to its own window/PID."""
import os
from pathlib import Path
import re
import subprocess
import sys
import time
binary=Path(sys.argv[1]).resolve();log=Path(sys.argv[2]).resolve()
env=os.environ.copy();env.pop('WAYLAND_DISPLAY',None);env['XDG_SESSION_TYPE']='x11'
def keytool(*args):return subprocess.check_output(['xdotool',*args],env=env,text=True,timeout=5).strip()
with log.open('w') as output:
    app=subprocess.Popen([str(binary),'--plaza'],env=env,stdout=output,stderr=output)
    try:
        window=None
        for _ in range(150):
            found=subprocess.run(['xdotool','search','--pid',str(app.pid),'--name','^EcoSphere'],env=env,capture_output=True,text=True,timeout=5)
            if found.returncode==0 and 'sRGB:' in log.read_text():window=found.stdout.splitlines()[0];break
            if app.poll() is not None:raise RuntimeError('App exited before test')
            time.sleep(.1)
        assert window is not None and keytool('getwindowpid',window)==str(app.pid)
        keytool('windowactivate','--sync',window)
        keytool('keydown','--window',window,'F5');time.sleep(1.1);keytool('keyup','--window',window,'F5');time.sleep(.15)
        for key in ['F3','F3','F4','F5','F5','F4','0','1','2','3','4','5','6','F5','0','6','Escape']:
            keytool('key','--window',window,key);time.sleep(.1)
        assert app.wait(timeout=10)==0
    finally:
        if app.poll() is None:app.terminate();app.wait(timeout=5)
text=log.read_text();rows=[line for line in text.splitlines() if line.startswith('Lighting state:')]
assert len(rows)==15, 'Possible repeat/wiring failure'
modes=[re.search(r'shading=([A-Z-]+)',line)[1] for line in rows]
assert modes[:6]==['PHONG','PHONG','BLINN-PHONG','PHONG','PHONG','BLINN-PHONG']
assert 'HUD=0' in rows[1] and 'HUD=1' in rows[4]
assert 'regression=1' in rows[12] and modes[12]=='PHONG'
assert 'regression=1' in rows[13] and modes[13]=='BLINN-PHONG'
assert 'FPS overlay: off' in text and 'FPS overlay: on' in text and 'Clean exit' in text
print('SHADING NATIVE PASS: held F5 once, hidden selection, F3/F4, 0 resets Blinn, 1-6 preserved, regression independent')
