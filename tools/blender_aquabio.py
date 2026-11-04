"""28 diseños propios AquaBio. Blender MCP o: blender --background --python este.py -- salida_nueva.

Reutiliza únicamente primitivas de Geometry; no llama al generador EcoEnergy.
Metros, +Y arriba en el motor; biblioteca aislada, escena original conservada.
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
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from blender_ecoenergy import Geometry, LETTERS, b

LETTERS.update({
    'Q': [[(0, 0), (0, 1), (1, 1), (1, 0), (0, 0)], [(.6, .3), (1.1, -.1)]],
    'X': [[(0, 0), (1, 1)], [(0, 1), (1, 0)]],
    'Z': [[(0, 1), (1, 1), (0, 0), (1, 0)]],
    'J': [[(0, 1), (1, 1), (1, 0), (0, 0), (0, .3)]],
})

CATALOG = [
    ('Plataforma de cuenca AquaBio', 'arquitectura', 'Organiza el recorrido desde la captacion hasta la conservacion'),
    ('Portico de gotas y ondas', 'arquitectura', 'Identifica el ingreso y el uso responsable del agua'),
    ('Sendero con drenaje lateral', 'arquitectura', 'Distingue circulacion peatonal de escorrentia superficial'),
    ('Cubierta recolectora de lluvia', 'captacion', 'Interception de lluvia y conduccion por pendiente hacia canaleta'),
    ('Canaleta basculante', 'captacion', 'Ilustra el ajuste de pendiente de un canal abierto'),
    ('Cisterna seccionada', 'almacenamiento', 'Almacena lluvia con tapa rebose y primer desvio ilustrativo'),
    ('Rejilla de captacion', 'captacion', 'Retiene residuos gruesos antes del almacenamiento'),
    ('Carcasa de bomba en voluta', 'bombeo', 'Muestra aspiracion axial e impulsion tangencial'),
    ('Impulsor centrifugo', 'bombeo', 'Transfiere energia mecanica al agua; giro cinematico'),
    ('Tanque sedimentador seccionado', 'tratamiento', 'Separa parte de los solidos suspendidos por gravedad'),
    ('Barredor radial de sedimentos', 'tratamiento', 'Representa recoleccion lenta de lodos depositados'),
    ('Filtro de arena estratificado', 'tratamiento', 'Retencion de particulas en un lecho granular'),
    ('Columna de carbon activado', 'tratamiento', 'Adsorcion de algunos compuestos; requiere mantenimiento'),
    ('Cartucho filtrante plisado', 'tratamiento', 'Elemento desmontable para inspeccion y recambio'),
    ('Camara UV seccionada', 'tratamiento', 'Desinfeccion requiere dosis caudal y transmitancia controlados'),
    ('Deposito de agua tratada', 'almacenamiento', 'Evita recontaminacion; tratada no implica potable'),
    ('Colector de tuberias', 'conduccion', 'Distribucion mediante codos derivaciones y bridas'),
    ('Valvula de volante manual', 'conduccion', 'Identifica un elemento de regulacion sin simular caudal'),
    ('Medidor de flujo con indicador', 'medicion', 'Representa un indicador rotativo sin lecturas reales'),
    ('Estacion de muestreo', 'medicion', 'Necesidad de controles microbiologicos y quimicos'),
    ('Rueda del ciclo hidrologico', 'ciclo', 'Conecta evaporacion condensacion precipitacion e infiltracion'),
    ('Nube con precipitacion', 'ciclo', 'Representacion conceptual de gotas y almacenamiento'),
    ('Cuenca de humedal con sustratos', 'biodiversidad', 'Habitat y retencion de agua; no garantiza potabilidad'),
    ('Roseta flotante de hojas', 'biodiversidad', 'Cobertura vegetal y habitat superficial; oscilacion ilustrativa'),
    ('Macolla de juncos', 'biodiversidad', 'Raices y tallos ofrecen refugio y protegen orillas'),
    ('Arbol de ribera', 'biodiversidad', 'Sombra conectividad y estabilizacion de ribera; especie no identificada'),
    ('Refugio de polinizadores', 'biodiversidad', 'Cavidades de refugio junto a vegetacion de ribera'),
    ('Pez educativo', 'biodiversidad', 'Fauna dependiente de habitat oxigenacion y calidad del agua'),
]


def board(g, title, lines, center=(0, 1.45, .8), width=3.2, readable=True):
    x, y, z = center
    g.mechanism_bounds = [[min(v[a] for v in g.vertices) for a in range(3)],
                          [max(v[a] for v in g.vertices) for a in range(3)]]
    heights = [min(.24 if readable and i == 0 else .18 if readable else .17,
                   (width - .25) / max(1, len(line)) / .82) for i, line in enumerate([title] + lines)]
    g.signs = [dict(title=title, lines=lines, center=center, width=width, height=1.22,
                    letter_heights=heights, bounds=[[x-width/2,y-.61,z-.06],[x+width/2,y+.61,z+.10]])]
    g.box((x, y, z), (width, 1.22, .12), 'signboard' if readable else 'slate')
    g.box((x, y + .49, z + .07), (width, .055, .03), 'turquoise')
    for index, line in enumerate([title] + lines):
        h = heights[index]
        g.text(line, (x, y + .26 - index * .29, z + .085), h, 'letters')
    for dx in (-width * .42, width * .42):
        g.box((x + dx, (y - .61) / 2, z), (.08, y - .61, .1), 'steel')


def create_model(n):
    g = Geometry()
    if n == 1:
        g.box((0, -.20, 0), (48, .36, 48), 'earth')
        g.box((-7, -.025, 0), (30, .07, 46), 'stone')
        for x in (-23.7, 23.7):
            g.box((x, .03, 0), (.18, .10, 47.6), 'turquoise')
        for z in (-23.7, 23.7):
            g.box((0, .03, z), (47.6, .1, .18), 'blue')
        # Franja escalonada de ribera, distinta de la plataforma solar.
        for i in range(4):
            g.box((8.2 + i * .45, -.015, -2), (.38, .08, 36 - i), 'sand')
    elif n == 2:
        for x in (-2.6, 2.6):
            g.cylinder((x, .15, 0), .45, .3, 'stone', 12)
            g.box((x, 1.8, 0), (.32, 3.3, .44), 'blue')
            for y in (.8, 1.5, 2.2):
                g.sphere((x, y, .25), (.14, .23, .09), 'turquoise', 8, 4)
        g.tube([(-2.6, 3.4, 0), (-1.4, 3.8, 0), (0, 3.55, 0), (1.4, 3.8, 0), (2.6, 3.4, 0)], .17, 'turquoise')
        board(g, 'AQUABIO', ['CAPTAR TRATAR CONSERVAR', 'CADA GOTA CUENTA'], (0, 2.9, .1), 4.5, readable=False)
    elif n == 3:
        g.box((0, .025, 0), (2.8, .05, 8.0), 'stone')
        for z in (-3.6, -2.4, -1.2, 0, 1.2, 2.4, 3.6):
            g.box((0, .056, z), (2.65, .015, .045), 'sand')
        for x in (-1.475, 1.475):
            g.box((x, .015, 0), (.17, .03, 8.0), 'blue')
            for z in (-3.6, -2.4, -1.2, 0, 1.2, 2.4, 3.6):
                g.box((x, .036, z), (.16, .02, .13), 'steel')
    elif n == 4:
        for x in (-1.9, 1.9):
            for z in (-1.45, 1.45):
                g.box((x, 1.6, z), (.13, 3.2, .13), 'steel')
        for x in (-1.05, 1.05):
            g.box((x, 3.2, 0), (2.2, .09, 3.35), 'blue', (0, 0, math.copysign(.14, x)))
        for z in (-1.65, 1.65):
            g.box((0, 3.2, z), (4.4, .16, .12), 'turquoise')
        g.tube([(0, 2.95, -2.02), (0, 2.1, -2.02), (0, .25, -2.02)], .08, 'steel')
        board(g, 'LLUVIA', ['CAPTAR LA LLUVIA', 'DESVIAR PRIMER LAVADO'], (-3.6, 1.5, .5))
    elif n == 5:
        # Pivote central: canal en U abierto sin techo; bascula alrededor de Z.
        g.box((0, -.15, 0), (3.5, .08, .42), 'turquoise')
        for z in (-.24, .24):
            g.box((0, .015, z), (3.5, .30, .06), 'steel')
        for x in (-1.3, 1.3):
            g.ring(.07, .13, -.10, .10, 'steel', 12, (x, -.19, 0), 'Z')
        g.tube([(1.7, -.14, 0), (1.95, -.22, 0), (1.95, -.52, 0)], .09, 'blue')
    elif n == 6:
        g.cylinder((0, .10, 0), 1.2, .2, 'stone', 24)
        g.arc_shell(.92, 1.05, 1.75, -.1, math.pi + .1, 'blue', 20, (0, 1.12, 0), 'Y')
        for y in (.3, 2.0):
            g.ring(.92, 1.08, y, y + .10, 'steel', 24)
        g.cylinder((0, .73, 0), .91, 1.0, 'water', 24)
        g.cylinder((0, 2.18, 0), 1.12, .12, 'turquoise', 24)
        g.cylinder((.2, 2.3, 0), .3, .12, 'steel', 16)
        g.tube([(-1.0, 1.6, 0), (-1.38, 1.6, 0), (-1.38, .2, 0)], .085, 'steel')
        board(g, 'CISTERNA', ['NO POTABILIZA', 'TAPA REBOSE LIMPIEZA'], (-3.2, 1.5, 0))
    elif n == 7:
        g.box((0, .10, 0), (1.8, .2, 1.2), 'stone')
        for x in (-.83, .83):
            g.box((x, .23, 0), (.08, .12, 1.05), 'steel')
        for x in range(-6, 7):
            g.box((x * .12, .23, 0), (.045, .09, 1.0), 'steel')
        g.tube([(-.5, .32, -.3), (-.2, .35, -.1), (.0, .32, -.2)], .035, 'earth')
    elif n == 8:
        g.box((0, .12, 0), (1.7, .24, 1.25), 'stone')
        g.arc_shell(.45, .65, .44, -.6, 4.25, 'blue', 22, (0, .90, 0), 'Z')
        g.tube([(.35, 1.45, 0), (.7, 1.6, 0), (.95, 1.6, 0)], .14, 'steel', 12)
        g.ring(.16, .25, .94, 1.04, 'steel', 16, (0, 1.6, 0), 'X')
        g.cylinder((0, .90, -.42), .29, .40, 'steel', 16, axis='Z')
        board(g, 'BOMBA', ['GIRO A IMPULSION', 'SIN CAUDAL REAL'], (-3, 1.5, -.2), 2.6)
    elif n == 9:
        # Eje +Z en el centro del disco, muestra interactiva aparte.
        g.cylinder((0, 0, 0), .43, .08, 'steel', 20, axis='Z')
        g.cylinder((0, 0, .10), .12, .2, 'turquoise', 12, axis='Z')
        for i in range(6):
            a = math.tau * i / 6
            pts = [(r * math.cos(a + q), r * math.sin(a + q), .08) for r, q in ((.12, 0), (.27, .17), (.40, .38))]
            g.tube(pts, .035, 'turquoise', 6)
    elif n == 10:
        g.cylinder((0, .12, 0), 1.65, .24, 'stone', 32)
        g.arc_shell(1.42, 1.58, 1.4, -.15, math.pi + .15, 'sand', 24, (0, .92, 0), 'Y')
        g.cylinder((0, .32, 0), 1.40, .16, 'earth', 32)
        g.cylinder((0, .54, 0), 1.39, .26, 'water', 32)
        g.ring(1.4, 1.62, 1.60, 1.72, 'blue', 32)
        g.tube([(-1.5, 1.3, 0), (-1.9, 1.3, 0), (-1.9, .5, 0)], .10, 'steel')
        board(g, 'SEDIMENTACION', ['SEPARA SOLIDOS', 'NO ELIMINA RIESGOS'], (-4, 1.5, 0), 3.6)
    elif n == 11:
        g.cylinder((0, 0, 0), .13, .36, 'steel', 12)
        for a in (0, math.pi):
            x, z = 1.28 * math.cos(a), 1.28 * math.sin(a)
            g.tube([(0, 0, 0), (x, 0, z)], .065, 'steel')
            for d in (.45, .85, 1.2):
                g.box((d * math.cos(a), -.17, d * math.sin(a)), (.16, .30, .28), 'turquoise', (0, -a, 0))
    elif n in (12, 13):
        r = .56 if n == 12 else .44
        g.box((0, .08, 0), (1.45, .16, 1.3), 'stone')
        for y in (.25, 1.85):
            g.ring(r - .06, r + .09, y, y + .11, 'steel', 20)
        g.arc_shell(r - .04, r + .03, 1.5, 0, math.pi, 'blue', 16, (0, 1.05, 0), 'Y')
        if n == 12:
            for y, h, mat in ((.48, .3, 'gravel'), (.87, .45, 'sand'), (1.35, .44, 'fine_sand')):
                g.cylinder((0, y, 0), r - .07, h, mat, 20)
            board(g, 'ARENA', ['RETIENE PARTICULAS', 'LAVAR Y MANTENER'], (-2.5, 1.5, 0), 2.9)
        else:
            for i in range(6):
                for j in range(4):
                    a = math.tau * j / 4 + i * .45
                    g.sphere((.23 * math.cos(a), .40 + i * .22, .23 * math.sin(a)), (.13, .11, .13), 'carbon', 6, 4)
            g.cylinder((0, 1.99, 0), .34, .13, 'turquoise', 12)
            board(g, 'CARBON ACTIVADO', ['ADSORBE COMPUESTOS', 'NO DESINFECTA TODO'], (-2.5, 1.5, 0), 3.3)
    elif n == 14:
        g.cylinder((0, .43, 0), .14, .86, 'white', 12)
        for i in range(16):
            a = math.tau * i / 16
            g.box((.19 * math.cos(a), .43, .19 * math.sin(a)), (.075, .72, .06), 'fine_sand', (0, -a, 0))
        for y in (.04, .83):
            g.ring(.11, .27, y - .04, y + .04, 'turquoise', 16)
        g.tube([(-.13, .88, 0), (-.13, 1.04, 0), (.13, 1.04, 0), (.13, .88, 0)], .028, 'steel')
    elif n == 15:
        for x in (-.8, .8):
            g.box((x, .4, 0), (.20, .8, .7), 'blue')
            g.ring(.28, .39, x - .09, x + .09, 'steel', 20, (0, .85, 0), 'X')
        g.arc_shell(.3, .35, 1.6, 0, math.pi, 'steel', 20, (0, .85, 0), 'Z')
        # Lampara horizontal visible por corte; sin representar un chorro ni emitir UV real.
        g.cylinder((0, .85, 0), .08, 1.75, 'uv', 12, axis='X')
        board(g, 'DESINFECCION UV', ['CONTROLAR DOSIS', 'NO CERTIFICA POTABLE'], (2.8, 1.5, -.4), 3.6)
    elif n == 16:
        g.box((0, .12, 0), (2.5, .24, 2.1), 'stone')
        g.box((0, 1.18, 0), (2.0, 1.95, 1.6), 'turquoise')
        for y in (.45, 1.1, 1.8):
            g.box((0, y, .83), (2.12, .08, .12), 'steel')
        g.box((0, 2.22, 0), (2.2, .16, 1.8), 'blue')
        g.cylinder((.55, 2.4, 0), .22, .24, 'steel', 12)
        board(g, 'AGUA TRATADA', ['EVITAR CONTAMINAR', 'NO CERTIFICA POTABLE'], (-3.5, 1.5, 0), 3.7)
    elif n == 17:
        g.tube([(-1.6, .6, 0), (1.6, .6, 0)], .12, 'steel', 12)
        for x in (-1.1, 0, 1.1):
            g.tube([(x, .6, 0), (x, .6, -.7), (x, .22, -.7)], .085, 'blue', 10)
            g.ring(.12, .23, x - .045, x + .045, 'steel', 12, (0, .6, 0), 'X')
            g.box((x, .18, 0), (.18, .36, .32), 'stone')
    elif n == 18:
        g.cylinder((0, .28, 0), .22, .75, 'steel', 16, axis='X')
        g.cylinder((0, .53, 0), .12, .50, 'blue', 12)
        g.ring(.28, .36, .78, .84, 'turquoise', 24)
        for a in range(4):
            angle = math.tau * a / 4
            g.tube([(0, .81, 0), (.30 * math.cos(angle), .81, .30 * math.sin(angle))], .028, 'steel', 6)
        for x in (-.37, .37):
            g.ring(.13, .28, x - .045, x + .045, 'blue', 12, (0, .28, 0), 'X')
    elif n == 19:
        # Indicador completo en +Z: escala sin cifras, sin afirmar una lectura medida.
        g.ring(.30, .37, -.06, .06, 'steel', 24, axis='Z')
        g.cylinder((0, 0, -.04), .30, .04, 'slate', 24, axis='Z')
        g.box((.09, .0, .04), (.28, .045, .04), 'letters')
        g.cylinder((0, 0, .06), .05, .06, 'turquoise', 12, axis='Z')
        for i in range(8):
            a = math.tau * i / 8
            g.box((.25 * math.cos(a), .25 * math.sin(a), .01), (.04, .04, .02), 'white')
    elif n == 20:
        g.box((0, .95, 0), (2.35, .15, 1.0), 'steel')
        for x in (-.95, .95):
            g.box((x, .45, 0), (.10, .9, .75), 'blue')
        for x in (-.62, 0, .62):
            g.cylinder((x, 1.18, 0), .12, .38, 'glass', 12)
            g.cylinder((x, 1.40, 0), .14, .07, 'turquoise', 12)
        board(g, 'MUESTREO', ['ANALIZAR MICROBIOS', 'Y QUIMICA SIN DATOS'], (3.1, 1.5, 0), 3.6)
    elif n == 21:
        # Rueda expositiva independiente, pivote central +Z; textos fijos en la nube AQUA-022.
        g.ring(1.15, 1.32, -.08, .08, 'turquoise', 32, axis='Z')
        g.cylinder((0, 0, 0), .19, .22, 'blue', 16, axis='Z')
        for i, mat in enumerate(('water', 'white', 'sand', 'green')):
            a = math.tau * i / 4
            g.tube([(0, 0, 0), (1.12 * math.cos(a), 1.12 * math.sin(a), 0)], .065, 'steel')
            g.sphere((1.15 * math.cos(a), 1.15 * math.sin(a), 0), (.25, .25, .14), mat, 8, 4)
            g.box((1.30 * math.cos(a), 1.30 * math.sin(a), .1), (.34, .13, .05), mat, (0, 0, a + .5))
    elif n == 22:
        g.box((0, .10, 0), (3.8, .2, 1.7), 'stone')
        # Soporte estacionario de AQUA-021, seis metros al oeste de la nube.
        g.box((-6, .10, -.2), (.8, .2, .7), 'stone')
        g.box((-6, .9, -.2), (.16, 1.6, .22), 'steel')
        g.cylinder((-6, 1.8, -.08), .14, .20, 'steel', 12, axis='Z')
        for x in (-1.3, 1.3):
            g.box((x, 1.65, -.20), (.12, 3.1, .14), 'steel')
        for x, y, r in ((-1.0, 3.3, .65), (0, 3.5, .85), (1.0, 3.3, .65)):
            g.sphere((x, y, 0), (r, r * .65, .40), 'white', 12, 6)
        for i in range(5):
            g.sphere((-1.2 + i * .6, 2.5 - (i % 2) * .3, .06), (.10, .22, .10), 'water', 8, 4)
        board(g, 'CICLO DEL AGUA', ['EVAPORAR Y LLOVER', 'INFILTRAR ALMACENAR'], (0, 1.5, .8), 3.8)
    elif n == 23:
        g.box((0, .02, 0), (7.6, .34, 6.0), 'earth')
        for x in (-3.65, 3.65):
            g.box((x, .30, 0), (.24, .55, 6.0), 'stone')
        for z in (-2.85, 2.85):
            g.box((0, .30, z), (7.2, .55, .24), 'sand')
        g.box((0, .24, 0), (7.0, .10, 5.5), 'water')
        for x in (-2.7, 2.7):
            for z in (-1.8, 0, 1.8):
                g.sphere((x, .33, z), (.48, .22, .37), 'gravel', 8, 4)
        board(g, 'HUMEDAL', ['HABITAT Y RETENCION', 'CUIDAR BIODIVERSIDAD'], (-5.9, 1.5, -1.8), 3.8)
    elif n == 24:
        for i in range(7):
            a = math.tau * i / 7
            g.sphere((.38 * math.cos(a), .015, .38 * math.sin(a)), (.37, .045, .18), 'green', 8, 4)
        g.sphere((0, .08, 0), (.13, .12, .13), 'flower', 8, 4)
        for i in range(5):
            g.tube([(0, -.04, 0), ((i - 2) * .035, -.35, .04)], .012, 'earth', 5)
    elif n == 25:
        for i in range(7):
            a = i * 2.4
            x, z = .18 * math.cos(a), .18 * math.sin(a)
            h = 1.1 + .16 * (i % 3)
            g.tube([(x, 0, z), (x + .06, h * .6, z), (x + .13, h, z)], .025, 'green', 6)
            g.cylinder((x + .13, h + .12, z), .055, .24, 'earth', 8)
            g.tube([(x, .15, z), (x - .25, h * .75, z + .2)], .022, 'reed', 5)
    elif n == 26:
        g.cylinder((0, .08, 0), .52, .16, 'earth', 10)
        g.tube([(0, 0, 0), (.08, 1.3, 0), (-.12, 2.1, 0)], .16, 'bark', 7)
        for x, y, z in ((-.8, 2.5, .1), (.7, 2.7, -.15), (0, 3.0, .4)):
            g.tube([(0, 1.2, 0), (x, y - .3, z)], .09, 'bark', 6)
            g.sphere((x, y, z), (.8, .60, .70), 'reed', 9, 5)
        for x in (-.42, .42):
            g.tube([(0, .4, 0), (x, .03, .3)], .065, 'bark', 6)
    elif n == 27:
        for x in (-.57, .57):
            g.box((x, 1.0, 0), (.12, 2.0, .12), 'bark')
        g.box((0, 1.40, 0), (1.28, 1.10, .40), 'sand')
        for x in (-.38, -.12, .14, .40):
            for y in (1.12, 1.40, 1.68):
                g.ring(.055, .09, .21, .42, 'bark', 8, (x, y, 0), 'Z')
        for x in (-.4, .4):
            g.box((x, 2.08, 0), (.92, .12, .75), 'blue', (0, 0, math.copysign(-.4, x)))
        board(g, 'POLINIZADORES', ['REFUGIO Y CONECTIVIDAD', 'PROTEGER LAS RIBERAS'], (0, .6, .5), 2.9, readable=False)
    elif n == 28:
        g.sphere((0, .35, 0), (.65, .29, .20), 'fish', 12, 6)
        g.prism_x([(.28, -.08), (.65, -.10), (.35, .08)], -.15, .26, 'turquoise')
        g.prism_x([(.12, -.06), (.50, -.06), (.31, .08)], -.78, -.55, 'blue')
        for z in (-.195, .195):
            g.sphere((.40, .44, z), (.035, .035, .025), 'carbon', 8, 4)
        g.tube([(.24, .25, .17), (.24, .42, .19), (.24, .52, .13)], .015, 'blue', 5)
    else:
        raise ValueError(n)
    return g


def material(name, color, metallic=0, roughness=.75, alpha=1, unlit=False):
    m = bpy.data.materials.new('Aqua_' + name)
    m.use_nodes = True
    m.use_backface_culling = alpha == 1
    tree = m.node_tree
    if unlit:
        tree.nodes.clear()
        out = tree.nodes.new('ShaderNodeOutputMaterial')
        shader = tree.nodes.new('ShaderNodeBackground')
        shader.inputs['Color'].default_value = (*color, 1)
        tree.links.new(shader.outputs[0], out.inputs['Surface'])
    else:
        shader = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
        for key, value in {'Base Color': (*color, 1), 'Metallic': metallic, 'Roughness': roughness, 'Alpha': alpha}.items():
            shader.inputs[key].default_value = value
        if alpha < 1:
            m.surface_render_method = 'BLENDED'
    return m


def build(destination):
    destination = Path(destination).resolve()
    assert not any(destination.rglob('*.glb')) and not any(destination.rglob('*.blend')), 'Usar salida nueva'
    for folder in ('models', 'textures', 'sources'):
        (destination / folder).mkdir(parents=True, exist_ok=True)
    original = bpy.context.window.scene
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    scene = bpy.data.scenes.new('EcoSphere_AquaBio_6B')
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    specs = {
        'earth': ((.30, .23, .13), 0, .94), 'stone': ((.55, .61, .60), 0, .90),
        'sand': ((.64, .53, .32), 0, .92), 'fine_sand': ((.82, .73, .49), 0, .90),
        'gravel': ((.37, .42, .39), 0, .94), 'blue': ((.035, .24, .49), .12, .45),
        'turquoise': ((.02, .53, .56), .15, .35), 'steel': ((.62, .69, .72), .83, .28),
        'water': ((.04, .38, .55), .12, .24), 'white': ((.86, .92, .93), 0, .55),
        'slate': ((.018, .052, .067), 0, .9), 'carbon': ((.032, .039, .041), 0, .94),
        'green': ((.10, .42, .19), 0, .85), 'reed': ((.27, .49, .15), 0, .88),
        'bark': ((.28, .16, .07), 0, .95), 'flower': ((.93, .66, .23), 0, .68),
        'fish': ((.24, .63, .66), .20, .4),
    }
    palette = {k: material(k, *v) for k, v in specs.items()}
    palette['glass'] = material('glass', (.26, .67, .73), roughness=.18, alpha=.48)
    palette['signboard'] = material('signboard', (.012, .032, .042), unlit=True)
    palette['letters'] = material('letters', (.73, .97, .91), unlit=True)
    palette['uv'] = material('uv_illustrative', (.45, .30, .9), unlit=True)
    logs, report, objects = io.StringIO(), [], []
    try:
        bpy.context.window.scene = scene
        for n, (name, category, purpose) in enumerate(CATALOG, 1):
            ident = f'AQUA-{n:03}'
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
        assert len({m['geometry_signature'] for m in report}) == 28
        for i, obj in enumerate(objects):
            obj.location = ((i % 7) * 40, (i // 7) * 40, 0)
        bpy.data.libraries.write(str(destination / 'sources/aquabio.blend'), {scene}, compress=True)
        (destination / 'export-report.json').write_text(json.dumps({'blender': bpy.app.version_string, 'models': report}, indent=2) + '\n')
        (destination / 'export.log').write_text(logs.getvalue())
        print('AQUABIO EXPORT PASS: 28 distinct designs;', sum(r['triangles'] for r in report), 'triangles')
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
