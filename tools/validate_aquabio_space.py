"""Auditoría espacial CPU de 6B: AABB importadas, caminos, paneles y barridos de animación.

Broad phase conservadora: AABB solapadas no demuestran intersección de triángulos.
Las uniones intencionales se enumeran; no es un motor de colisiones.
"""
import argparse
import copy
import csv
import itertools
import json
import math
import html
from pathlib import Path


def require(ok, message):
    if not ok:
        raise ValueError(message)


def bounds(row):
    return [[float(row['min_'+c]) for c in 'xyz'], [float(row['max_'+c]) for c in 'xyz']]


def transform(box, row):
    points=[]
    for p in itertools.product(*zip(*box)):
        points.append([sum(float(row[f'm{c}{r}'])*p[c] for c in range(3))+float(row[f'm3{r}']) for r in range(3)])
    return [[min(p[a] for p in points) for a in range(3)], [max(p[a] for p in points) for a in range(3)]]


def overlaps(a, b, axes=(0,1,2), margin=.005):
    return all(min(a[1][i],b[1][i])-max(a[0][i],b[0][i])>margin for i in axes)


def gap(a, b):
    return math.hypot(*(max(0,a[0][i]-b[1][i],b[0][i]-a[1][i]) for i in (0,2)))


def segment_box(origin, target, box):
    enter,leave=0.,1.
    for a in range(3):
        delta=target[a]-origin[a]
        if abs(delta)<1e-8:
            if origin[a]<box[0][a] or origin[a]>box[1][a]:return False
        else:
            t0,t1=sorted(((box[0][a]-origin[a])/delta,(box[1][a]-origin[a])/delta))
            enter=max(enter,t0);leave=min(leave,t1)
            if leave<enter:return False
    return enter<1-1e-4 and leave>1e-4


def audit(root, rows, views, tour):
    source=json.loads((root/'docs/assets/AQUABIO_EXPORT_REPORT.json').read_text())
    models={m['model_id']:m for m in source['models']}
    base={r['instance_id']:r for r in rows if float(r['time_s'])==0}
    require(len(base)==52 and len(rows)==3652,'52 bases y 200 pasos de dieciocho animaciones requeridos')
    for r in rows:
        require(all(math.isfinite(float(v)) for k,v in r.items() if k!='instance_id'),'Estado no finito')
    boxes={iid:bounds(row) for iid,row in base.items()}
    platform=boxes['AQUA-001-main']
    require(abs(platform[1][0]-platform[0][0]-48)<.001 and abs(platform[1][2]-platform[0][2]-48)<.001,'La ampliacion debe existir en la geometria importada')
    for iid,box in boxes.items():
        for a in (0,2):require(box[0][a]>=platform[0][a]-.001 and box[1][a]<=platform[1][a]+.001,'Instancia fuera del terreno: '+iid)
        expected=transform(models[iid[:8]]['bounds'],base[iid])
        require(all(expected[0][a]<=box[0][a]+.001 and expected[1][a]>=box[1][a]-.001 for a in range(3)),'Bounds fuente/importador inconsistentes: '+iid)
    stations=['AQUA-004-collector','AQUA-006-cistern','AQUA-008-housing','AQUA-010-tank','AQUA-012-sand',
              'AQUA-013-carbon','AQUA-015-uv','AQUA-016-treated','AQUA-020-sampling','AQUA-022-precipitation','AQUA-023-basin','AQUA-027-shelter']
    distances=[]
    for a,b in itertools.combinations(stations,2):
        distance=gap(boxes[a],boxes[b]);require(distance>=2.5-.001,'Estaciones sin 2.5 m de separacion XZ: '+a+' / '+b)
        distances.append(dict(a=a,b=b,gap_m=distance))
    paths={iid:transform([[-1.4,0,-4],[1.4,.07,4]],row) for iid,row in base.items() if iid.startswith('AQUA-003-')}
    require(len(paths)==10,'Conservar diez instancias de caminos')
    visited={next(iter(paths))}
    while True:
        expanded=visited|{iid for iid,box in paths.items() if any(gap(box,paths[v])<=.001 for v in visited)}
        if expanded==visited:break
        visited=expanded
    require(len(visited)==10,'Caminos desconectados')
    for iid,row in base.items():
        if iid not in paths:continue
        width=2.8*math.hypot(float(row['m00']),float(row['m02']))
        expected=3.8 if iid in ('AQUA-003-entry','AQUA-003-path-0','AQUA-003-path-1','AQUA-003-path-2') else 2.8
        require(abs(width-expected)<.001,'Ancho util incorrecto: '+iid)
    # El portal salva el paso; la rueda/soporte estan al final del recorrido, no en un tramo pasante.
    path_exceptions={'AQUA-001-main','AQUA-002-entry','AQUA-021-cycle','AQUA-022-precipitation'}
    for path,box in paths.items():
        for iid,obstacle in boxes.items():
            if iid in paths or iid in path_exceptions:continue
            # Refinar la caja global: un cartel lateral no rellena el hueco hasta su maquina.
            model=models[iid[:8]]
            parts=[obstacle]
            if model['signs']:
                parts=[transform(model['mechanism_bounds'],base[iid])]
                for sign in model['signs']:
                    support=copy.deepcopy(sign['bounds']);support[0][1]=0
                    parts.append(transform(support,base[iid]))
            require(not any(overlaps(box,p) for p in parts),'AABB de objeto invade ancho util del camino: '+iid+' / '+path)
    panels=[]
    for iid,row in base.items():
        model=models[iid[:8]]
        if not model['signs'] or iid.startswith(('AQUA-002','AQUA-027')):continue
        sign=model['signs'][0];plate=transform(sign['bounds'],row)
        center=[(plate[0][i]+plate[1][i])*.5 for i in range(3)]
        observer=[center[0],1.7,center[2]+4.5]
        require(sign['letter_heights'][0]>=.239 and min(sign['letter_heights'][1:])>=.16,'Tipografia demasiado pequena: '+iid)
        sample_targets=[[center[0]+dx*(plate[1][0]-plate[0][0])*.35,y,plate[1][2]] for dx in (-1,0,1) for y in (1.1,1.5,1.9)]
        for other,obstacle in boxes.items():
            if other=='AQUA-001-main' or other in paths:continue
            if other==iid:obstacle=transform(model['mechanism_bounds'],row)
            for target in sample_targets:
                require(not segment_box(observer,target,obstacle),'Linea de lectura tapada (AABB conservadora): '+iid+' / '+other)
        panels.append(dict(instance_id=iid,observer=observer,panel_bounds=plate,min_letter_height=min(sign['letter_heights'])))
    require(len(panels)==11,'Once paneles revisados')
    # Solo contactos de montaje/agua; no se ignoran vecinos de otra estacion.
    joins={frozenset(pair) for pair in (
        ('AQUA-005-adjustable','AQUA-004-collector'),('AQUA-009-pump','AQUA-008-housing'),
        ('AQUA-011-scraper','AQUA-010-tank'),('AQUA-019-indicator','AQUA-017-distribution'),
        ('AQUA-021-cycle','AQUA-022-precipitation'),('AQUA-024-floating','AQUA-023-basin'),('AQUA-025-sway','AQUA-023-basin'))}
    joins.update(frozenset((iid,"AQUA-023-basin")) for iid in base
                 if iid.startswith(("AQUA-024-","AQUA-025-")))
    moving={r['instance_id'] for r in rows if float(r['time_s'])>0}
    times={}
    for r in rows:
        if float(r['time_s'])>0:times.setdefault(r['time_s'],{})[r['instance_id']]=bounds(r)
    for time,step in times.items():
        require(len(step)==18,'Barrido animado incompleto')
        current={**boxes,**step}
        for iid,box in step.items():
            for other,obstacle in current.items():
                if other==iid or other=='AQUA-001-main' or other in paths or frozenset((iid,other)) in joins:continue
                require(not overlaps(box,obstacle),'Barrido AABB animado no previsto: '+iid+' / '+other+' t='+time)
    # Las piezas asociadas conservan el pivote respecto a sus estructuras.
    anchors=[('AQUA-009-pump','AQUA-008-housing',[0,.9,.16]),('AQUA-005-adjustable','AQUA-004-collector',[0,2.99,-2.02]),
             ('AQUA-011-scraper','AQUA-010-tank',[0,1.09,0]),('AQUA-019-indicator','AQUA-017-distribution',[0,1.09,.15]),('AQUA-021-cycle','AQUA-022-precipitation',[-6,1.8,0])]
    for moving_id,stand,offset in anchors:
        require(all(abs(float(base[moving_id][f'm3{a}'])-float(base[stand][f'm3{a}'])-offset[a])<.001 for a in range(3)),'Pivote desalineado: '+moving_id)
    for iid in ('AQUA-009-demo','AQUA-014-demo','AQUA-028-demo'):
        require(abs(boxes[iid][0][1]-.01)<.002,'Muestra flotante/sumergida: '+iid)
    view_by_name={v['capture']:v for v in views}
    entry=view_by_name['entry'];eye=[float(entry['camera_'+a]) for a in 'xyz']
    require(abs(eye[1]-1.7)<.001 and eye==[0.,1.7,13.],'Camara inicial incorrecta')
    require(any(all(box[0][a]<=eye[a]<=box[1][a] for a in (0,2)) for box in paths.values()),'Entrada fuera del sendero')
    for iid,box in boxes.items():
        if iid not in paths and iid!='AQUA-001-main':require(not all(box[0][a]<=eye[a]<=box[1][a] for a in range(3)),'Camara dentro de objeto: '+iid)
    for panel in panels:
        view=view_by_name['panels/'+panel['instance_id'][:8]]
        require(abs(float(view['camera_y'])-1.7)<.001,'Lectura debe comprobarse a 1.7 m')
        require(all(abs(float(view['camera_'+a])-panel['observer'][i])<.06 for i,a in enumerate('xyz')),'Captura no corresponde al observador auditado')
    require(len(tour)==120,'Recorrido debe registrar 120 camaras reales')
    for point in tour:
        p=[float(point['camera_'+a]) for a in 'xyz']
        require(abs(p[1]-1.7)<.001,'Altura alterada en recorrido')
        for iid,row in base.items():
            if iid.startswith(('AQUA-001','AQUA-003')):continue
            model=models[iid[:8]];parts=[boxes[iid]]
            if model['signs']:
                parts=[transform(model['mechanism_bounds'],row)]
                for sign in model['signs']:
                    support=copy.deepcopy(sign['bounds']);support[0][1]=0
                    parts.append(transform(support,row))
            require(not any(all(box[0][a]<=p[a]<=box[1][a] for a in range(3)) for box in parts),'Camara de recorrido dentro de AABB de objeto: '+iid)
    return dict(tour_samples=len(tour),platform_m=[48,48],instances=52,paths=10,connected_paths=len(visited),useful_width_m=dict(main=3.8,secondary=2.8),
        minimum_station_gap_m=min(d['gap_m'] for d in distances),station_distances=distances,readable_panels=panels,
        animation_samples=len(times),animated_entities=len(moving),mount_exceptions=[sorted(p) for p in sorted(joins,key=lambda p:sorted(p))],
        limits='AABB conservadoras y lineas de lectura muestreadas; no colisiones exactas, legibilidad visual se inspecciona en framebuffer')



def write_plan(path, rows, source):
    base={r['instance_id']:r for r in rows if float(r['time_s'])==0}
    models={m['model_id']:m for m in source['models']}
    scale=11
    def xy(x,z):return 380+x*scale,385+z*scale
    def rect(box,color,stroke='#367780'):
        x,y=xy(box[0][0],box[0][2]);w=(box[1][0]-box[0][0])*scale;h=(box[1][2]-box[0][2])*scale
        return f'<rect x="{x:.2f}" y="{y:.2f}" width="{max(w,1):.2f}" height="{max(h,1):.2f}" fill="{color}" stroke="{stroke}" stroke-width="0.8"/>'
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 980 790"><rect width="980" height="790" fill="#eef7f4"/>',
         '<g font-family="sans-serif" fill="#123c49"><text x="35" y="34" font-size="23">AquaBio — recorrido por subzonas: 48 × 48 m</text><text x="35" y="59" font-size="14">52 instancias / 28 modelos; -Z norte; cajas importadas y anchuras útiles de caminos</text></g>']
    svg.append(rect(bounds(base['AQUA-001-main']),'#d4e1ce'))
    for iid,row in base.items():
        if iid.startswith('AQUA-003-'):svg.append(rect(transform([[-1.4,0,-4],[1.4,.07,4]],row),'#fffce6'))
    for iid,row in base.items():
        if iid.startswith(('AQUA-001','AQUA-003')):continue
        model=models[iid[:8]];box=bounds(row)
        if model['mechanism_bounds']:box=transform(model['mechanism_bounds'],row)
        color='#57adb0' if iid.startswith('AQUA-023') else '#8cb777' if iid.startswith(('AQUA-024','AQUA-025','AQUA-026')) else '#8cbcc9'
        svg.append(rect(box,color))
        for sign in model['signs']:svg.append(rect(transform(sign['bounds'],row),'#143c47'))
        x,y=xy(float(row['m30']),float(row['m32']))
        svg.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.8" fill="#ad5d24"><title>{html.escape(iid)}</title></circle>')
        if not any(t in iid for t in ('static-','bank-','path-','branch','demo')):
            svg.append(f'<text x="{x+4:.2f}" y="{y-5:.2f}" font-family="sans-serif" font-size="11" fill="#123c49">{iid[5:8]}</text>')
    for text,y in [('A — Captación / precipitación',155),('B — Almacenamiento / conducción',205),('C — Tratamiento / filtración',255),('D — Medición junto al eje',305),('E — Humedal oriental',345),('Eje útil 3.8 m / ramales 2.8 m',375),('Cámara (0, 1.7, 13)',425),('Altura visitante: 1.7 m',455),('Paso: 3 m/s; Enter / H intactos',485),('● Colocación / número AQUA-XXX',545),('Paneles laterales oscuros',575)]:
        svg.append(f'<text x="685" y="{y}" font-family="sans-serif" font-size="15" fill="#123c49">{html.escape(text)}</text>')
    svg.append('<text x="35" y="720" font-family="sans-serif" font-size="14">Se conserva la plataforma de 2 304 m²; no se escalan maquinaria ni vegetación.</text><text x="35" y="748" font-family="sans-serif" font-size="14">AABB conservadoras ≠ colisión exacta. Se exceptúan montajes, suelo y vegetación del humedal; no hay física.</text></svg>')
    path.write_text('\n'.join(svg)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--bounds',type=Path,required=True)
    parser.add_argument('--views',type=Path,required=True);parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--self-test',action='store_true');parser.add_argument('--plan',type=Path);args=parser.parse_args()
    with args.bounds.open() as f:rows=list(csv.DictReader(f))
    with args.views.open() as f:views=list(csv.DictReader(f))
    with (args.views.parent/'tour-cameras.csv').open() as f:tour=list(csv.DictReader(f))
    result=audit(args.root.resolve(),rows,views,tour)
    if args.self_test:
        assert gap([[0,0,0],[1,1,1]],[[4,0,1],[5,1,2]])==3
        assert overlaps([[0,0,0],[1,1,1]],[[.5,.5,.5],[2,2,2]])
        assert not overlaps([[0,0,0],[1,1,1]],[[1,0,0],[2,1,1]])
        assert segment_box([0,0,0],[0,0,3],[[-1,-1,1],[1,1,2]])
        assert not segment_box([2,0,0],[2,0,3],[[-1,-1,1],[1,1,2]])
        def disconnect_path(items):
            row=next(x for x in items if x['instance_id']=='AQUA-003-path-8')
            for key in ('min_z','max_z','m32'):row[key]=str(float(row[key])+7)
        for label,mutate in [('camino desconectado',disconnect_path),('fuera del terreno',lambda r:r[0].update(min_x='-100')),
                             ('ancho alterado',lambda r:next(x for x in r if x['instance_id']=='AQUA-003-entry').update(m00='1'))]:
            bad=copy.deepcopy(rows);mutate(bad)
            try:audit(args.root.resolve(),bad,views,tour)
            except ValueError:print('Expected rejection:',label)
            else:raise AssertionError('No se rechazo: '+label)
    args.report.write_text(json.dumps(result,indent=2)+'\n')
    if args.plan:write_plan(args.plan,rows,json.loads((args.root/'docs/assets/AQUABIO_EXPORT_REPORT.json').read_text()))
    print('AQUABIO SPACE PASS: 48x48 m; 52 instances; ten connected paths; 11 readable panel views; 200 animation steps; conservative AABB checks')


if __name__=='__main__':main()
