"""Five original shared interpretation instruments; Blender 5.2 bpy/CLI, no downloads.

Reuse only primitive construction/export conventions, never existing model geometry.
Engine metres (+Y up) are converted by b() before glTF Y-up export.
"""
import contextlib
import hashlib
import io
import json
import math
import sys
from pathlib import Path
import bpy
import bmesh

sys.path.insert(0, str(Path(__file__).resolve().parent))
from blender_ecoenergy import Geometry, b
from blender_aquabio import material

ROOT = Path(__file__).resolve().parents[1]


def label(g, title, line, width, y=.30, z=.42):
    g.box((0, y, z), (width, .40, .065), 'dark')
    for text, dy, cap in [(title, .065, .11), (line, -.10, .085)]:
        size = min(cap, (width-.12)/max(1, len(text))/.82)
        g.text(text, (0, y+dy, z+.045), size, 'letters')


def create_model(n):
    g = Geometry()
    if n == 1:
        g.cylinder((0, .08, 0), .62, .16, 'ceramic', 24)
        for a in (0, math.tau/3, math.tau*2/3):
            g.tube([(.55*math.cos(a), .12, .55*math.sin(a)), (0, 1.05, 0)], .045, 'metal', 8)
        ring = Geometry(); ring.ring(.59, .66, -.035, .035, 'metal', 40, axis='Z')
        angle = math.radians(35)
        def tilt(x, y, z):
            return (x, 1.16+y*math.cos(angle)-z*math.sin(angle), y*math.sin(angle)+z*math.cos(angle))
        g.mesh([tilt(*v) for v in ring.vertices], ring.faces, 'metal')
        g.tube([tilt(0, 0, -.78), tilt(0, 0, .78)], .023, 'yellow', 8)
        for i in range(13):
            a = math.pi*i/12
            g.tube([tilt(.60*math.cos(a), .60*math.sin(a), .045),
                    tilt(.68*math.cos(a), .68*math.sin(a), .045)], .012, 'yellow', 6)
        label(g, 'RELOJ SOLAR', 'ESCALA ILUSTRATIVA', 1.5, z=.58)
    elif n == 2:
        for x in (-.60, .60):
            g.box((x, .06, 0), (.25, .12, .85), 'metal')
            g.box((x, 1.08, -.15), (.07, 2.10, .07), 'blue')
        g.box((0, 2.08, -.15), (1.45, .09, .09), 'metal')
        for x in (-.16, .16):g.cylinder((x, 1.82, -.12), .20, .055, 'blue', 20, axis='X')
        g.cylinder((0, 1.82, -.12), .12, .30, 'metal', 16, axis='X')
        g.tube([(0, 1.82, .04), (0, 1.18, .04)], .012, 'white', 6)
        g.cylinder((0, .77, .04), .41, .045, 'dark', 32, axis='Z')
        for sector in (0, 2):
            for i in range(8):
                a=(sector+i/8)*math.pi/2; c=(sector+(i+1)/8)*math.pi/2
                g.mesh([(0,.77,.065),(.405*math.cos(a),.77+.405*math.sin(a),.065),
                        (.405*math.cos(c),.77+.405*math.sin(c),.065)], [(0,1,2)], 'white')
        label(g, 'DISCO SECCHI', 'TRANSPARENCIA', 1.50, y=.27, z=.35)
    elif n == 3:
        g.cylinder((0, .055, 0), .48, .11, 'metal', 20)
        for x in (-.33, .33):g.box((x, .78, -.15), (.055, 1.48, .055), 'blue')
        g.cylinder((0, 1.30, 0), .035, .76, 'metal', 12, axis='X')
        # Open rear half of a hollow conical collector exposes the twin buckets.
        pts=[];steps=16
        for y,r in [(1.82,.12),(2.25,.49),(1.82,.09),(2.25,.45)]:
            pts += [(r*math.cos(math.pi+i*math.pi/steps),y,r*math.sin(math.pi+i*math.pi/steps)) for i in range(steps+1)]
        faces=[];s=steps+1
        for i in range(steps):
            faces.extend([(i,i+1,s+i+1,s+i),(2*s+i,3*s+i,3*s+i+1,2*s+i+1),
                          (s+i,s+i+1,3*s+i+1,3*s+i),(i,2*s+i,2*s+i+1,i+1)])
        faces += [(0,s,3*s,2*s),(steps,2*s+steps,3*s+steps,s+steps)]
        g.mesh(pts,faces,'blue')
        for x in (-.18,.18):
            g.box((x,1.38,0),(.29,.04,.38),'yellow')
            for z in (-.18,.18):g.box((x,1.47,z),(.29,.17,.035),'yellow')
        g.box((0,1.40,0),(.035,.30,.38),'metal')
        g.box((.40,1.48,-.14),(.10,.65,.05),'white')
        for y in (1.20,1.30,1.40,1.50,1.60,1.70):g.box((.40,y,-.10),(.08,.012,.02),'dark')
        label(g,'PLUVIOMETRO','MODELO ESTATICO',1.15,y=.29,z=.39)
    elif n == 4:
        xs=[-.78,-.39,0,.39,.78]
        levels=[[.10,.10,.10,.10,.10],[.47,.53,.44,.49,.45],
                [.98,1.04,.96,1.02,.97],[1.46,1.50,1.47,1.52,1.48]]
        for k,mat in enumerate(('sand','earth','wood')):
            outline=[(x,y) for x,y in zip(xs,levels[k])]+list(reversed(list(zip(xs,levels[k+1]))))
            v=[(x,y,z) for z in (-.20,.20) for x,y in outline];s=len(outline)
            f=[tuple(reversed(range(s))),tuple(range(s,2*s))]+[(i,(i+1)%s,(i+1)%s+s,i+s) for i in range(s)]
            g.mesh(v,f,mat)
        for x in (-.85,.85):g.box((x,.84,0),(.07,1.68,.48),'metal')
        g.box((0,.07,0),(1.8,.14,.70),'metal')
        for side in (-1,1):
            g.tube([(side*.25,1.53,.245),(side*.15,1.20,.245),(side*.32,.85,.245),(side*.22,.57,.245)],.023,'green',7)
            for k in range(3):
                y=1.23-k*.21;g.tube([(side*.20,y,.25),(side*(.45+k*.06),y-.18,.25)],.014,'green',6)
        for x,y in [(-.6,.3),(.45,.30),(.2,.69),(-.4,.75)]:g.sphere((x,y,.21),(.065,.045,.035),'ceramic',8,4)
        label(g,'SUELO VIVO','RAICES Y MATERIA',1.72,y=.27,z=.32)
    elif n == 5:
        g.box((0,.08,0),(1.9,.16,1.0),'metal')
        g.mesh([(-.27,.15,-.12),(.27,.15,-.12),(0,1.66,-.12),
                (-.27,.15,.12),(.27,.15,.12),(0,1.66,.12)],
               [(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],'blue')
        g.box((0,1.63,0),(1.85,.075,.08),'yellow')
        g.cylinder((0,1.65,0),.065,.30,'metal',12,axis='Z')
        for x in (-.65,.65):
            g.cylinder((x,.77,0),.29,.045,'metal',20)
            for dx,dz in [(-.22,-.12),(.22,-.12),(0,.24)]:
                g.tube([(x,1.62,0),(x+dx,.80,dz)],.012,'metal',6)
        g.box((-.65,.98,0),(.23,.37,.23),'wood')
        g.sphere((.65,.98,0),(.20,.20,.20),'ceramic',12,6)
        g.tube([(0,1.68,.19),(0,1.94,.19)],.015,'dark',6)
        label(g,'MASA Y VOLUMEN','EJEMPLO SIN PESAJE',1.90,y=.29,z=.45)
    else:
        raise ValueError('Expected shared model 1..5')
    return g


def build(destination):
    destination=Path(destination).resolve()
    assert not any(destination.rglob('*.glb')) and not any(destination.rglob('*.blend')), 'Use a new output directory'
    for folder in ('models','sources'):(destination/folder).mkdir(parents=True,exist_ok=True)
    plans=json.loads((ROOT/'docs/assets/SHARED_PLANNING.json').read_text())['models']
    old=bpy.context.window.scene;scene=bpy.data.scenes.new('EcoSphere_Shared_6D')
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    palette={k:material('Shared_'+k,c,metal,rough) for k,c,metal,rough in [
        ('metal',(.48,.55,.56),.72,.38),('ceramic',(.78,.80,.67),.05,.65),
        ('dark',(.018,.035,.04),.05,.8),('yellow',(.93,.61,.10),.1,.6),
        ('white',(.86,.88,.81),0,.8),('blue',(.035,.28,.39),.18,.5),
        ('earth',(.29,.15,.07),0,.95),('sand',(.62,.45,.24),0,.95),
        ('wood',(.39,.24,.10),0,.9),('green',(.12,.39,.15),0,.8)]}
    palette['letters']=material('Shared_letters',(.83,.98,.84),unlit=True)
    objects=[];report=[];logs=io.StringIO()
    try:
        bpy.context.window.scene=scene
        for n,plan in enumerate(plans,1):
            ident=plan['model_id'];g=create_model(n)
            assert bpy.data.objects.get(ident) is None,ident
            mesh=bpy.data.meshes.new(ident+'_Geometry');mesh.from_pydata([b(v) for v in g.vertices],[],g.faces)
            keys=list(dict.fromkeys(g.materials))
            for k in keys:mesh.materials.append(palette[k])
            for poly,k in zip(mesh.polygons,g.materials):poly.material_index=keys.index(k)
            uv=mesh.uv_layers.new(name='UVMap')
            for poly in mesh.polygons:
                for loop in poly.loop_indices:
                    p=mesh.vertices[mesh.loops[loop].vertex_index].co;uv.data[loop].uv=(p.x*.5,(p.y+p.z)*.5)
            bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
            assert all(f.calc_area()>1e-9 for f in bm.faces),ident
            bm.to_mesh(mesh);bm.free();mesh.update();mesh.calc_loop_triangles()
            obj=bpy.data.objects.new(ident,mesh);scene.collection.objects.link(obj);objects.append(obj)
            obj['ecosphere_model_id']=ident;obj['educational_function']=plan['purpose'];obj['origin']='ground centre; engine +Y up'
            bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
            target=destination/'models'/(ident+'.glb')
            with contextlib.redirect_stdout(logs):
                bpy.ops.export_scene.gltf(filepath=str(target),export_format='GLB',use_selection=True,use_active_scene=True,
                    export_yup=True,export_normals=True,export_texcoords=True,export_animations=False,
                    export_cameras=False,export_lights=False,export_draco_mesh_compression_enable=False,
                    export_tangents=False,export_attributes=False)
            raw=target.read_bytes();length=int.from_bytes(raw[12:16],'little');doc=json.loads(raw[20:20+length])
            assert set(doc.get('extensionsUsed',[]))<={'KHR_materials_unlit'}
            lo=[min(v[a] for v in g.vertices) for a in range(3)];hi=[max(v[a] for v in g.vertices) for a in range(3)]
            dims=[hi[a]-lo[a] for a in range(3)]
            assert all(d<=limit+.001 for d,limit in zip(dims,plan['reservation_m'])),(ident,dims)
            shape=sorted(set(tuple(round((v[a]-lo[a])/dims[a],5) for a in range(3)) for v in g.vertices))
            report.append(dict(model_id=ident,name=plan['name'],educational_function=plan['purpose'],dimensions_m=dims,
                bounds=[lo,hi],triangles=len(mesh.loop_triangles),materials=keys,textures=0,bytes=len(raw),
                sha256=hashlib.sha256(raw).hexdigest(),geometry_signature=hashlib.sha256(repr(shape).encode()).hexdigest()))
            print(ident,len(mesh.loop_triangles),'triangles',len(raw),'bytes',flush=True)
        assert len({m['geometry_signature'] for m in report})==5
        for i,o in enumerate(objects):o.location=(i*3.0,0,0)
        bpy.data.libraries.write(str(destination/'sources/shared.blend'),{scene},compress=True)
        (destination/'export-report.json').write_text(json.dumps({'blender':bpy.app.version_string,'production':'bpy/CLI','models':report},ensure_ascii=False,indent=2)+'\n')
        (destination/'export.log').write_text(logs.getvalue().rstrip()+'\n')
        print('SHARED EXPORT PASS: five original metric designs; no external assets/textures',flush=True)
    finally:
        bpy.context.window.scene=old


if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    if len(args)!=1:raise SystemExit('Usage: blender --background --python tools/blender_shared.py -- new_output_directory')
    build(args[0])
