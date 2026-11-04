"""Fuentes propias de los 25 modelos de plaza; ejecutar en Blender por MCP o --python.

build('/ruta/nueva') exporta modelos, PNG y biblioteca .blend aislada, sin tocar
la escena original ni reemplazar archivos. Las coordenadas del script son X/Y/Z
del motor (+Y altura); b(x,y,z) convierte a Blender antes de exportar Y arriba.
"""
import bpy
import bmesh
import csv
import hashlib
import io
import json
import math
import contextlib
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
TAU = math.tau


def b(p):
    return (p[0], -p[2], p[1])


# Trazos propios en una cuadrícula unitaria, sin fuentes ni imágenes externas.
LETTERS = {
    'A': [[(0,0),(.5,1),(1,0)],[(.2,.4),(.8,.4)]],
    'B': [[(0,0),(0,1),(.7,1),(1,.75),(.7,.5),(0,.5)],[(.7,.5),(1,.25),(.7,0),(0,0)]],
    'C': [[(1,1),(0,1),(0,0),(1,0)]], 'D': [[(0,0),(0,1),(.6,1),(1,.7),(1,.3),(.6,0),(0,0)]],
    'E': [[(1,1),(0,1),(0,0),(1,0)],[(0,.5),(.8,.5)]],
    'F': [[(0,0),(0,1),(1,1)],[(0,.5),(.8,.5)]],
    'G': [[(1,1),(0,1),(0,0),(1,0),(1,.5),(.6,.5)]],
    'H': [[(0,0),(0,1)],[(1,0),(1,1)],[(0,.5),(1,.5)]],
    'I': [[(0,1),(1,1)],[(.5,1),(.5,0)],[(0,0),(1,0)]],
    'J': [[(0,1),(1,1),(1,0),(0,0),(0,.3)]],
    'K': [[(0,0),(0,1)],[(1,1),(0,.5),(1,0)]],
    'L': [[(0,1),(0,0),(1,0)]], 'M': [[(0,0),(0,1),(.5,.5),(1,1),(1,0)]],
    'N': [[(0,0),(0,1),(1,0),(1,1)]], 'O': [[(0,0),(0,1),(1,1),(1,0),(0,0)]],
    'P': [[(0,0),(0,1),(1,1),(1,.5),(0,.5)]],
    'Q': [[(0,0),(0,1),(1,1),(1,0),(0,0)],[(.6,.3),(1.1,-.1)]],
    'R': [[(0,0),(0,1),(1,1),(1,.5),(0,.5)],[(.4,.5),(1,0)]],
    'S': [[(1,1),(0,1),(0,.5),(1,.5),(1,0),(0,0)]],
    'T': [[(0,1),(1,1)],[(.5,1),(.5,0)]], 'U': [[(0,1),(0,0),(1,0),(1,1)]],
    'V': [[(0,1),(.5,0),(1,1)]], 'W': [[(0,1),(.2,0),(.5,.5),(.8,0),(1,1)]],
    'X': [[(0,0),(1,1)],[(0,1),(1,0)]], 'Y': [[(0,1),(.5,.5),(1,1)],[(.5,.5),(.5,0)]],
    'Z': [[(0,1),(1,1),(0,0),(1,0)]],
}


class Geometry:
    def __init__(self):
        self.vertices, self.faces, self.materials = [], [], []

    def mesh(self, vertices, faces, material):
        offset = len(self.vertices)
        self.vertices.extend(tuple(v) for v in vertices)
        self.faces.extend(tuple(offset+i for i in f) for f in faces)
        self.materials.extend([material]*len(faces))

    def box(self, center, size, material):
        x,y,z = center
        a,h,c = (s/2 for s in size)
        vertices = [(x+dx*a,y+dy*h,z+dz*c) for dx,dy,dz in
                    [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                     (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        self.mesh(vertices,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(3,7,6,2),(0,4,7,3),(1,2,6,5)],material)

    def cylinder(self, center, radius, height, material, sides=32, top_radius=None):
        top_radius = radius if top_radius is None else top_radius
        x,y,z = center
        vertices = [(x+r*math.cos(TAU*i/sides),y+h,z+r*math.sin(TAU*i/sides))
                    for h,r in [(-height/2,radius),(height/2,top_radius)] for i in range(sides)]
        faces = [(i,(i+1)%sides,(i+1)%sides+sides,i+sides) for i in range(sides)]
        faces += [tuple(reversed(range(sides))),tuple(range(sides,2*sides))]
        self.mesh(vertices,faces,material)

    def ring(self, inner, outer, bottom, top, material, sides=96):
        vertices = [(r*math.cos(TAU*i/sides),y,r*math.sin(TAU*i/sides))
                    for y,r in [(bottom,inner),(bottom,outer),(top,inner),(top,outer)] for i in range(sides)]
        faces=[]
        for i in range(sides):
            j=(i+1)%sides
            faces += [(i,j,j+2*sides,i+2*sides),(i+sides,i+3*sides,j+3*sides,j+sides),
                      (i+2*sides,j+2*sides,j+3*sides,i+3*sides),(i,i+sides,j+sides,j)]
        self.mesh(vertices,faces,material)

    def sphere(self, center, radii, material, sides=16, rings=8):
        c=Vector(center)
        vertices=[c+Vector((0,radii[1],0)),c-Vector((0,radii[1],0))]
        for j in range(1,rings):
            phi=math.pi*j/rings
            vertices += [c+Vector((radii[0]*math.sin(phi)*math.cos(TAU*i/sides),
                                   radii[1]*math.cos(phi),radii[2]*math.sin(phi)*math.sin(TAU*i/sides))) for i in range(sides)]
        faces=[]
        for i in range(sides):
            k=(i+1)%sides
            faces += [(0,2+i,2+k),(1,2+(rings-2)*sides+k,2+(rings-2)*sides+i)]
            for j in range(rings-2):
                a=2+j*sides+i; b0=2+j*sides+k
                faces.append((a,a+sides,b0+sides,b0))
        self.mesh(vertices,faces,material)

    def tube(self, points, radius, material, sides=8):
        points=list(map(Vector,points)); vertices=[];previous=None
        for i,p in enumerate(points):
            tangent=(points[min(i+1,len(points)-1)]-points[max(i-1,0)]).normalized()
            if previous is None:
                reference=Vector((0,1,0)) if abs(tangent.y)<.9 else Vector((1,0,0))
                u=tangent.cross(reference).normalized()
            else:
                u=(previous-tangent*previous.dot(tangent)).normalized()
                assert u.length>.9,'Curva con giro discontinuo'
            v=tangent.cross(u).normalized();previous=u
            vertices += [p+radius*(math.cos(TAU*k/sides)*u+math.sin(TAU*k/sides)*v) for k in range(sides)]
        faces=[]
        for i in range(len(points)-1):
            for k in range(sides):
                j=(k+1)%sides; a=i*sides
                faces.append((a+k,a+j,a+j+sides,a+k+sides))
        faces += [tuple(reversed(range(sides))),tuple(range((len(points)-1)*sides,len(points)*sides))]
        self.mesh(vertices,faces,material)

    def leaf(self, start, end, width, material):
        a,c=Vector(start),Vector(end); middle=(a+c)/2
        side=Vector((-(c-a).y,(c-a).x,0)).normalized()*width
        vertices=[a,middle+side,c,middle-side,middle+Vector((0,0,.09)),middle-Vector((0,0,.09))]
        self.mesh(vertices,[(i,(i+1)%4,4) for i in range(4)]+[((i+1)%4,i,5) for i in range(4)],material)

    def text(self, words, center, height, material):
        x,y,z=center; advance=height*.85
        start=x-(len(words)*advance-height*.2)/2
        for index,letter in enumerate(words):
            if letter==' ': continue
            for path in LETTERS[letter]:
                for a,c in zip(path,path[1:]):
                    self.tube([(start+index*advance+a[0]*height*.65,y+a[1]*height,z),
                               (start+index*advance+c[0]*height*.65,y+c[1]*height,z)],height*.065,material,4)


def material(name, color, metallic=0, roughness=.75, alpha=1, double_sided=False):
    m=bpy.data.materials.new('Plaza_'+name)
    m.use_nodes=True
    m.use_backface_culling=not double_sided
    node=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    node.inputs['Base Color'].default_value=(*color,1)
    node.inputs['Metallic'].default_value=metallic
    node.inputs['Roughness'].default_value=roughness
    node.inputs['Alpha'].default_value=alpha
    if alpha<1:
        allowed={e.identifier for e in bpy.types.Material.bl_rna.properties['surface_render_method'].enum_items}
        assert 'BLENDED' in allowed
        m.surface_render_method='BLENDED'
    return m


def create_model(number, destination='ecoenergy'):
    g=Geometry()
    if number==1:
        g.cylinder((0,-.09,0),10.5,.18,'stone',96)
        g.ring(3.3,3.45,0,.014,'sand')
        g.ring(9.8,9.95,0,.014,'sand')
        for i in range(24):
            a=TAU*i/24
            g.tube([(3.45*math.cos(a),.006,3.45*math.sin(a)),(9.8*math.cos(a),.006,9.8*math.sin(a))],.018,'joint',4)
    elif number==2:
        g.cylinder((0,-.22,0),22.5,.18,'lawn',128)
        g.ring(10.5,12,-.03,.025,'sand')
        g.ring(11.85,12,-.02,.04,'stone')
    elif number==3:
        g.box((0,-.055,0),(3.6,.11,8),'stone')
        for x in [-1.73,1.73]: g.box((x,.015,0),(.14,.06,8),'sand')
        for z in [-3,-2,-1,0,1,2,3]: g.box((0,.003,z),(3.35,.008,.025),'joint')
    elif number==4:
        g.cylinder((0,.15,0),1.22,.3,'stone',48)
        g.cylinder((0,.6,0),.92,.6,'sand',48,1.02)
        g.ring(.92,1.06,.83,.93,'metal',48)
        g.text('ECOSPHERE',(0,.38,1.025),.14,'green')
    elif number==5:
        g.sphere((0,2.05,0),(1.1,1.1,1.1),'ocean',32,16)
        # Continentes estilizados originales: parches angulares, no un mapa geográfico exacto.
        for lon,lat,sx,sy in [(-80,30,28,40),(-60,-30,17,35),(20,12,25,37),(85,40,45,25),(135,-28,20,15)]:
            def point(radius,angle):
                p=math.radians(lat+sy*radius*math.sin(angle));q=math.radians(lon+sx*radius*math.cos(angle))
                return (1.125*math.cos(p)*math.cos(q),2.05+1.125*math.sin(p),1.125*math.cos(p)*math.sin(q))
            vertices=[point(0,0)]+[point(r/4,TAU*i/12) for r in range(1,5) for i in range(12)]
            faces=[(0,1+i,1+(i+1)%12) for i in range(12)]
            for r in range(3):
                for i in range(12):
                    a=1+r*12+i;c=1+r*12+(i+1)%12
                    faces.append((a,a+12,c+12,c))
            g.mesh(vertices,faces,'green')
        g.leaf((-.25,0,0),(-1.45,2.1,0),.6,'green')
        g.leaf((.1,.05,.1),(1.45,1.65,.1),.55,'mint')
        g.tube([(-.4,0,0),(-.25,.7,0),(0,1,0)],.1,'metal',10)
        g.tube([(1.23*math.cos(TAU*i/48),2.05+1.23*math.sin(TAU*i/48),-.06) for i in range(49)],.027,'metal',6)
    elif number==6:
        g.cylinder((0,.08,0),3.1,.16,'stone',64)
        g.ring(2.7,3.05,.1,.65,'sand',64)
        g.ring(2.68,2.78,.62,.7,'mint',64)
        g.ring(1.3,2.7,.46,.49,'water',64)
    elif number==7:
        for i in range(8):
            a=TAU*i/8
            g.cylinder((2.3*math.cos(a),.49,2.3*math.sin(a)),.085,.08,'metal',12)
            points=[]
            for j in range(13):
                t=j/12; r=2.3-.85*t
                points.append((r*math.cos(a),.53+3.3*t*(1-t),r*math.sin(a)))
            g.tube(points,.038,'water',8)
    elif number==8:
        accent={'ecoenergy':'amber','aquabio':'cyan','ecocycle':'mint'}[destination]
        points=[(-2.3,0,0),(-2.3,2.6,0)]
        points += [(2.3*math.cos(math.pi*(1-i/24)),2.6+1.45*math.sin(math.pi*(1-i/24)),0) for i in range(1,25)]
        points += [(2.3,0,0)]
        g.tube(points,.16,accent,12)
        for x in [-2.3,2.3]: g.box((x,.1,0),(.65,.2,.65),'stone')
        g.box((0,3.18,.06),(4.05,.85,.18),'screen')
        g.text(destination.upper(),(.35,3,.18),.36,'white')
        if destination=='ecoenergy':
            g.mesh([(-1.6,3.5,.2),(-1.85,3.16,.2),(-1.57,3.16,.2),(-1.68,2.85,.2),(-1.3,3.28,.2),(-1.58,3.28,.2)],[(0,1,2),(2,3,4),(2,4,5)],accent)
        elif destination=='aquabio':
            g.mesh([(-1.6,3.52,.2),(-1.86,3.08,.2),(-1.75,2.9,.2),(-1.45,2.9,.2),(-1.34,3.08,.2)],[(0,1,2),(0,2,3),(0,3,4)],accent)
        else:
            points=[(-1.6,3.49,.2),(-1.9,2.96,.2),(-1.3,2.96,.2)]
            for i,a in enumerate(points):
                c=points[(i+1)%3];v=Vector(c)-Vector(a);tip=Vector(a)+v*.8
                g.tube([a,tip],.035,accent,4)
                side=Vector((-v.y,v.x,0)).normalized()*.09;tail=tip-v.normalized()*.16
                g.mesh([tip,tail+side,tail-side],[(0,1,2)],accent)
    elif number==9:
        g.box((0,-.04,0),(5.3,.08,3),'sand')
        for x in [-2.5,2.5]: g.box((x,.025,0),(.16,.04,3),'mint')
        g.box((0,.012,0),(.06,.015,2.5),'joint')
        for x in [-1.4,1.4]:
            g.tube([(x-.3,.02,-.4),(x,.02,-.8),(x+.3,.02,-.4)],.04,'mint',4)
    elif number==10:
        for x in [-2,2]:
            for z in [-1.4,1.4]: g.box((x,1.4,z),(.17,2.8,.17),'wood')
        for x in [-2,2]: g.box((x,2.85,0),(.2,.22,3.5),'wood')
        for x in [-2,-1.5,-1,-.5,0,.5,1,1.5,2]: g.box((x,3,0),(.17,.18,3.8),'wood')
        g.leaf((-1.9,1.4,-1.45),(-1.7,2.8,-1.45),.3,'green')
    elif number==11:
        for x in [-1.8,1.8]: g.box((x,1.5,0),(.16,3,.16),'metal')
        g.box((0,3.08,0),(4.8,.18,3.1),'metal')
        g.box((0,3.2,0),(4.55,.08,2.85),'solar')
        for x in [-2.3,0,2.3]: g.box((x,3.26,0),(.06,.06,3),'white')
    elif number==12:
        g.box((0,1.65,0),(.18,3.3,.18),'metal')
        # Fondo continuo: impide leer paneles lejanos entre los destinos del tótem.
        g.box((0,2,.04),(3.5,2.25,.18),'screen')
        for y,label in [(2.8,'ECOENERGY'),(2,'AQUABIO'),(1.2,'ECOCYCLE')]:
            g.text(label,(0,y-.15,.15),.31,'white')
        for y in [1.6,2.4]:g.box((0,y,.145),(3.2,.025,.015),'metal')
        g.cylinder((0,.06,0),.5,.12,'stone',24)
    elif number==13:
        g.box((0,.15,0),(1.6,.3,1),'stone')
        g.box((0,1.05,0),(1.3,1.5,.65),'green')
        g.box((0,1.55,.36),(1.06,.66,.04),'screen')
        g.text('INFO',(0,1.43,.42),.24,'white')
        for x in [-1,1]: g.box((x,2.35,0),(.06,.5,.06),'metal')
        g.box((0,2.62,0),(2.8,.5,.18),'screen')
        g.text('INFORMACION',(0,2.49,.11),.25,'white')
        for x in [-.3,0,.3]: g.cylinder((x,1,.37),.07,.08,'mint',12)
        g.box((0,2.1,0),(1.8,.15,1.3),'wood')
    elif number==14:
        for x in [-1.1,1.1]: g.box((x,.9,0),(.12,1.8,.12),'metal')
        g.box((0,1.6,0),(3,1.55,.16),'wood')
        g.box((0,1.6,.095),(2.8,1.35,.03),'screen')
        for y,words,size in [(1.92,'CUIDA EL AGUA',.25),(1.47,'REUSA',.34),(1.02,'RECICLA',.30)]:
            g.text(words,(0,y,.14),size,'white')
    elif number==15:
        for x in [-.85,.85]:
            g.box((x,.23,0),(.13,.46,.7),'metal')
            g.box((x,.7,-.29),(.12,.8,.12),'metal')
        for z in [-.28,-.08,.12,.32]: g.box((0,.48,z),(2.25,.1,.16),'wood')
        for y in [.72,.95]: g.box((0,y,-.3),(2.25,.16,.11),'wood')
    elif number==16:
        g.cylinder((0,.08,0),.3,.16,'stone',20)
        g.cylinder((0,1.7,0),.06,3.3,'metal',12)
        g.tube([(0,3.3,0),(.3,3.5,0),(.65,3.5,0)],.06,'metal',8)
        g.box((.65,3.48,0),(.8,.13,.45),'metal')
        g.box((.65,3.4,0),(.66,.035,.32),'lamp')
    elif number==17:
        for x in [-1.2,-.4,.4,1.2]:
            g.tube([(x,0,-.45),(x,.72,-.45),(x,.9,-.2),(x,.9,.2),(x,.72,.45),(x,0,.45)],.045,'metal',8)
        for z in [-.45,.45]: g.box((0,.045,z),(3,.09,.14),'metal')
    elif number==18:
        for x,color,label in [(-.7,'ocean','P'),(0,'green','O'),(.7,'sand','R')]:
            g.box((x,.5,0),(.62,1,.65),color)
            g.box((x,1.04,0),(.7,.1,.73),'metal')
            g.box((x,.92,.37),(.36,.16,.03),'joint')
            g.text(label,(x,.38,.335),.25,'white')
    elif number==19:
        g.cylinder((0,.4,0),.19,.8,'metal',20)
        g.cylinder((0,.86,0),.4,.12,'metal',32)
        g.ring(.2,.38,.9,.96,'white',32)
        g.tube([(.22,.92,-.1),(.22,1.16,-.1),(.1,1.16,-.1),(.06,1.05,-.1)],.025,'metal',8)
        g.cylinder((0,.055,0),.36,.11,'stone',24)
    elif number==20:
        g.box((0,.3,0),(2.4,.6,.9),'wood')
        g.box((0,.61,0),(2.2,.05,.7),'soil')
        for x in [-1,1]: g.box((x,.3,.46),(.045,.6,.025),'metal')
    elif number==21:
        g.cylinder((0,1.5,0),.22,3,'bark',12,.12)
        for x,z in [(-1,0),(1,.2),(0,-1),(.2,1)]:
            g.tube([(0,1.8,0),(x*.7,2.8,z*.7),(x,3.1,z)],.09,'bark',8)
            g.sphere((x,3.45,z),(1.45,.8,1.4),'green',12,6)
        g.sphere((0,3.85,0),(1.65,.85,1.55),'mint',12,6)
    elif number==22:
        g.cylinder((0,.85,0),.12,1.7,'bark',10,.065)
        for x,z,h in [(-.45,0,2),( .4,.1,2.2),(0,-.4,2.35),(0,.35,1.9)]:
            g.tube([(0,1.1,0),(x,h-.3,z)],.045,'bark',6)
            g.sphere((x,h,z),(.62,.75,.6),'blossom',10,5)
    elif number==23:
        for i in range(7):
            a=TAU*i/7; x=.42*math.cos(a);z=.32*math.sin(a)
            g.sphere((x,.4,z),(.43,.4,.4),'green',10,5)
            for j in range(5):
                t=TAU*j/5
                g.sphere((x+.08*math.cos(t),.76,z+.08*math.sin(t)),(.075,.025,.075),'flower',6,3)
            g.sphere((x,.785,z),(.035,.025,.035),'lamp',6,3)
    elif number==24:
        for clump,(x,z) in enumerate([(-.25,0),(.25,.1),(0,-.3)]):
            for i in range(12):
                a=TAU*i/12; height=.48+.19*((i+clump)%4)
                base=Vector((x,0,z));side=Vector((.04*math.cos(a),0,.04*math.sin(a)))
                mid=base+Vector((.16*math.cos(a),height*.6,.16*math.sin(a)))
                tip=base+Vector((.32*math.cos(a),height,.32*math.sin(a)))
                g.mesh([base-side,base+side,mid+side*.6,mid-side*.6,tip],[(0,1,2,3),(3,2,4)],'grass')
    elif number==25:
        for x,y,z,r in [(-.45,.22,0,(.65,.3,.5)),(.4,.3,-.1,(.5,.4,.6)),(.1,.1,.5,(.38,.18,.3))]:
            g.sphere((x,y,z),r,'rock',7,4)
    else:
        raise ValueError(number)
    return g


def build(destination):
    destination=Path(destination).resolve()
    assert not (destination/'sources/plaza.blend').exists(), 'Se requiere una salida nueva'
    assert not any((destination/'models').glob('*.glb')), 'No se reemplazan GLB existentes'
    for folder in ['models','sources','textures']: (destination/folder).mkdir(parents=True,exist_ok=True)
    original=bpy.context.window.scene
    scene=bpy.data.scenes.new('EcoSphere_Plaza_Catalog')
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    specs={
        'stone':((.42,.48,.46),0,.9), 'sand':((.72,.67,.5),0,.82), 'joint':((.16,.23,.21),0,.9),
        'lawn':((.12,.26,.13),0,1), 'green':((.06,.34,.17),0,.75), 'mint':((.2,.52,.28),0,.68),
        'ocean':((.035,.25,.48),.25,.36), 'metal':((.38,.47,.43),.75,.28),
        'wood':((.36,.2,.085),0,.88), 'white':((.88,.92,.84),0,.65),
        'screen':((.008,.025,.035),0,.8), 'amber':((.85,.5,.035),.4,.4), 'cyan':((.015,.45,.65),.4,.4),
        'solar':((1,1,1),.2,.38), 'lamp':((1,.79,.35),0,.4),
        'soil':((.12,.065,.025),0,1), 'bark':((.23,.12,.045),0,1),
        'blossom':((.72,.4,.47),0,.8), 'flower':((.85,.55,.08),0,.7), 'rock':((.32,.36,.35),0,.95)}
    palette={name:material(name,*values) for name,values in specs.items()}
    palette['water']=material('water',(.025,.38,.5),.15,.2,.7,True)
    palette['glass']=material('glass',(.08,.4,.48),0,.2,.45,True)
    palette['grass']=material('grass',(.3,.46,.14),0,.9,1,True)
    report=[]; logs=io.StringIO();objects=[]
    try:
        bpy.context.window.scene=scene
        image=bpy.data.images.new('Plaza_SolarCells_128',width=128,height=128,alpha=True)
        pixels=[]
        for y in range(128):
            for x in range(128):
                color=(.48,.66,.76,1) if x%32<2 or y%32<2 else (.018,.055+.015*(x%16)/16,.15,1)
                pixels.extend(color)
        image.pixels=pixels;image.file_format='PNG'
        image.filepath_raw=str(destination/'textures/solar_cells.png'); image.save();image.pack()
        node=palette['solar'].node_tree.nodes.new('ShaderNodeTexImage');node.image=image
        principled=next(n for n in palette['solar'].node_tree.nodes if n.type=='BSDF_PRINCIPLED')
        palette['solar'].node_tree.links.new(node.outputs['Color'],principled.inputs['Base Color'])
        catalog=list(csv.DictReader((ROOT/'docs/assets/PLAZA_INVENTORY.csv').open()))
        assert len(catalog)==25
        for number,row in enumerate(catalog,1):
            ident=f'PLAZA-{number:03}';assert row['model_id']==ident
            previous=bpy.data.objects.get(ident)
            if previous:
                assert previous.users_scene and all(s.name.startswith('EcoSphere_Plaza_Catalog') for s in previous.users_scene), 'ID ocupado por contenido ajeno'
                previous.name=ident+'_previous'
            g=create_model(number)
            mesh=bpy.data.meshes.new(ident+'_Geometry');mesh.from_pydata([b(v) for v in g.vertices],[],g.faces)
            keys=list(dict.fromkeys(g.materials))
            for key in keys: mesh.materials.append(palette[key])
            for polygon,key in zip(mesh.polygons,g.materials): polygon.material_index=keys.index(key)
            bm=bmesh.new();bm.from_mesh(mesh)
            # Normales por triángulo: los tubos y parches curvos no tienen quads planos.
            bmesh.ops.triangulate(bm,faces=list(bm.faces))
            bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
            mesh.update();mesh.calc_loop_triangles()
            uv=mesh.uv_layers.new(name='UVMap')
            for loop in mesh.loops:
                p=mesh.vertices[loop.vertex_index].co
                uv.data[loop.index].uv=(p.x/4.55+.5,-p.y/2.85+.5)
            obj=bpy.data.objects.new(ident,mesh);scene.collection.objects.link(obj);objects.append(obj)
            assert obj.name==ident
            obj['ecosphere_model_id']=ident
            bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
            glb=destination/'models'/(ident+'.glb')
            with contextlib.redirect_stdout(logs):
                result=bpy.ops.export_scene.gltf(filepath=str(glb),export_format='GLB',use_selection=True,
                    use_active_scene=True,export_yup=True,export_normals=True,export_texcoords=True,
                    export_animations=False,export_cameras=False,export_lights=False,export_materials='EXPORT',
                    export_draco_mesh_compression_enable=False,export_tangents=False,export_attributes=False)
            assert 'FINISHED' in result and glb.stat().st_size>0
            data=glb.read_bytes();json_length=int.from_bytes(data[12:16],'little');document=json.loads(data[20:20+json_length])
            assert not document.get('extensionsUsed'), document.get('extensionsUsed')
            minima=[min(v[a] for v in g.vertices) for a in range(3)]
            dimensions=[max(v[a] for v in g.vertices)-minima[a] for a in range(3)]
            normalized=sorted(set(tuple(round((v[a]-minima[a])/max(dimensions[a],1e-9),5) for a in range(3)) for v in g.vertices))
            signature=hashlib.sha256(repr(normalized).encode()).hexdigest()
            report.append({'model_id':ident,'name':row['name'],'triangles':len(mesh.loop_triangles),
                           'vertices_blender':len(mesh.vertices),'dimensions_m':dimensions,'materials':keys,
                           'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'geometry_signature':signature})
            print(f'EXPORTED {ident}: {len(mesh.loop_triangles)} triangles; {len(data)} bytes')
        assert len({r['geometry_signature'] for r in report})==25,'Geometrias duplicadas'
        # Catálogo editable separado; no incluye objetos, cámaras o materiales del usuario.
        for i,obj in enumerate(objects): obj.location=((i%5)*52,(i//5)*52,0)
        bpy.data.libraries.write(str(destination/'sources/plaza.blend'),{scene},compress=True)
        (destination/'export-report.json').write_text(json.dumps({'blender':bpy.app.version_string,'models':report},indent=2)+'\n')
        (destination/'export.log').write_text(logs.getvalue())
        print(f'PLAZA EXPORT PASS: 25 distinct models; {sum(r["triangles"] for r in report)} source triangles; editable library saved')
    finally:
        bpy.context.window.scene=original
        print('Original scene restored:',original.name)


def revise_signage(destination, totem_only=False):
    """5C: carga el maestro existente, conserva 21 mallas y revisa cuatro; dos variantes no aumentan modelos únicos."""
    destination=Path(destination).resolve()
    assert not destination.exists(), 'Requiere salida nueva para conservar exportaciones previas'
    for folder in ['models/variants','sources']: (destination/folder).mkdir(parents=True,exist_ok=True)
    original=bpy.context.window.scene
    logs=io.StringIO();report=[]
    with bpy.data.libraries.load(str(ROOT/'assets/plaza/sources/plaza.blend'),link=False) as (source,target):
        target.scenes=[name for name in source.scenes if name.startswith(('EcoSphere_Plaza_Catalog','EcoSphere_Plaza_5C','EcoSphere_Plaza_5D'))]
    assert len(target.scenes)==1
    scene=target.scenes[0];scene.name='EcoSphere_Plaza_5D' if totem_only else 'EcoSphere_Plaza_5C'
    all_objects={o.get('ecosphere_model_id'):o for o in scene.objects if o.type=='MESH'}
    objects={ident:obj for ident,obj in all_objects.items() if ident and len(ident)==9}
    assert len(objects)==25
    palette={m.name.removeprefix('Plaza_').split('.')[0]:m for o in objects.values() for m in o.data.materials}
    palette.update(screen=material('screen_5C',(.008,.025,.035)),white=material('white_5C',(.95,.98,.93)),
                   amber=material('amber_5C',(.85,.5,.035),.4,.4),cyan=material('cyan_5C',(.015,.45,.65),.4,.4))
    # Mismo patrón unlit del probe de Fase 2: rótulos legibles sin reflejo especular.
    for key,color in [('screen',(.008,.025,.035,1)),('white',(.95,.98,.93,1))]:
        tree=palette[key].node_tree;tree.nodes.clear()
        output=tree.nodes.new('ShaderNodeOutputMaterial');flat=tree.nodes.new('ShaderNodeBackground')
        flat.inputs['Color'].default_value=color;tree.links.new(flat.outputs[0],output.inputs['Surface'])
    try:
        bpy.context.window.scene=scene
        revisions=[(12,None)] if totem_only else [(8,'ecoenergy'),(12,None),(13,None),(14,None),(8,'aquabio'),(8,'ecocycle')]
        for number,variant in revisions:
            ident=f'PLAZA-{number:03}';object_id=ident if variant in (None,'ecoenergy') else ident+'-'+variant
            g=create_model(number,variant or 'ecoenergy')
            mesh=bpy.data.meshes.new(object_id+'_5C_Geometry');mesh.from_pydata([b(v) for v in g.vertices],[],g.faces)
            keys=list(dict.fromkeys(g.materials))
            for key in keys: mesh.materials.append(palette[key])
            for polygon,key in zip(mesh.polygons,g.materials): polygon.material_index=keys.index(key)
            bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.triangulate(bm,faces=list(bm.faces))
            bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free();mesh.update();mesh.calc_loop_triangles()
            if object_id in all_objects:
                obj=all_objects[object_id];obj.data=mesh
            else:
                obj=bpy.data.objects.new(object_id+'_5C',mesh);scene.collection.objects.link(obj)
            obj['ecosphere_model_id']=object_id
            obj['ecosphere_base_model_id']=ident
            location=obj.location.copy();obj.location=(0,0,0)
            bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
            relative=('variants/'+object_id+'.glb') if object_id!=ident else ident+'.glb'
            path=destination/'models'/relative
            with contextlib.redirect_stdout(logs):
                result=bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
                    use_active_scene=True,export_yup=True,export_normals=True,export_texcoords=True,
                    export_animations=False,export_cameras=False,export_lights=False,export_materials='EXPORT',
                    export_draco_mesh_compression_enable=False,export_tangents=False,export_attributes=False)
            assert 'FINISHED' in result
            obj.location=location
            data=path.read_bytes();document=json.loads(data[20:20+int.from_bytes(data[12:16],'little')])
            assert set(document.get('extensionsUsed',[]))<={'KHR_materials_unlit'}
            minima=[min(v[a] for v in g.vertices) for a in range(3)]
            dimensions=[max(v[a] for v in g.vertices)-minima[a] for a in range(3)]
            normalized=sorted(set(tuple(round((v[a]-minima[a])/max(dimensions[a],1e-9),5) for a in range(3)) for v in g.vertices))
            report.append({'model_id':object_id,'base_model_id':ident,'triangles':len(mesh.loop_triangles),
                'dimensions_m':dimensions,'materials':keys,'sha256':hashlib.sha256(data).hexdigest(),
                'geometry_signature':hashlib.sha256(repr(normalized).encode()).hexdigest(),'bytes':len(data),'glb_file':relative})
            print('EXPORTED',object_id,len(mesh.loop_triangles),'triangles',len(data),'bytes')
        bpy.data.libraries.write(str(destination/'sources/plaza.blend'),{scene},compress=True)
        (destination/'export-report.json').write_text(json.dumps({'blender':bpy.app.version_string,'models':report},indent=2)+'\n')
        (destination/'export.log').write_text(logs.getvalue())
        print('SIGNAGE PASS:',len(revisions),'revisions; 25 base models + 2 text/icon variants, not 27 unique models')
    finally:
        bpy.context.window.scene=original
        print('Original scene restored:',original.name)


if __name__=='__main__':
    import sys
    arguments=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    if len(arguments)==2 and arguments[0]=='--signage': revise_signage(arguments[1])
    elif len(arguments)==2 and arguments[0]=='--wayfinding': revise_signage(arguments[1],totem_only=True)
    elif len(arguments)==1: build(arguments[0])
    else: raise RuntimeError('Usar -- [--signage] /ruta/nueva/de/salida')
