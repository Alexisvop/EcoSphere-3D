"""Fuentes propias de los 28 modelos de EcoEnergy (Fase 6A); ejecutar en Blender por MCP o --python.

build('/ruta/nueva') exporta 28 GLB autocontenidos, texturas PNG fuente y biblioteca
editable ecoenergy.blend sin tocar la escena original del usuario ni sobrescribir salidas.
Coordenadas en metros del motor (+X derecha, +Y arriba, +Z hacia entrada sur);
b(p) convierte a Blender (X, -Z, Y) antes de exportar con export_yup=True.
"""
import bpy
import bmesh
import contextlib
import hashlib
import io
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
TAU = math.tau

LETTERS = {
    'A': [[(0, 0), (.5, 1), (1, 0)], [(.2, .4), (.8, .4)]],
    'B': [[(0, 0), (0, 1), (.7, 1), (1, .75), (.7, .5), (0, .5)], [(.7, .5), (1, .25), (.7, 0), (0, 0)]],
    'C': [[(1, 1), (0, 1), (0, 0), (1, 0)]],
    'D': [[(0, 0), (0, 1), (.6, 1), (1, .7), (1, .3), (.6, 0), (0, 0)]],
    'E': [[(1, 1), (0, 1), (0, 0), (1, 0)], [(0, .5), (.8, .5)]],
    'F': [[(0, 0), (0, 1), (1, 1)], [(0, .5), (.8, .5)]],
    'G': [[(1, 1), (0, 1), (0, 0), (1, 0), (1, .5), (.6, .5)]],
    'H': [[(0, 0), (0, 1)], [(1, 0), (1, 1)], [(0, .5), (1, .5)]],
    'I': [[(0, 1), (1, 1)], [(.5, 1), (.5, 0)], [(0, 0), (1, 0)]],
    'K': [[(0, 0), (0, 1)], [(1, 1), (0, .5), (1, 0)]],
    'L': [[(0, 1), (0, 0), (1, 0)]],
    'M': [[(0, 0), (0, 1), (.5, .5), (1, 1), (1, 0)]],
    'N': [[(0, 0), (0, 1), (1, 0), (1, 1)]],
    'O': [[(0, 0), (0, 1), (1, 1), (1, 0), (0, 0)]],
    'P': [[(0, 0), (0, 1), (1, 1), (1, .5), (0, .5)]],
    'R': [[(0, 0), (0, 1), (1, 1), (1, .5), (0, .5)], [(.4, .5), (1, 0)]],
    'S': [[(1, 1), (0, 1), (0, .5), (1, .5), (1, 0), (0, 0)]],
    'T': [[(0, 1), (1, 1)], [(.5, 1), (.5, 0)]],
    'U': [[(0, 1), (0, 0), (1, 0), (1, 1)]],
    'V': [[(0, 1), (.5, 0), (1, 1)]],
    'W': [[(0, 1), (.2, 0), (.5, .5), (.8, 0), (1, 1)]],
    'Y': [[(0, 1), (.5, .5), (1, 1)], [(.5, .5), (.5, 0)]],
    '+': [[(.5, .15), (.5, .85)], [(.15, .5), (.85, .5)]],
    '-': [[(.15, .5), (.85, .5)]],
}

CATALOG = [
    ('ENERGY-001', 'Plataforma principal EcoEnergy', 'arquitectura'),
    ('ENERGY-002', 'Estructura de acceso energetico', 'arquitectura'),
    ('ENERGY-003', 'Sistema de caminos educativos', 'arquitectura'),
    ('ENERGY-004', 'Soporte de panel solar', 'estructura'),
    ('ENERGY-005', 'Modulo fotovoltaico', 'maquinaria'),
    ('ENERGY-006', 'Mecanismo seguidor solar', 'maquinaria'),
    ('ENERGY-007', 'Gabinete inversor', 'maquinaria'),
    ('ENERGY-008', 'Caja de conexiones fotovoltaicas', 'maquinaria'),
    ('ENERGY-009', 'Medidor inteligente', 'medicion'),
    ('ENERGY-010', 'Gabinete de baterias', 'almacenamiento'),
    ('ENERGY-011', 'Bateria modular', 'almacenamiento'),
    ('ENERGY-012', 'Indicador fisico de carga', 'medicion'),
    ('ENERGY-013', 'Transformador de distribucion', 'distribucion'),
    ('ENERGY-014', 'Poste de distribucion', 'distribucion'),
    ('ENERGY-015', 'Canalizacion de cables', 'distribucion'),
    ('ENERGY-016', 'Torre de aerogenerador', 'eolica'),
    ('ENERGY-017', 'Gondola de turbina', 'eolica'),
    ('ENERGY-018', 'Rotor eolico', 'eolica'),
    ('ENERGY-019', 'Modelo de pala eolica educativa', 'eolica'),
    ('ENERGY-020', 'Veleta', 'eolica'),
    ('ENERGY-021', 'Anemometro', 'eolica'),
    ('ENERGY-022', 'Generador electrico seccionado', 'eolica'),
    ('ENERGY-023', 'Bobina del generador', 'eolica'),
    ('ENERGY-024', 'Ventilador de demostracion', 'eolica'),
    ('ENERGY-025', 'Estacion de carga electrica', 'movilidad'),
    ('ENERGY-026', 'Vehiculo electrico educativo', 'movilidad'),
    ('ENERGY-027', 'Conector de carga', 'movilidad'),
    ('ENERGY-028', 'Panel informativo energetico', 'educativo'),
]


def b(p):
    return (p[0], -p[2], p[1])


class Geometry:
    def __init__(self):
        self.vertices, self.faces, self.materials, self.uv_boxes = [], [], [], {}

    def mesh(self, vertices, faces, material, uv_box=None):
        offset = len(self.vertices)
        self.vertices.extend(tuple(v) for v in vertices)
        start_face = len(self.faces)
        self.faces.extend(tuple(offset + i for i in f) for f in faces)
        self.materials.extend([material] * len(faces))
        if uv_box is not None:
            for f_idx in range(start_face, len(self.faces)):
                self.uv_boxes[f_idx] = uv_box

    def box(self, center, size, material, rot_euler=(0, 0, 0), uv_box=None):
        cx, cy, cz = center
        hx, hy, hz = (s / 2 for s in size)
        rot = (Matrix.Rotation(rot_euler[2], 3, 'Z') @
               Matrix.Rotation(rot_euler[1], 3, 'Y') @
               Matrix.Rotation(rot_euler[0], 3, 'X'))
        pts = [Vector((cx, cy, cz)) + rot @ Vector((dx * hx, dy * hy, dz * hz))
               for dx, dy, dz in [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
                                  (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]]
        self.mesh(pts, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                        (3, 7, 6, 2), (0, 4, 7, 3), (1, 2, 6, 5)], material, uv_box)

    def cylinder(self, center, radius, height, material, sides=24, top_radius=None, axis='Y'):
        top_radius = radius if top_radius is None else top_radius
        cx, cy, cz = center
        verts = []
        for h, r in [(-height / 2, radius), (height / 2, top_radius)]:
            for i in range(sides):
                a = TAU * i / sides
                u, v = r * math.cos(a), r * math.sin(a)
                if axis == 'Y':
                    verts.append((cx + u, cy + h, cz + v))
                elif axis == 'Z':
                    verts.append((cx + u, cy + v, cz + h))
                else:
                    verts.append((cx + h, cy + u, cz + v))
        faces = [(i, (i + 1) % sides, (i + 1) % sides + sides, i + sides) for i in range(sides)]
        faces += [tuple(reversed(range(sides))), tuple(range(sides, 2 * sides))]
        self.mesh(verts, faces, material)

    def ring(self, inner, outer, bottom, top, material, sides=48, center=(0, 0, 0), axis='Y'):
        cx, cy, cz = center
        verts = []
        for h, r in [(bottom, inner), (bottom, outer), (top, inner), (top, outer)]:
            for i in range(sides):
                a = TAU * i / sides
                u, v = r * math.cos(a), r * math.sin(a)
                if axis == 'Y':
                    verts.append((cx + u, cy + h, cz + v))
                elif axis == 'Z':
                    verts.append((cx + u, cy + v, cz + h))
                else:
                    verts.append((cx + h, cy + u, cz + v))
        faces = []
        for i in range(sides):
            j = (i + 1) % sides
            faces += [(i, j, j + 2 * sides, i + 2 * sides),
                      (i + sides, i + 3 * sides, j + 3 * sides, j + sides),
                      (i + 2 * sides, j + 2 * sides, j + 3 * sides, i + 3 * sides),
                      (i, i + sides, j + sides, j)]
        self.mesh(verts, faces, material)

    def arc_shell(self, inner, outer, length, a0, a1, material, segs=16, center=(0, 0, 0), axis='Z'):
        cx, cy, cz = center
        verts = []
        for h in [-length / 2, length / 2]:
            for r in [inner, outer]:
                for i in range(segs + 1):
                    a = a0 + (a1 - a0) * i / segs
                    u, v = r * math.cos(a), r * math.sin(a)
                    if axis == 'Z':
                        verts.append((cx + u, cy + v, cz + h))
                    else:
                        verts.append((cx + u, cy + h, cz + v))
        n = segs + 1
        faces = []
        for i in range(segs):
            b_in0, b_in1 = i, i + 1
            b_out0, b_out1 = n + i, n + i + 1
            t_in0, t_in1 = 2 * n + i, 2 * n + i + 1
            t_out0, t_out1 = 3 * n + i, 3 * n + i + 1
            faces += [(b_in0, b_out0, b_out1, b_in1),
                      (t_in0, t_in1, t_out1, t_out0),
                      (b_in0, b_in1, t_in1, t_in0),
                      (b_out0, t_out0, t_out1, b_out1)]
        faces += [(0, 2 * n, 3 * n, n), (segs, n + segs, 3 * n + segs, 2 * n + segs)]
        self.mesh(verts, faces, material)

    def sphere(self, center, radii, material, sides=16, rings=8):
        c = Vector(center)
        verts = [c + Vector((0, radii[1], 0)), c - Vector((0, radii[1], 0))]
        for j in range(1, rings):
            phi = math.pi * j / rings
            verts += [c + Vector((radii[0] * math.sin(phi) * math.cos(TAU * i / sides),
                                  radii[1] * math.cos(phi),
                                  radii[2] * math.sin(phi) * math.sin(TAU * i / sides)))
                      for i in range(sides)]
        faces = []
        for i in range(sides):
            k = (i + 1) % sides
            faces += [(0, 2 + i, 2 + k), (1, 2 + (rings - 2) * sides + k, 2 + (rings - 2) * sides + i)]
            for j in range(rings - 2):
                a = 2 + j * sides + i
                b0 = 2 + j * sides + k
                faces.append((a, a + sides, b0 + sides, b0))
        self.mesh(verts, faces, material)

    def tube(self, points, radius, material, sides=8):
        pts = list(map(Vector, points))
        verts, prev = [], None
        for i, p in enumerate(pts):
            tangent = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            if prev is None:
                ref = Vector((0, 1, 0)) if abs(tangent.y) < .9 else Vector((1, 0, 0))
                u = tangent.cross(ref).normalized()
            else:
                u = (prev - tangent * prev.dot(tangent)).normalized()
            v = tangent.cross(u).normalized()
            prev = u
            verts += [p + radius * (math.cos(TAU * k / sides) * u + math.sin(TAU * k / sides) * v)
                      for k in range(sides)]
        faces = []
        for i in range(len(pts) - 1):
            for k in range(sides):
                j = (k + 1) % sides
                a = i * sides
                faces.append((a + k, a + j, a + j + sides, a + k + sides))
        faces += [tuple(reversed(range(sides))), tuple(range((len(pts) - 1) * sides, len(pts) * sides))]
        self.mesh(verts, faces, material)

    def prism_x(self, yz_poly, x0, x1, material):
        n = len(yz_poly)
        verts = [(x0, y, z) for y, z in yz_poly] + [(x1, y, z) for y, z in yz_poly]
        faces = [(i, (i + 1) % n, (i + 1) % n + n, i + n) for i in range(n)]
        faces += [tuple(reversed(range(n))), tuple(range(n, 2 * n))]
        self.mesh(verts, faces, material)

    def loft_blade(self, stations, material, rot_z=0.0):
        # stations: list of (radius_r, chord_w, thickness_t, twist_rad, offset_z)
        rz = Matrix.Rotation(rot_z, 3, 'Z')
        n_prof = 10
        verts = []
        for r, chord, thick, twist, oz in stations:
            for i in range(n_prof):
                a = TAU * i / n_prof
                px = chord * 0.5 * math.cos(a)
                pz = thick * 0.5 * math.sin(a)
                tx = px * math.cos(twist) - pz * math.sin(twist)
                tz = px * math.sin(twist) + pz * math.cos(twist) + oz
                verts.append(rz @ Vector((tx, r, tz)))
        faces = []
        for s in range(len(stations) - 1):
            base = s * n_prof
            for i in range(n_prof):
                j = (i + 1) % n_prof
                faces.append((base + i, base + j, base + n_prof + j, base + n_prof + i))
        faces += [tuple(reversed(range(n_prof))),
                  tuple(range((len(stations) - 1) * n_prof, len(stations) * n_prof))]
        self.mesh(verts, faces, material)

    def text(self, words, center, height, material, back=False):
        x, y, z = center
        advance = height * .82
        sign = -1 if back else 1
        start = x - sign * (len(words) * advance - height * .2) / 2
        for index, letter in enumerate(words):
            if letter == ' ':
                continue
            for path in LETTERS[letter]:
                for a, c in zip(path, path[1:]):
                    p0 = (start + sign * (index * advance + a[0] * height * .62), y + a[1] * height, z)
                    p1 = (start + sign * (index * advance + c[0] * height * .62), y + c[1] * height, z)
                    self.tube([p0, p1], height * .068, material, 4)


def make_material(name, color, metallic=0.0, roughness=0.72, alpha=1.0, double_sided=False, unlit=False):
    m = bpy.data.materials.new('Energy_' + name)
    m.use_nodes = True
    m.use_backface_culling = not double_sided
    if unlit:
        tree = m.node_tree
        tree.nodes.clear()
        out = tree.nodes.new('ShaderNodeOutputMaterial')
        bg = tree.nodes.new('ShaderNodeBackground')
        bg.inputs['Color'].default_value = (*color, 1.0)
        tree.links.new(bg.outputs[0], out.inputs['Surface'])
        return m
    node = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    node.inputs['Base Color'].default_value = (*color, 1.0)
    node.inputs['Metallic'].default_value = metallic
    node.inputs['Roughness'].default_value = roughness
    node.inputs['Alpha'].default_value = alpha
    if alpha < 1.0:
        m.surface_render_method = 'BLENDED'
    return m


def build_textures(destination, palette):
    # 1. Textura monocristalina de celdas solares (256x256)
    pv = bpy.data.images.new('Energy_SolarCells_256', width=256, height=256, alpha=True)
    pv_px = []
    for y in range(256):
        for x in range(256):
            gx, gy = x % 32, y % 42
            border = x < 5 or x > 250 or y < 5 or y > 250
            busbar = gx in (0, 1, 31) or gy in (0, 1, 41)
            finger = (gx == 16) or (gy % 10 == 0 and gx % 4 == 0)
            chamfer = (gx < 4 and gy < 4) or (gx > 28 and gy < 4) or (gx < 4 and gy > 38) or (gx > 28 and gy > 38)
            if border or busbar or chamfer:
                c = (.78, .84, .90, 1.0)
            elif finger:
                c = (.32, .52, .74, 1.0)
            else:
                c = (.016, .052 + .022 * ((x + y) % 19) / 19.0, .185, 1.0)
            pv_px.extend(c)
    pv.pixels = pv_px
    pv.file_format = 'PNG'
    pv.filepath_raw = str(destination / 'textures/solar_cells.png')
    pv.save()
    pv.pack()
    tex_pv = palette['solar_tex'].node_tree.nodes.new('ShaderNodeTexImage')
    tex_pv.image = pv
    bsdf_pv = next(n for n in palette['solar_tex'].node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    palette['solar_tex'].node_tree.links.new(tex_pv.outputs['Color'], bsdf_pv.inputs['Base Color'])

    # 2. Diagrama educativo de flujo energetico (256x256) para paneles ENERGY-028
    diag = bpy.data.images.new('Energy_Diagram_256', width=256, height=256, alpha=True)
    dg_px = []
    for y in range(256):
        for x in range(256):
            c = (.015, .035, .06, 1.0)  # Fondo pizarra oscuro
            if x < 6 or x > 249 or y < 6 or y > 249:
                c = (.88, .56, .08, 1.0)  # Marco ambar
            # Bloques: Solar (arriba izq), Eolica (abajo izq), Red/Inversor (centro), Bateria + EV (derecha)
            elif 22 <= x <= 82 and 148 <= y <= 212:
                c = (.92, .62, .08, 1.0) if (x - 22) % 15 > 2 and (y - 148) % 16 > 2 else (.15, .28, .52, 1.0)
            elif 22 <= x <= 82 and 44 <= y <= 108:
                c = (.12, .52, .82, 1.0)
            elif 104 <= x <= 154 and 92 <= y <= 164:
                c = (.24, .32, .40, 1.0) if not (118 <= x <= 140 and 112 <= y <= 144) else (.95, .72, .15, 1.0)
            elif 176 <= x <= 234 and 148 <= y <= 212:
                c = (.16, .68, .38, 1.0)
            elif 176 <= x <= 234 and 44 <= y <= 108:
                c = (.08, .62, .72, 1.0)
            # Lineas de conduccion energetica entre etapas
            elif (83 <= x <= 103 and (176 <= y <= 184 or 72 <= y <= 80)) or \
                 (96 <= x <= 103 and 76 <= y <= 180) or \
                 (155 <= x <= 175 and (176 <= y <= 184 or 72 <= y <= 80)) or \
                 (155 <= x <= 162 and 76 <= y <= 180):
                c = (.96, .82, .24, 1.0)
            dg_px.extend(c)
    diag.pixels = dg_px
    diag.file_format = 'PNG'
    diag.filepath_raw = str(destination / 'textures/energy_diagram.png')
    diag.save()
    diag.pack()
    tex_dg = palette['diagram_tex'].node_tree.nodes.new('ShaderNodeTexImage')
    tex_dg.image = diag
    bsdf_dg = next(n for n in palette['diagram_tex'].node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    palette['diagram_tex'].node_tree.links.new(tex_dg.outputs['Color'], bsdf_dg.inputs['Base Color'])


def create_model(number):
    g = Geometry()
    if number == 1:
        # ENERGY-001: Plataforma/terreno principal (36 x 0.36 x 34 m)
        g.box((0, -.16, 0), (36.0, .28, 34.0), 'warm_ground')
        g.box((0, -.01, 0), (33.6, .04, 31.6), 'concrete')
        for sx in [-16.9, 16.9]:
            g.box((sx, .04, 0), (.22, .08, 33.8), 'amber')
        for sz in [-15.9, 15.9]:
            g.box((0, .04, sz), (33.8, .08, .22), 'amber')
        g.ring(1.8, 2.35, .005, .035, 'amber', 48, center=(0, 0, -3.0))
        g.cylinder((0, .018, -3.0), 1.78, .024, 'slate', 36)
        for a in [0, math.pi / 2, math.pi, 3 * math.pi / 2]:
            g.box((2.9 * math.cos(a), .022, -3.0 + 2.9 * math.sin(a)), (.9, .02, .22), 'solar_gold', (0, -a, 0))
    elif number == 2:
        # ENERGY-002: Estructura de acceso energetico
        for sx in [-2.75, 2.75]:
            g.box((sx, .18, 0), (.78, .36, .95), 'concrete')
            g.box((sx, 2.05, 0), (.34, 3.55, .48), 'steel')
            g.box((sx * .93, 2.05, .26), (.10, 3.35, .08), 'amber')
            g.box((sx * .93, 2.05, -.26), (.10, 3.35, .08), 'amber')
        g.box((0, 3.92, 0), (6.5, .42, 1.05), 'steel')
        for z in [-.32, 0, .32]:
            g.box((0, 4.22, z), (6.1, .07, .24), 'blue_panel', (.22, 0, 0))
        g.box((0, 3.42, 0), (4.65, .72, .26), 'screen')
        g.text('ECOENERGY', (0, 3.25, .16), .33, 'unlit_gold')
        g.text('ECOENERGY', (0, 3.25, -.16), .33, 'unlit_gold', back=True)
    elif number == 3:
        # ENERGY-003: Sistema de caminos educativos (3.4 x 0.12 x 7.0 m)
        g.box((0, -.035, 0), (3.4, .09, 7.0), 'pavement')
        for sx in [-1.62, 1.62]:
            g.box((sx, .025, 0), (.16, .06, 7.0), 'aluminum')
            g.box((sx * .93, .032, 0), (.06, .02, 6.9), 'amber')
        for z in [-2.8, -1.4, 0, 1.4, 2.8]:
            g.box((0, .014, z), (3.05, .012, .05), 'slate')
    elif number == 4:
        # ENERGY-004: Soporte de panel solar (altura de apoyo ~1.05 m)
        for sx in [-.72, .72]:
            g.box((sx, .08, 0), (.28, .16, 1.18), 'concrete')
            g.box((sx, .44, .42), (.08, .58, .08), 'aluminum')
            g.box((sx, .66, -.38), (.08, 1.02, .08), 'aluminum')
            g.tube([(sx, .72, .42), (sx, 1.14, -.38)], .038, 'aluminum', 6)
            g.tube([(sx, .22, .38), (sx, .95, -.35)], .025, 'steel', 6)
        for z, y in [(.32, .79), (-.28, 1.09)]:
            g.box((0, y, z), (1.84, .055, .065), 'aluminum', (.46, 0, 0))
    elif number == 5:
        # ENERGY-005: Modulo fotovoltaico (1.96 x 0.08 x 1.18 m, pivote central)
        g.box((0, 0, 0), (1.96, .065, 1.18), 'aluminum')
        g.box((0, -.022, 0), (1.86, .02, 1.08), 'white_plastic')
        g.box((0, .026, 0), (1.86, .022, 1.08), 'solar_tex', uv_box=(-0.93, 0.93, -0.54, 0.54, 'XZ'))
        g.box((0, -.055, -.32), (.24, .05, .18), 'slate')
        for sx in [-.06, .06]:
            g.tube([(sx, -.06, -.41), (sx, -.11, -.52), (sx * 1.6, -.12, -.58)], .012, 'rubber', 6)
    elif number == 6:
        # ENERGY-006: Mecanismo seguidor solar (altura a horquilla 1.68 m)
        g.cylinder((0, .07, 0), .52, .14, 'concrete', 24)
        g.cylinder((0, .16, 0), .34, .06, 'steel', 20)
        g.cylinder((0, .78, 0), .13, 1.18, 'steel', 20)
        g.ring(.14, .26, 1.32, 1.42, 'amber', 24)
        g.box((0, 1.46, 0), (.56, .09, .26), 'steel')
        for sx in [-.23, .23]:
            g.box((sx, 1.60, 0), (.07, .24, .18), 'steel')
        g.cylinder((0, 1.68, 0), .045, .58, 'aluminum', 14, axis='X')
        g.cylinder((.22, 1.16, -.18), .05, .42, 'amber', 12)
        g.tube([(0, 1.02, .14), (0, 1.56, .24)], .028, 'aluminum', 8)
    elif number == 7:
        # ENERGY-007: Gabinete inversor DC/AC
        g.box((0, .10, 0), (1.36, .20, .76), 'concrete')
        g.box((0, 1.05, 0), (1.24, 1.70, .66), 'amber')
        g.box((0, 1.06, .34), (1.06, 1.48, .04), 'white_plastic')
        for sx in [-.64, .64]:
            for i in range(6):
                g.box((sx, 1.05, -.22 + i * .09), (.04, 1.28, .03), 'aluminum')
        g.box((0, 1.36, .37), (.72, .42, .03), 'screen')
        g.text('DC AC', (0, 1.28, .395), .14, 'unlit_gold')
        g.cylinder((.32, .86, .38), .07, .05, 'slate', 16, axis='Z')
        g.box((.32, .86, .42), (.03, .12, .04), 'red_accent')
        for x in [-.36, -.12, .12, .36]:
            g.cylinder((x, .24, -.12), .045, .10, 'slate', 12)
    elif number == 8:
        # ENERGY-008: Caja de conexiones fotovoltaicas con ventana translucida
        for sx in [-.24, .24]:
            g.cylinder((sx, .38, 0), .035, .76, 'steel', 12)
        g.box((0, .04, 0), (.68, .08, .44), 'concrete')
        g.box((0, .94, -.04), (.74, .62, .36), 'steel')
        for sx, mat in [(-.18, 'amber'), (0, 'copper'), (.18, 'blue_panel')]:
            g.box((sx, .94, .02), (.08, .36, .06), mat)
        g.box((0, .94, .155), (.62, .48, .025), 'glass')
        g.box((.39, .94, 0), (.05, .18, .10), 'amber')
        for x in [-.22, -.08, .08, .22]:
            g.cylinder((x, .60, -.04), .028, .08, 'rubber', 10)
    elif number == 9:
        # ENERGY-009: Medidor inteligente bidireccional
        g.cylinder((0, .06, 0), .28, .12, 'concrete', 20)
        g.cylinder((0, .62, 0), .065, 1.02, 'steel', 16)
        g.box((0, 1.25, 0), (.52, .58, .32), 'blue_panel')
        g.box((0, 1.28, .165), (.40, .34, .025), 'screen')
        g.text('KWH', (0, 1.22, .185), .105, 'unlit_cyan')
        g.cylinder((0, 1.25, .19), .23, .05, 'glass', 24, axis='Z')
        for sx, m in [(-.12, 'amber'), (.12, 'emerald')]:
            g.sphere((sx, 1.03, .17), (.03, .03, .02), m, 10, 5)
    elif number == 10:
        # ENERGY-010: Gabinete de baterias BESS
        g.box((0, .09, 0), (1.64, .18, 1.02), 'concrete')
        g.box((-.22, 1.16, 0), (1.12, 1.96, .92), 'slate')
        g.box((.54, 1.16, -.12), (.40, 1.96, .68), 'slate')
        # Carril vertical calibrado para el indicador ENERGY-012
        for sx in [.38, .70]:
            g.box((sx, 1.16, .26), (.05, 1.72, .08), 'aluminum')
        for i in range(5):
            g.box((.54, .48 + i * .32, .23), (.26, .025, .03), 'amber')
        # Puerta izquierda y rejillas
        g.box((-.22, 1.22, .47), (.96, 1.56, .03), 'steel')
        for y in [1.72, 1.80, 1.88]:
            g.box((-.22, y, .49), (.76, .03, .02), 'amber')
        g.box((-.22, 2.02, .47), (.86, .16, .04), 'screen')
        g.text('BESS', (-.22, 1.96, .50), .10, 'unlit_gold')
    elif number == 11:
        # ENERGY-011: Bateria modular LiFePO4 (interactiva)
        g.box((0, .18, 0), (.82, .34, .62), 'emerald')
        g.box((0, .36, 0), (.84, .035, .64), 'aluminum')
        for i in range(5):
            g.box((0, .18, -.22 + i * .11), (.85, .26, .025), 'slate')
        for sx in [-.28, .28]:
            g.tube([(sx, .14, .31), (sx, .14, .39), (sx, .25, .39), (sx, .25, .31)], .02, 'aluminum', 8)
        g.cylinder((-.22, .41, .18), .042, .09, 'amber', 14)
        g.cylinder((.22, .41, .18), .042, .09, 'blue_panel', 14)
        g.box((0, .22, .32), (.32, .08, .025), 'screen')
    elif number == 12:
        # ENERGY-012: Indicador fisico de carga (shuttle movil sobre ENERGY-010)
        g.box((0, 0, -.04), (.30, .26, .09), 'aluminum')
        g.box((0, 0, .03), (.24, .18, .07), 'amber')
        g.box((0, 0, .075), (.34, .045, .04), 'unlit_gold')
        for sy in [-.10, .10]:
            g.cylinder((0, sy, -.06), .03, .34, 'copper', 12, axis='X')
    elif number == 13:
        # ENERGY-013: Transformador de distribucion trifasico
        g.box((0, .11, 0), (2.36, .22, 1.76), 'concrete')
        g.box((0, 1.02, 0), (1.46, 1.60, 1.12), 'steel')
        for sx in [-.88, .88]:
            for i in range(7):
                g.box((sx, 1.02, -.42 + i * .14), (.28, 1.26, .035), 'aluminum')
            g.box((sx, 1.60, 0), (.24, .07, .96), 'steel')
            g.box((sx, .44, 0), (.24, .07, .96), 'steel')
        # Tanque conservador superior y aisladores AT/BT
        g.cylinder((-.25, 2.08, -.28), .22, 1.24, 'amber', 20, axis='X')
        for x in [-.32, -.05]:
            g.cylinder((x, 1.90, -.28), .04, .22, 'steel', 10)
        for x in [-.44, 0, .44]:
            for ring_i in range(4):
                g.cylinder((x, 1.92 + ring_i * .11, .22), .095 - ring_i * .01, .06, 'ceramic_cyan', 16)
            g.cylinder((x, 2.08, .22), .035, .52, 'copper', 12)
            g.cylinder((x, 1.90, -.02), .055, .24, 'ceramic_cyan', 14)
    elif number == 14:
        # ENERGY-014: Poste de distribucion electrica
        g.cylinder((0, .14, 0), .42, .28, 'concrete', 20)
        g.cylinder((0, 2.95, 0), .19, 5.45, 'steel', 18, top_radius=.11)
        for y, w in [(4.55, 2.15), (5.25, 1.65)]:
            g.box((0, y, 0), (w, .10, .12), 'amber')
            for sx in [-1, 1]:
                g.tube([(sx * .14, y - .38, 0), (sx * w * .38, y - .04, 0)], .025, 'steel', 6)
            for x in [-w * .44, 0, w * .44]:
                g.cylinder((x, y + .14, 0), .065, .18, 'ceramic_cyan', 14, top_radius=.04)
                g.sphere((x, y + .25, 0), (.03, .03, .03), 'copper', 8, 4)
    elif number == 15:
        # ENERGY-015: Canalizacion de cables (0.68 x 0.18 x 5.0 m)
        g.box((0, .035, 0), (.68, .07, 5.0), 'steel')
        for sx in [-.31, .31]:
            g.box((sx, .095, 0), (.06, .11, 5.0), 'aluminum')
        for sx, mat in [(-.16, 'amber'), (0, 'blue_panel'), (.16, 'emerald')]:
            g.cylinder((sx, .09, 0), .042, 4.96, mat, 12, axis='Z')
        for z in [-2.1, -.7, .7, 2.1]:
            g.box((0, .155, z), (.68, .025, .24), 'amber')
    elif number == 16:
        # ENERGY-016: Torre de aerogenerador (9.0 m altura)
        g.cylinder((0, .16, 0), 1.18, .32, 'concrete', 28)
        g.ring(.86, 1.04, .30, .42, 'steel', 28)
        g.cylinder((0, 4.65, 0), .84, 8.70, 'white_plastic', 28, top_radius=.46)
        g.ring(.50, .54, 7.60, 8.15, 'amber', 28)
        g.cylinder((0, 8.96, 0), .49, .12, 'steel', 28)
        g.box((0, .92, .80), (.52, 1.12, .14), 'steel')
        g.box((0, .36, 1.05), (.68, .12, .42), 'concrete')
    elif number == 17:
        # ENERGY-017: Gondola de turbina (montaje en Y=9.0 sobre ENERGY-016)
        g.cylinder((0, .08, 0), .48, .16, 'steel', 24)
        yz = [(.14, -1.55), (.14, 1.25), (.55, 1.38), (1.18, 1.15), (1.26, -1.30), (.78, -1.65)]
        g.prism_x(yz, -.66, .66, 'white_plastic')
        for sx in [-.68, .68]:
            g.box((sx, .72, -.20), (.04, .24, 1.95), 'amber')
        for i in range(5):
            g.box((0, .52 + i * .12, -1.63), (.88, .04, .08), 'slate')
        g.cylinder((0, .65, 1.36), .26, .24, 'steel', 20, axis='Z')
        g.cylinder((0, 1.38, -.85), .05, .28, 'steel', 10)
        g.sphere((0, 1.54, -.85), (.08, .08, .08), 'red_accent', 12, 6)
    elif number == 18:
        # ENERGY-018: Rotor eolico (origen en el eje del buje, gira en +Z)
        g.cylinder((0, 0, -.12), .28, .36, 'steel', 20, axis='Z')
        g.cylinder((0, 0, .34), .34, .68, 'amber', 24, top_radius=.05, axis='Z')
        stations = [
            (.26, .24, .22, 0.00, .02),
            (.65, .52, .16, 0.34, .03),
            (1.45, .44, .11, 0.24, .02),
            (2.45, .32, .075, 0.14, .01),
            (3.45, .20, .045, 0.06, .00),
            (3.82, .11, .028, 0.02, .00),
        ]
        tip_stations = [
            (3.45, .205, .048, 0.06, .00),
            (3.85, .112, .030, 0.02, .00),
        ]
        for k in range(3):
            ang = TAU * k / 3
            g.loft_blade(stations, 'white_plastic', rot_z=ang)
            g.loft_blade(tip_stations, 'amber', rot_z=ang)
    elif number == 19:
        # ENERGY-019: Modelo de pala eolica educativa seccionada en cuna (interactiva)
        g.box((0, .05, 0), (2.35, .10, .66), 'slate')
        for sx in [-.78, .68]:
            g.box((sx, .28, 0), (.14, .38, .48), 'amber')
        # Seccion aerodinamica horizontal a lo largo de +X con larguero interno visible
        blade_stations = [
            (0.0, .28, .28, 0.0, 0.0),
            (0.45, .62, .22, 0.32, 0.0),
            (1.15, .52, .15, 0.20, 0.0),
            (1.85, .36, .10, 0.10, 0.0),
        ]
        rot = Matrix.Translation(Vector((-1.05, .62, 0))) @ Matrix.Rotation(-math.pi / 2, 4, 'Z')
        sub = Geometry()
        sub.loft_blade(blade_stations, 'white_plastic')
        verts = [tuple(rot @ Vector(v)) for v in sub.vertices]
        g.mesh(verts, sub.faces, 'white_plastic')
        # Larguero estructural interno expuesto en la punta seccionada (+X)
        g.box((.96, .62, 0), (.42, .08, .16), 'amber', (.10, 0, 0))
        g.box((.96, .62, 0), (.44, .11, .04), 'aluminum', (.10, 0, 0))
        g.cylinder((-1.08, .62, 0), .17, .08, 'steel', 18, axis='X')
    elif number == 20:
        # ENERGY-020: Veleta (origen en pivote vertical, gira en +Y)
        g.cylinder((0, 0, 0), .055, .18, 'copper', 16)
        g.cylinder((0, .10, 0), .024, .96, 'aluminum', 10, axis='X')
        # Punta conica hacia -X y aleta doble hacia +X
        g.cylinder((-.50, .10, 0), .065, .20, 'amber', 14, top_radius=.015, axis='X')
        g.box((.38, .20, 0), (.26, .22, .025), 'blue_panel')
        g.box((.42, .02, 0), (.22, .16, .025), 'amber')
    elif number == 21:
        # ENERGY-021: Anemometro de 3 cazoletas (origen en pivote vertical, gira en +Y)
        g.cylinder((0, 0, 0), .06, .16, 'steel', 16)
        g.sphere((0, .10, 0), (.065, .05, .065), 'amber', 12, 6)
        for k in range(3):
            a = TAU * k / 3
            dx, dz = math.cos(a), math.sin(a)
            px, pz = -dz, dx
            g.tube([(0, .06, 0), (.38 * dx, .06, .38 * dz)], .016, 'aluminum', 8)
            g.cylinder((.40 * dx, .06, .40 * dz), .095, .07, 'blue_panel', 14, top_radius=.045, axis='Z' if abs(pz) > .5 else 'X')
            g.sphere((.40 * dx + .02 * px, .06, .40 * dz + .02 * pz), (.085, .085, .085), 'amber', 12, 6)
    elif number == 22:
        # ENERGY-022: Generador electrico seccionado (estator abierto)
        g.box((0, .09, 0), (1.48, .18, 1.28), 'concrete')
        for sx in [-.46, .46]:
            g.box((sx, .38, 0), (.18, .42, .96), 'steel')
        # Carcasa y nucleo del estator con corte didactico superior-frontal (angulo 100° a 355°)
        g.arc_shell(.44, .60, .88, math.radians(85), math.radians(355), 'steel', 20, center=(0, .92, 0), axis='Z')
        g.arc_shell(.35, .44, .76, math.radians(85), math.radians(355), 'slate', 20, center=(0, .92, 0), axis='Z')
        for k in range(9):
            ang = math.radians(98 + k * 29)
            cx, cy = .38 * math.cos(ang), .92 + .38 * math.sin(ang)
            g.box((cx, cy, 0), (.07, .05, .82), 'copper', (0, 0, ang))
        g.ring(.12, .58, -.48, -.42, 'steel', 24, center=(0, .92, 0), axis='Z')
        g.box((-.54, 1.28, -.15), (.26, .28, .32), 'amber')
    elif number == 23:
        # ENERGY-023: Bobina / rotor electromagnetico del generador (interactivo)
        g.cylinder((0, 0, 0), .065, 1.06, 'steel', 18, axis='Z')
        g.cylinder((0, 0, -.04), .18, .58, 'slate', 18, axis='Z')
        for k in range(6):
            ang = TAU * k / 6
            cx, cy = .23 * math.cos(ang), .23 * math.sin(ang)
            g.box((cx, cy, -.04), (.11, .09, .50), 'copper', (0, 0, ang))
            g.box((1.22 * cx, 1.22 * cy, -.04), (.04, .13, .46), 'steel', (0, 0, ang))
        for z in [.32, .40]:
            g.ring(.066, .115, z - .02, z + .02, 'amber', 18, axis='Z')
    elif number == 24:
        # ENERGY-024: Ventilador de demostracion (impulsor axial, origen en eje, gira en +Z)
        g.ring(.52, .58, -.14, .14, 'blue_panel', 28, axis='Z')
        g.cylinder((0, 0, 0), .14, .24, 'steel', 18, axis='Z')
        g.cylinder((0, 0, .16), .14, .14, 'amber', 18, top_radius=.03, axis='Z')
        for k in range(6):
            ang = TAU * k / 6
            cx, cy = .32 * math.cos(ang), .32 * math.sin(ang)
            g.box((cx, cy, 0), (.04, .34, .14), 'aluminum', (.42 * math.sin(ang), -.42 * math.cos(ang), ang))
    elif number == 25:
        # ENERGY-025: Estacion de carga electrica EV
        g.box((0, .08, 0), (1.14, .16, .84), 'concrete')
        g.box((0, 1.16, -.05), (.68, 2.02, .38), 'slate')
        g.box((0, 1.16, .15), (.56, 1.82, .04), 'blue_panel')
        g.box((0, 2.22, .08), (.86, .14, .68), 'amber', (.15, 0, 0))
        g.box((0, 1.48, .18), (.42, .32, .03), 'screen')
        g.text('EV', (0, 1.38, .205), .15, 'unlit_cyan')
        g.box((0, 1.06, .18), (.36, .06, .03), 'emerald')
        for sx in [-.37, .37]:
            g.box((sx, 1.12, .04), (.08, .20, .16), 'amber')
        pts = [(.39, 1.08, .06), (.48, .78, .12), (.46, .48, .04), (.38, .72, -.04), (.38, 1.04, .02)]
        g.tube(pts, .024, 'rubber', 8)
    elif number == 26:
        # ENERGY-026: Vehiculo electrico educativo (mirando +Z, vidrio translucido)
        body_yz = [(.24, -1.88), (.24, 1.88), (.58, 1.92), (.78, 1.25),
                   (1.38, .55), (1.42, -.85), (.88, -1.68), (.56, -1.92)]
        g.prism_x(body_yz, -.84, .84, 'blue_panel')
        # Paquete de bateria inferior resaltado en los estribos
        g.box((0, .22, 0), (1.72, .10, 2.15), 'amber')
        # Cabina/parabrisas translucido
        g.box((0, 1.08, .82), (1.48, .44, .58), 'glass', (-.52, 0, 0))
        g.box((0, 1.12, -.12), (1.52, .38, 1.18), 'glass')
        # Ruedas y rines metalicos
        for sx in [-.84, .84]:
            for sz in [-1.18, 1.18]:
                g.cylinder((sx, .32, sz), .32, .22, 'rubber', 20, axis='X')
                g.cylinder((sx + (.02 if sx > 0 else -.02), .32, sz), .20, .23, 'aluminum', 16, axis='X')
        # Barra de luces y puerto de carga lateral
        g.box((0, .62, 1.91), (1.36, .08, .05), 'unlit_cyan')
        g.box((0, .66, -1.91), (1.32, .08, .05), 'red_accent')
        g.cylinder((-.86, .74, 1.02), .09, .04, 'emerald', 14, axis='X')
    elif number == 27:
        # ENERGY-027: Conector de carga educativo CCS / Tipo-2 (interactivo)
        g.box((0, .04, -.04), (.36, .08, .44), 'slate')
        g.box((0, .22, -.08), (.22, .28, .32), 'amber', (-.25, 0, 0))
        g.tube([(0, .28, -.22), (0, .42, -.02), (0, .38, .18)], .052, 'blue_panel', 12)
        g.cylinder((0, .38, .28), .115, .20, 'slate', 20, axis='Z')
        g.ring(.095, .12, .34, .42, 'amber', 20, center=(0, .38, 0), axis='Z')
        for px, py in [(-.045, .41), (.045, .41), (-.055, .36), (0, .36), (.055, .36), (-.035, .31), (.035, .31)]:
            g.cylinder((px, py, .36), .014, .10, 'copper', 10, axis='Z')
        g.tube([(0, .16, -.22), (0, .10, -.36), (0, .06, -.46)], .036, 'rubber', 10)
    elif number == 28:
        # ENERGY-028: Panel informativo energetico con diagrama UV embebido
        for sx in [-1.14, 1.14]:
            g.cylinder((sx, .06, 0), .18, .12, 'concrete', 16)
            g.box((sx, 1.14, 0), (.11, 2.16, .12), 'steel')
        g.box((0, 1.52, 0), (2.56, 1.44, .12), 'slate')
        g.box((0, 1.44, .072), (2.28, 1.12, .025), 'diagram_tex', uv_box=(-1.14, 1.14, 0.88, 2.00, 'XY'))
        g.box((0, 2.15, .04), (2.68, .34, .18), 'screen')
        g.text('ENERGIA LIMPIA', (0, 2.04, .15), .17, 'unlit_gold')
    else:
        raise ValueError(number)
    return g


def build(destination):
    destination = Path(destination).resolve()
    assert not (destination / 'sources/ecoenergy.blend').exists(), 'Se requiere una salida nueva'
    assert not any((destination / 'models').glob('*.glb')), 'No se reemplazan GLB existentes'
    for folder in ['models', 'sources', 'textures']:
        (destination / folder).mkdir(parents=True, exist_ok=True)
    original = bpy.context.window.scene
    scene = bpy.data.scenes.new('EcoSphere_EcoEnergy_6A')
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0

    specs = {
        'warm_ground': ((.58, .50, .36), 0.0, .92),
        'concrete': ((.48, .50, .50), 0.0, .88),
        'pavement': ((.36, .39, .42), 0.0, .86),
        'slate': ((.14, .18, .22), .15, .72),
        'steel': ((.44, .49, .54), .82, .30),
        'aluminum': ((.72, .76, .80), .88, .24),
        'copper': ((.78, .38, .14), .90, .26),
        'amber': ((.88, .52, .05), .25, .34),
        'solar_gold': ((.96, .70, .12), .35, .30),
        'blue_panel': ((.06, .34, .68), .20, .30),
        'ceramic_cyan': ((.14, .52, .66), .10, .26),
        'emerald': ((.10, .56, .32), .18, .36),
        'white_plastic': ((.90, .92, .94), 0.0, .34),
        'red_accent': ((.82, .14, .10), .10, .38),
        'rubber': ((.06, .07, .08), 0.0, .94),
        'solar_tex': ((1.0, 1.0, 1.0), .22, .26),
        'diagram_tex': ((1.0, 1.0, 1.0), 0.0, .55),
    }
    palette = {k: make_material(k, *v) for k, v in specs.items()}
    palette['glass'] = make_material('glass', (.16, .52, .68), 0.0, .14, alpha=.42, double_sided=True)
    palette['screen'] = make_material('screen', (.010, .024, .038), unlit=True)
    palette['unlit_gold'] = make_material('unlit_gold', (.98, .78, .22), unlit=True)
    palette['unlit_cyan'] = make_material('unlit_cyan', (.32, .88, .96), unlit=True)

    report = []
    logs = io.StringIO()
    objects = []
    try:
        bpy.context.window.scene = scene
        build_textures(destination, palette)
        for number, (ident, name, category) in enumerate(CATALOG, 1):
            prev = bpy.data.objects.get(ident)
            if prev:
                assert prev.users_scene and all(s.name.startswith('EcoSphere_EcoEnergy') for s in prev.users_scene), \
                    'ID ocupado por objeto ajeno: ' + ident
                prev.name = ident + '_previous'
            g = create_model(number)
            mesh = bpy.data.meshes.new(ident + '_Geometry')
            mesh.from_pydata([b(v) for v in g.vertices], [], g.faces)
            keys = list(dict.fromkeys(g.materials))
            for key in keys:
                mesh.materials.append(palette[key])
            for idx, (polygon, key) in enumerate(zip(mesh.polygons, g.materials)):
                polygon.material_index = keys.index(key)

            # Capa UV en malla original antes de triangular para preservar uv_boxes exactos
            uv = mesh.uv_layers.new(name='UVMap')
            for poly_idx, poly in enumerate(mesh.polygons):
                box = g.uv_boxes.get(poly_idx)
                for loop_idx in poly.loop_indices:
                    co = mesh.vertices[mesh.loops[loop_idx].vertex_index].co
                    # Recordar: co en Blender es (X, -Z_engine, Y_engine)
                    ex, ey, ez = co.x, co.z, -co.y
                    if box and box[4] == 'XZ':
                        u = (ex - box[0]) / max(box[1] - box[0], 1e-6)
                        v = (ez - box[2]) / max(box[3] - box[2], 1e-6)
                    elif box and box[4] == 'XY':
                        u = (ex - box[0]) / max(box[1] - box[0], 1e-6)
                        v = (ey - box[2]) / max(box[3] - box[2], 1e-6)
                    else:
                        u, v = ex * .25 + .5, (ey + ez) * .25 + .5
                    uv.data[loop_idx].uv = (min(max(u, 0.0), 1.0) if box else u,
                                            min(max(v, 0.0), 1.0) if box else v)

            bm = bmesh.new()
            bm.from_mesh(mesh)
            bmesh.ops.triangulate(bm, faces=list(bm.faces))
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            for face in bm.faces:
                assert face.calc_area() > 1e-9, f'Triangulo degenerado en {ident}'
            bm.to_mesh(mesh)
            bm.free()
            mesh.update()
            mesh.calc_loop_triangles()

            obj = bpy.data.objects.new(ident, mesh)
            scene.collection.objects.link(obj)
            objects.append(obj)
            obj['ecosphere_model_id'] = ident
            obj['ecosphere_category'] = category
            bpy.ops.object.select_all(action='DESELECT')
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            glb = destination / 'models' / f'{ident}.glb'
            with contextlib.redirect_stdout(logs):
                res = bpy.ops.export_scene.gltf(
                    filepath=str(glb),
                    export_format='GLB',
                    use_selection=True,
                    use_active_scene=True,
                    export_yup=True,
                    export_normals=True,
                    export_texcoords=True,
                    export_animations=False,
                    export_cameras=False,
                    export_lights=False,
                    export_materials='EXPORT',
                    export_draco_mesh_compression_enable=False,
                    export_tangents=False,
                    export_attributes=False,
                )
            assert 'FINISHED' in res and glb.stat().st_size > 0
            data = glb.read_bytes()
            jlen = int.from_bytes(data[12:16], 'little')
            doc = json.loads(data[20:20 + jlen])
            assert set(doc.get('extensionsUsed', [])) <= {'KHR_materials_unlit'}, doc.get('extensionsUsed')
            minima = [min(v[a] for v in g.vertices) for a in range(3)]
            dims = [max(v[a] for v in g.vertices) - minima[a] for a in range(3)]
            norm_pts = sorted(set(
                tuple(round((v[a] - minima[a]) / max(dims[a], 1e-9), 5) for a in range(3))
                for v in g.vertices
            ))
            sig = hashlib.sha256(repr(norm_pts).encode()).hexdigest()
            report.append({
                'model_id': ident,
                'name': name,
                'category': category,
                'triangles': len(mesh.loop_triangles),
                'vertices_blender': len(mesh.vertices),
                'dimensions_m': dims,
                'materials': keys,
                'bytes': len(data),
                'sha256': hashlib.sha256(data).hexdigest(),
                'geometry_signature': sig,
            })
            print(f'EXPORTED {ident}: {len(mesh.loop_triangles)} triangles; {len(data)} bytes')

        assert len({r['geometry_signature'] for r in report}) == 28, 'Geometrias duplicadas en catalogo EcoEnergy'
        for i, obj in enumerate(objects):
            obj.location = ((i % 7) * 42.0, (i // 7) * 42.0, 0.0)
        bpy.data.libraries.write(str(destination / 'sources/ecoenergy.blend'), {scene}, compress=True)
        (destination / 'export-report.json').write_text(
            json.dumps({'blender': bpy.app.version_string, 'models': report}, indent=2) + '\n'
        )
        (destination / 'export.log').write_text(logs.getvalue())
        print(f'ECOENERGY EXPORT PASS: 28 distinct models; {sum(r["triangles"] for r in report)} source triangles; editable library saved')
    finally:
        bpy.context.window.scene = original
        print('Original scene restored:', original.name)


if __name__ == '__main__':
    import sys
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if len(args) == 1:
        build(args[0])
    else:
        raise RuntimeError('Usar -- /ruta/nueva/de/salida')
