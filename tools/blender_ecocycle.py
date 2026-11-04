"""EcoCycle: 24 geometrías propias. Reproduce en una salida nueva con Blender bpy.

Usa primitivas, tipografía y exportación compatibles ya revisadas; no reutiliza modelos.
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

sys.path.insert(0,str(Path(__file__).resolve().parent))
from blender_ecoenergy import Geometry, b
from blender_aquabio import board, material
from plan_ecocycle import NAMES, PANELS

PURPOSES=[
 ('infraestructura','Organiza cuatro etapas sobre 48x48 m; sin colisiones fisicas'),
 ('infraestructura','Entrada a reducir reparar reutilizar y reciclar'),
 ('infraestructura','Circulacion con losas y guias de material recuperado'),
 ('educacion','Priorizar reducir y reparar; reciclar depende de diseno y sistemas locales'),
 ('clasificacion','Separar papel plastico vidrio y metales; verificar aceptacion local'),
 ('clasificacion','Dosificar residuos mediante vibracion ilustrativa; no calcula transporte'),
 ('procesamiento','Trasladar materiales y sostener rodillo y tambor ferroso'),
 ('procesamiento','Giro de rodillo; la banda no simula flujo de residuos'),
 ('clasificacion','Barrido de sensor conceptual; no identifica materiales opticamente'),
 ('clasificacion','Desvio cinematico por compuerta; no clasifica objetos reales'),
 ('procesamiento','Atraccion magnetica ilustrada solo para materiales ferromagneticos'),
 ('papel','Compactar papel separado para acopio; no elimina contaminacion'),
 ('papel','Carrera del embolo de compactacion ilustrativa sin fuerza calculada'),
 ('papel','Fardo recuperado con fibras y flejes; la calidad depende de limpieza'),
 ('plastico','Reduccion de tamano; separar polimeros y proteger al operador'),
 ('plastico','Rotor dentado de trituracion; no fragmenta geometria'),
 ('plastico','Lavado requiere tratamiento del agua usada; sin eficiencia inventada'),
 ('plastico','Conformado mediante calor y husillo; sin temperatura o caudal simulado'),
 ('plastico','Filamento recuperado sujeto a calidad y compatibilidad de polimero'),
 ('vidrio','Reduccion mecanica de vidrio clasificado; evitar mezclas incompatibles'),
 ('vidrio','Fragmentos recuperados; contaminantes afectan calidad del material'),
 ('reutilizacion','Compostaje de organicos necesita aire humedad y manejo; no tiempo simulado'),
 ('reutilizacion','Reparacion prolonga uso; evaluar seguridad y piezas recuperables'),
 ('reutilizacion','Mueble de listones reutilizados; valorar integridad antes de usar')]
CATALOG=[(name,category,purpose) for name,(category,purpose) in zip(NAMES,PURPOSES)]


def create_model(n):
    g=Geometry()
    if n==1:
        g.box((0,-.22,0),(48,.44,48),'ground')
        for x in (-23.65,23.65):g.box((x,.035,0),(.22,.07,47.3),'green')
        for z in (-23.65,23.65):g.box((0,.035,z),(47.3,.07,.22),'orange')
        for x in (-20,-12,-4,4,12,20):
            for z in (-20,-4,12):g.box((x,.008,z),(1.1,.016,.5),'stone')
        for x in (-22,22):
            for z in (-14,2,18):g.box((x,.035,z),(.6,.07,4),'yellow')
    elif n==2:
        for x in (-2.8,2.8):
            g.box((x,1.75,0),(.4,3.5,.6),'green');g.box((x,.13,0),(.8,.26,.9),'steel')
        g.box((0,3.5,0),(6.2,.5,.6),'blue')
        g.text('ECOCYCLE',(0,3.51,.32),.34,'letters')
        for a in (0,2.1,4.2):
            g.box((.6*math.cos(a),2.7+.6*math.sin(a),0),(.6,.16,.18),'yellow',(0,0,a+.5))
    elif n==3:
        for z in (-4,-2,0,2,4):
            g.box((0,.028,z),(3.8,.056,1.94),'stone')
            for x in (-1.1,0,1.1):g.box((x,.061,z),(.5,.014,.12),'green')
        for x in (-1.99,1.99):g.box((x,.04,0),(.18,.08,10),'orange')
    elif n==4:
        for x in (-1.55,1.55):g.box((x,1.3,-.3),(.14,2.6,.14),'wood')
        g.box((0,2.65,-.2),(3.8,.18,1.7),'green')
        g.box((0,.12,-.2),(3.5,.24,1.3),'stone')
        g.ring(.48,.63,-.04,.04,'orange',18,center=(0,2,-.35),axis='Z')
    elif n==5:
        for i,mat in enumerate(('paper','blue','glass','yellow')):
            x=-1.35+i*.9
            g.box((x,.7,0),(.75,1.3,1.1),mat)
            g.box((x,1.38,0),(.8,.12,1.16),'charcoal')
            g.box((x,1.45,.08),(.42,.02,.32),'steel')
            g.box((x,.68,.57),(.40,.25,.025),'white')
        g.box((0,.06,0),(3.7,.12,1.4),'stone')
    elif n==6:
        for x in (-1.3,1.3):
            for z in (-1.2,1.2):
                g.box((x,.65,z),(.12,1.3,.12),'steel')
                for y in (.45,.55,.65):g.ring(.08,.12,y,y+.03,'yellow',8,(x,0,z))
        g.box((0,1.35,0),(3,.18,3.4),'orange')
        for x in (-1.4,1.4):g.box((x,1.8,.4),(.13,.9,2.5),'steel')
        for x in (-1.1,1.1):g.box((x,2.38,0),(.12,.9,.12),'blue')
        g.box((0,2.78,0),(2.35,.12,.18),'blue')
        for x in (-.7,0,.7):g.box((x,1.48,.5),(.5,.12,.4),'paper')
    elif n==7:
        g.box((0,1.23,0),(2.8,.12,4.8),'charcoal')
        for x in (-1.55,1.55):
            g.box((x,1,0),(.18,.4,5.1),'blue')
            for z in (-2,2):g.box((x,.65,z),(.2,1.3,.2),'steel')
            g.box((x,1.6,-2),(.15,.8,.3),'yellow')
        for z in (-1.2,0,1.2):g.box((0,1.31,z),(2.7,.03,.07),'green')
    elif n==8:
        g.cylinder((0,0,0),.22,2.6,'steel',16,axis='X')
        for x in (-1.4,1.4):g.cylinder((x,0,0),.08,.28,'blue',10,axis='X')
        for i in range(10):
            a=i*math.tau/10
            g.box((0,.235*math.cos(a),.235*math.sin(a)),(2.3,.06,.06),'orange',(a,0,0))
    elif n==9:
        g.box((0,0,0),(1.05,.4,.55),'blue')
        for x in (-.3,0,.3):
            g.cylinder((x,-.23,.06),.1,.12,'charcoal',10)
            g.cylinder((x,-.30,.06),.065,.02,'lens',10)
        g.cylinder((0,.26,0),.1,.14,'steel',10)
    elif n==10:
        g.cylinder((0,0,0),.07,.65,'steel',10)
        g.box((.58,0,0),(1.16,.6,.10),'yellow')
        for x in (.2,.6,1):g.box((x,.03,.06),(.12,.45,.03),'orange',(0,0,.3))
    elif n==11:
        g.cylinder((0,0,0),.6,2.6,'steel',20,axis='X')
        for a in (0,math.pi/2,math.pi,math.pi*1.5):
            g.box((0,.59*math.cos(a),.59*math.sin(a)),(2.45,.12,.15),'yellow',(a,0,0))
        for x in (-1.42,1.42):g.cylinder((x,0,0),.12,.25,'blue',10,axis='X')
        g.box((.5,.6,0),(.3,.14,.18),'orange')
    elif n==12:
        g.box((0,.12,0),(3.6,.24,2.8),'stone')
        for x in (-1.4,1.4):g.box((x,1.9,0),(.28,3.6,1.9),'green')
        g.box((0,3.6,0),(3.4,.30,2),'steel')
        g.box((0,.6,0),(2.3,.2,1.8),'orange')
        g.cylinder((0,3.25,0),.18,.4,'steel',12)
    elif n==13:
        g.box((0,-.1,0),(2,.25,1.6),'steel')
        g.box((0,-.25,0),(1.9,.06,1.5),'orange')
        g.cylinder((0,.30,0),.11,.6,'charcoal',12)
    elif n==14:
        g.box((0,.5,0),(1.4,1,1.1),'paper')
        for y in (.15,.35,.65,.85):g.box((0,y,.56),(1.35,.025,.025),'wood')
        for x in (-.4,.4):
            g.box((x,.5,.57),(.06,1.02,.035),'blue')
            g.box((x,1.02,0),(.06,.035,1.15),'blue')
    elif n==15:
        g.box((0,.14,0),(3,.28,2.4),'steel')
        for x in (-1.25,1.25):g.box((x,1.5,0),(.25,2.6,1.8),'orange')
        g.box((0,1.1,-.85),(2.6,1.5,.13),'blue')
        for z in (-.75,.75):g.box((0,2.55,z),(2.7,.65,.14),'yellow',(0,0,0))
        g.box((0,.6,.8),(1.7,.12,1.1),'charcoal',(.18,0,0))
    elif n==16:
        g.cylinder((0,0,0),.20,1.8,'steel',12,axis='X')
        for i,x in enumerate((-.65,-.2,.25,.7)):
            for j in range(6):
                a=j*math.tau/6+i*.4
                g.box((x,.34*math.cos(a),.34*math.sin(a)),(.18,.42,.15),'charcoal',(a,0,0))
        for x in (-1,1):g.cylinder((x,0,0),.10,.3,'blue',10,axis='X')
    elif n==17:
        g.box((0,.1,0),(3.4,.2,2.8),'stone')
        for x in (-1.45,1.45):g.box((x,.85,0),(.15,1.5,2.4),'steel')
        for z in (-1.15,1.15):g.box((0,.85,z),(2.9,1.5,.15),'green')
        g.box((0,1.05,0),(2.7,.04,2.1),'water')
        for i in range(6):g.box((-.9+i*.35,1.14,(-1)**i*.5),(.25,.10,.28),'blue',(0,i*.5,0))
        g.tube([(-1.6,.2,-.9),(-1.6,1.8,-.9),(-.7,1.8,-.9)],.07,'steel',8)
    elif n==18:
        g.box((0,.18,0),(5.3,.36,2),'green')
        for x in (-1.8,1.8):g.box((x,.65,0),(.45,.9,1.3),'steel')
        shell=Geometry();shell.arc_shell(.35,.48,3.7,0,math.pi,'orange',16)
        g.mesh([(z,y+1.25,-x) for x,y,z in shell.vertices],shell.faces,'orange')
        # Sección de husillo horizontal, representación estática de conformado.
        g.cylinder((0,1.25,0),.16,4.3,'steel',16,axis='X')
        for i in range(9):
            x=-1.6+i*.4;g.box((x,1.25,0),(.12,.50,.45),'yellow',(i*.4,0,0))
        g.cylinder((-1.4,2.05,0),.55,.8,'blue',12,top_radius=.85)
        g.cylinder((2.3,1.25,0),.22,.35,'steel',12,top_radius=.08,axis='X')
    elif n==19:
        for z in (-.48,.48):g.cylinder((0,.85,z),.78,.12,'orange',20,axis='Z')
        g.cylinder((0,.85,0),.48,.9,'green',20,axis='Z')
        for z in (-.3,-.15,0,.15,.3):g.ring(.46,.51,z,z+.04,'blue',20,(0,.85,0),'Z')
        g.box((0,.08,0),(1.55,.16,1.3),'stone')
    elif n==20:
        g.box((0,.18,0),(2.9,.36,2.4),'blue')
        for x in (-1.1,1.1):g.box((x,1.2,0),(.2,2.2,1.8),'steel')
        g.cylinder((0,2.05,0),.65,.8,'green',10,top_radius=1)
        g.box((0,.95,-.6),(2.1,1,.2),'orange')
        for x in (-.4,.4):g.cylinder((x,1.1,0),.32,1.2,'charcoal',10,axis='Z')
        g.box((0,.55,.7),(1.4,.1,1.4),'yellow',(.2,0,0))
    elif n==21:
        g.box((0,.1,0),(2.7,.2,2),'wood')
        for z in (-.85,.85):g.box((0,.6,z),(2.7,1,.1),'steel')
        for x in (-1.3,1.3):g.box((x,.6,0),(.1,1,1.7),'steel')
        for i in range(18):
            x=-1+(i%6)*.39;z=-.55+(i//6)*.5
            g.box((x,.35+(i%3)*.13,z),(.32,.18,.29),'glass',(i*.3,i*.7,.4))
    elif n==22:
        g.cylinder((0,.12,0),1.2,.24,'stone',20)
        g.cylinder((0,.96,0),.98,1.5,'wood',16)
        for i in range(12):
            a=i*math.tau/12;g.box((1.02*math.cos(a),1,1.02*math.sin(a)),(.14,1.35,.18),'green',(0,-a,0))
        for y in (.35,1.6):g.ring(.97,1.11,y,y+.10,'steel',24)
        g.cylinder((0,1.82,0),.95,.2,'green',16,top_radius=.7)
        g.box((.8,1.82,0),(.3,.17,.15),'orange')
    elif n==23:
        g.box((0,1.05,0),(3.4,.18,1.6),'wood')
        for x in (-1.4,1.4):
            for z in (-.55,.55):g.box((x,.5,z),(.16,1,.16),'green')
        g.box((0,1.7,-.7),(3.3,1.25,.12),'wood')
        for x in (-1,-.3,.4,1.1):
            g.box((x,1.6,-.55),(.07,.5,.10),'steel',(0,0,.2));g.box((x,1.85,-.55),(.25,.08,.1),'orange')
        g.box((.7,1.2,.15),(.55,.22,.3),'steel');g.cylinder((.7,1.4,.15),.08,.3,'yellow',10)
    elif n==24:
        for x in (-.6,.6):
            for z in (-.6,.6):g.box((x,.5,z),(.12,1,.12),'steel')
        for z in (-.55,-.2,.15,.5):g.box((0,1.02,z),(1.5,.12,.25),'wood')
        for x in (-.62,.62):g.box((x,1.55,-.6),(.13,1,.13),'green')
        for y in (1.4,1.75,2.0):g.box((0,y,-.65),(1.55,.16,.15),'wood')
        g.box((.5,1.72,-.55),(.23,.15,.06),'yellow')
    else:raise ValueError(n)
    if n in PANELS:
        center,title,lines=PANELS[n];board(g,title,lines,center,3.8)
    return g


def build(destination):
    destination = Path(destination).resolve()
    assert not any(destination.rglob('*.glb')) and not any(destination.rglob('*.blend')), 'Usar salida nueva'
    for folder in ('models', 'textures', 'sources'):
        (destination / folder).mkdir(parents=True, exist_ok=True)
    original = bpy.context.window.scene
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    scene = bpy.data.scenes.new('EcoSphere_EcoCycle_6C')
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    specs={
        'ground':((.40,.44,.35),0,.95),'stone':((.65,.66,.59),0,.92),
        'green':((.10,.42,.16),.05,.6),'turquoise':((.06,.38,.42),.1,.5),'orange':((.93,.30,.055),.1,.55),
        'yellow':((.96,.69,.10),.1,.5),'blue':((.04,.22,.40),.15,.5),
        'steel':((.60,.64,.65),.82,.3),'wood':((.42,.25,.10),0,.9),
        'charcoal':((.035,.045,.05),.05,.9),'paper':((.73,.70,.53),0,.95),
        'glass':((.22,.57,.40),.10,.33),'white':((.83,.86,.80),0,.7),
        'water':((.055,.31,.37),.12,.3)}
    palette={k:material(k,*v) for k,v in specs.items()}
    palette['signboard']=material('signboard',(.012,.032,.042),unlit=True)
    palette['letters']=material('letters',(.78,.98,.83),unlit=True)
    palette['lens']=material('lens',(.12,.8,.35),unlit=True)
    for key,m in palette.items():m.name='Cycle_'+key
    logs, report, objects = io.StringIO(), [], []
    try:
        bpy.context.window.scene = scene
        for n, (name, category, purpose) in enumerate(CATALOG, 1):
            ident = f'CYCLE-{n:03}'
            assert bpy.data.objects.get(ident) is None, 'ID ocupado; no se cambia una escena ajena: ' + ident
            g = create_model(n)
            mesh = bpy.data.meshes.new(ident + '_Geometry')
            mesh.from_pydata([b(v) for v in g.vertices], [], g.faces)
            keys = list(dict.fromkeys(g.materials))
            for key in keys:
                mesh.materials.append(palette[key])
            for polygon, key in zip(mesh.polygons, g.materials):
                polygon.material_index = keys.index(key)
            uv = mesh.uv_layers.new(name='UVMap')
            for polygon in mesh.polygons:
                for loop in polygon.loop_indices:
                    p = mesh.vertices[mesh.loops[loop].vertex_index].co
                    uv.data[loop].uv = (p.x * .25, (p.y + p.z) * .25)
            bm = bmesh.new(); bm.from_mesh(mesh)
            bmesh.ops.triangulate(bm, faces=list(bm.faces))
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            assert all(f.calc_area() > 1e-9 for f in bm.faces), 'Cara degenerada: ' + ident
            bm.to_mesh(mesh); bm.free(); mesh.update(); mesh.calc_loop_triangles()
            obj = bpy.data.objects.new(ident, mesh); scene.collection.objects.link(obj)
            obj['ecosphere_model_id'], obj['educational_function'] = ident, purpose
            objects.append(obj)
            bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            glb = destination / 'models' / (ident + '.glb')
            with contextlib.redirect_stdout(logs):
                bpy.ops.export_scene.gltf(filepath=str(glb), export_format='GLB', use_selection=True,
                    use_active_scene=True, export_yup=True, export_normals=True, export_texcoords=True,
                    export_animations=False, export_cameras=False, export_lights=False,
                    export_draco_mesh_compression_enable=False, export_tangents=False, export_attributes=False)
            data = glb.read_bytes(); length = int.from_bytes(data[12:16], 'little')
            doc = json.loads(data[20:20 + length])
            assert set(doc.get('extensionsUsed', [])) <= {'KHR_materials_unlit'}
            lo = [min(v[a] for v in g.vertices) for a in range(3)]
            dims = [max(v[a] for v in g.vertices) - lo[a] for a in range(3)]
            normalized = sorted(set(tuple(round((v[a] - lo[a]) / dims[a], 5) for a in range(3)) for v in g.vertices))
            report.append(dict(model_id=ident, name=name, category=category, educational_function=purpose,
                dimensions_m=dims, triangles=len(mesh.loop_triangles), materials=keys, bytes=len(data),
                bounds=[lo,[lo[a]+dims[a] for a in range(3)]], signs=getattr(g,'signs',[]),
                mechanism_bounds=getattr(g,'mechanism_bounds',None),
                sha256=hashlib.sha256(data).hexdigest(), geometry_signature=hashlib.sha256(repr(normalized).encode()).hexdigest()))
            print(ident, len(mesh.loop_triangles), 'triangles', len(data), 'bytes')
        assert len({m['geometry_signature'] for m in report}) == 24
        for i, obj in enumerate(objects):
            obj.location = ((i % 7) * 40, (i // 7) * 40, 0)
        bpy.data.libraries.write(str(destination / 'sources/ecocycle.blend'), {scene}, compress=True)
        (destination / 'export-report.json').write_text(json.dumps({'blender': bpy.app.version_string, 'models': report}, indent=2) + '\n')
        (destination / 'export.log').write_text(logs.getvalue())
        print('ECOCYCLE EXPORT PASS: 24 distinct designs;', sum(r['triangles'] for r in report), 'triangles')
    finally:
        bpy.context.window.scene = original
        for obj in selected:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = active
        print('Original scene restored:', original.name)


if __name__ == '__main__':
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if len(args) != 1:
        raise SystemExit('Usar -- salida_nueva')
    build(args[0])
