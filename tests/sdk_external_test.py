"""Configura/compila/ejecuta un consumidor con solo cabeceras públicas y librerías CPU declaradas."""
import argparse
import json
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
for name in ['root','build','core','scene','compiler','flags','link-flags']:
    p.add_argument('--'+name,required=True)
a=p.parse_args()
root=Path(a.root).resolve();build=Path(a.build).resolve()
commands=[['cmake','-S',str(root/'tests/sdk_consumer'),'-B',str(build),'-G','Ninja',
    '-DSDK_ROOT='+str(root),'-DCORE_LIBRARY='+a.core,'-DSCENE_LIBRARY='+a.scene,
    '-DCMAKE_CXX_COMPILER='+a.compiler,'-DCMAKE_CXX_FLAGS='+a.flags,'-DCMAKE_EXE_LINKER_FLAGS='+a.link_flags],
    ['cmake','--build',str(build),'-j','2']]
for command in commands:
    print('$',*command,flush=True);subprocess.run(command,check=True)
for entry in json.loads((build/'compile_commands.json').read_text()):
    command=entry['command']
    if str(root/'src') in command:raise RuntimeError('Consumer received internal include paths')
    print('PUBLIC COMPILE:',command,flush=True)
subprocess.run([str(build/('sdk_consumer.exe' if os.name=='nt' else 'sdk_consumer'))],check=True)
