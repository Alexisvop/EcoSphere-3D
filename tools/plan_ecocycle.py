"""Plano previo a modelado: reservas métricas, componentes y señales. No genera mallas."""
import json
from pathlib import Path

# Las colocaciones se trasladan a Settings; los tamaños son reservas de diseño, no mediciones GLB.
POSITIONS=[(0,0,-1),(0,.01,18),(0,.04,12),(-7.5,.01,14),(-16,.01,9),(-16,.01,1),
 (-15,.01,-8),(-15,1.31,-6),(-16,2.61,1),(-16,1.81,-.5),(-15,1.66,-10),(-16,.01,-18),
 (-16,2.71,-18),(-7.5,.01,-18),(-7,.01,-8),(-7,1.51,-8),(14,.01,-18),(14,.01,-8),
 (7.5,.01,-18),(7.5,.01,-1),(16,.01,1),(16,.01,20),(16,.01,12),(7.5,.01,12)]
SIZES=[(48,.4,48),(6.4,4,.9),(3.8,.08,10),(3.8,2.7,1.8),(3.6,2,2),(3.4,2.5,3.8),
 (3.6,3,5.4),(2.6,.5,.5),(1.1,.65,.65),(1.2,.7,.2),(2.8,1.4,1.4),(3.6,3.7,2.8),
 (2,.5,1.4),(1.4,1.1,1.1),(3.2,2.8,2.8),(1.7,.9,.9),(3.4,2.5,2.8),(5.4,2.5,2.8),
 (1.5,1.8,1.5),(3,2.8,2.4),(2.8,1.4,2),(2.8,2,2.8),(3.6,2,2.2),(1.6,2.1,1.8)]
# Señales en los costados; las piezas animadas no llevan textos que giren con ellas.
PANELS={4:((0,1.6,.45),'CICLO CIRCULAR',['REDUCIR Y REPARAR','RECICLAR NO ES TODO']),
 5:((-3.6,1.5,0),'SEPARAR',['PAPEL VIDRIO METAL','REVISAR REGLAS LOCALES']),
 6:((-3.6,1.5,0),'ALIMENTAR',['DOSIFICAR RESIDUOS','VIBRACION ILUSTRADA']),
 7:((-3.6,1.5,0),'SEPARACION',['IMAN PARA FERROSOS','NO TODOS LOS METALES']),
 12:((-3.8,1.5,0),'PRENSAR PAPEL',['COMPACTAR PARA ACOPIO','NO LIMPIA LAS FIBRAS']),
 15:((3.2,1.5,0),'TRITURAR',['REDUCIR EL TAMANO','NO MEZCLAR POLIMEROS']),
 17:((3.6,1.5,0),'LAVAR PLASTICO',['RETIRAR SUCIEDAD','TRATAR EL AGUA USADA']),
 18:((4.2,1.5,0),'EXTRUSION',['CALOR Y CONFORMADO','CONTROLAR EL MATERIAL']),
 20:((-3.6,1.5,0),'RECUPERAR VIDRIO',['SEPARAR POR TIPO','EVITAR CONTAMINANTES']),
 23:((3.7,1.5,0),'REPARAR',['ALARGAR VIDA UTIL','COMPOSTAR ORGANICOS'])}
PATHS=[(0,.04,2),(0,.04,-8),(0,.04,-18),(-9,.04,5),(9,.04,5),(-9,.04,-12),(9,.04,-12)]
EXTRAS={4:[(7.5,.01,20)],14:[(-9.5,.01,-20.5),(-6,.01,-20.5)],19:[(6,.01,-20.5),(9,.01,-20.5)],24:[(7,.01,14.5),(10,.01,14.5)]}
INTERACTIVE=[5,14,17,19,21,23,24]
ANIMATED=[8,11,16,22,10,13,6,9]
NAMES=['Plataforma industrial EcoCycle','Portal de flechas circulares','Sendero de losas recuperadas','Quiosco de economia circular',
'Estacion de separacion de residuos','Alimentador vibratorio','Bastidor de cinta transportadora','Rodillo helicoidal','Sensor optico demostrativo',
'Compuerta de desvio','Tambor magnetico','Bastidor de prensa de papel','Embolo de compactacion','Fardo de papel recuperado',
'Carcasa de trituradora','Rotor de cuchillas','Tanque de lavado','Extrusora seccionada','Bobina de filamento','Trituradora de vidrio',
'Cajon de vidrio recuperado','Compostador rotativo','Banco de reparacion','Silla de listones recuperados']

def write_plan(path, positions=POSITIONS):
    def rect(x,z,w,d,color):return f'<rect x="{350+(x-w/2)*11:.2f}" y="{350+(z-d/2)*11:.2f}" width="{w*11:.2f}" height="{d*11:.2f}" fill="{color}" stroke="#406658"/>'
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1020 720"><rect width="1020" height="720" fill="#eef5ed"/><g font-family="sans-serif" fill="#193a30">',
    '<text x="25" y="30" font-size="23">EcoCycle — plano previo: 48 × 48 m reales desde el inicio</text>',rect(0,-1,48,48,'#ccdac6')]
    for i,(x,y,z) in enumerate([POSITIONS[2]]+PATHS):
        svg.append(rect(x,z,3.8 if i<4 else 18,10 if i<4 else 2.8,'#fff0cb'))
    for n,((x,y,z),(w,h,d)) in enumerate(zip(positions,SIZES),1):
        if n in (1,3):continue
        svg.append(rect(x,z,w,d,'#e8b660' if z<0 and x<0 else '#92bea0'))
        if n in PANELS:
            (px,py,pz),title,lines=PANELS[n];svg.append(rect(x+px,z+pz,3.8,.15,'#143d49'))
            svg.append(rect(x+px,z+pz+3.2,3.8,2.5,'#d4e9e5'))
        svg.append(f'<text x="{350+x*11:.1f}" y="{350+z*11:.1f}" font-size="11">{n:03}{" ★" if n in INTERACTIVE else ""}{" ↻" if n in ANIMATED else ""}</text>')
    for label,y in [('A SW — clasificación',100),('B NO — procesamiento',140),('C NE — recuperación',180),('D SE — reutilización',220),('Entrada: +Z, (0,1.7,13)',280),('Eje: 3.8 m / ramales: 2.8 m',320),('★ siete manipulables',380),('↻ ocho mecanismos animados',420),('Azul oscuro: panel lateral',460),('Azul claro: observación frontal',500),('Reservas ≠ medidas importadas',560),('Sin física / procesos ilustrativos',600)]:svg.append(f'<text x="660" y="{y}" font-size="17">{label}</text>')
    svg.append('<circle cx="350" cy="493" r="6" fill="#245ab0"/><text x="25" y="690" font-size="14">-Z norte / +Y vertical. Objetivo: ≥2.5 m entre AABB XZ de estaciones; confirmar tras importar GLB.</text></g></svg>')
    path.write_text('\n'.join(svg)+'\n')

if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]
    assert len(POSITIONS)==len(SIZES)==len(NAMES)==24 and len(set(INTERACTIVE))==7 and len(set(ANIMATED))==8
    write_plan(root/'docs/assets/ECOCYCLE_PLAN.svg')
    (root/'docs/assets/ECOCYCLE_PLANNING.json').write_text(json.dumps(dict(stage='pre-modeling reservations; verify imported bounds',platform=[48,48],positions=POSITIONS,sizes=SIZES,panels=PANELS,paths=PATHS,extras=EXTRAS,interactive=INTERACTIVE,animated=ANIMATED,names=NAMES),indent=2)+'\n')
