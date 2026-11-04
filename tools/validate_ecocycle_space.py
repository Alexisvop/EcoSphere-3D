"""EcoCycle: auditoría métrica CPU sobre vértices importados; AABB no equivale a colisión exacta."""
import argparse
import copy
import csv
import itertools
import json
import math
from pathlib import Path
from validate_aquabio_space import bounds, transform, gap, overlaps, segment_box, require


def audit(root,rows,views,tour):
    models={m['model_id']:m for m in json.loads((root/'docs/assets/ECOCYCLE_EXPORT_REPORT.json').read_text())['models']}
    base={r['instance_id']:r for r in rows if float(r['time_s'])==0}
    require(len(base)==38 and len(rows)==1638,'38 bases y 200 pasos de ocho mecanismos')
    require(all(math.isfinite(float(v)) for r in rows for k,v in r.items() if k!='instance_id'),'Estado no finito')
    boxes={iid:bounds(r) for iid,r in base.items()};platform=boxes['CYCLE-001-main']
    require(all(abs(platform[1][a]-platform[0][a]-48)<.001 for a in (0,2)),'Suelo importado no es 48x48')
    for iid,box in boxes.items():
        require(all(box[0][a]>=platform[0][a]-.001 and box[1][a]<=platform[1][a]+.001 for a in (0,2)),'Instancia fuera del terreno: '+iid)
        source=transform(models[iid[:9]]['bounds'],base[iid])
        require(all(source[0][a]<=box[0][a]+.001 and source[1][a]>=box[1][a]-.001 for a in range(3)),'Bounds fuente/importador: '+iid)
    stations=[f'CYCLE-{n:03}-main' for n in (4,5,6,7,12,14,15,17,18,19,20,21,22,23,24)]
    distances=[dict(a=a,b=b,gap_m=gap(boxes[a],boxes[b])) for a,b in itertools.combinations(stations,2)]
    require(all(d['gap_m']>=2.5-.001 for d in distances),'Estaciones sin 2.5 m entre AABB XZ')
    paths={iid:transform([[-1.9,0,-5],[1.9,.08,5]],r) for iid,r in base.items() if iid.startswith('CYCLE-003-')}
    require(len(paths)==8,'Ocho caminos requeridos');visited={'CYCLE-003-main'}
    while True:
        new=visited|{iid for iid,box in paths.items() if any(gap(box,paths[v])<.001 for v in visited)}
        if new==visited:break
        visited=new
    require(len(visited)==8,'Camino desconectado')
    def components(iid,row=None):
        r=row or base[iid];model=models[iid[:9]]
        if not model['signs']:return [transform(model['bounds'],r)]
        parts=[transform(model['mechanism_bounds'],r)]
        for sign in model['signs']:
            support=copy.deepcopy(sign['bounds']);support[0][1]=0;parts.append(transform(support,r))
        return parts
    for iid,box in paths.items():
        r=base[iid];width=3.8*math.hypot(float(r['m00']),float(r['m02']))
        require(abs(width-(3.8 if abs(float(r['m00']))>.1 else 2.8))<.001,'Ancho incorrecto: '+iid)
        for other in boxes:
            if other in paths or other.startswith(('CYCLE-001','CYCLE-002')):continue
            require(not any(overlaps(box,p) for p in components(other)),'Objeto invade ancho util: '+other+' / '+iid)
    views={v['capture']:v for v in views};panels=[]
    for iid,r in base.items():
        model=models[iid[:9]]
        if not model['signs']:continue
        sign=model['signs'][0];plate=transform(sign['bounds'],r);center=[sum(p)/2 for p in zip(*plate)]
        observer=[center[0],1.7,center[2]+4.5]
        require(min(sign['letter_heights'])>=.16 and sign['letter_heights'][0]>=.239,'Letra demasiado pequena: '+iid)
        require(all(platform[0][a]<=observer[a]<=platform[1][a] for a in (0,2)),'Observador fuera del terreno: '+iid)
        for other in boxes:
            if other in paths or other.startswith('CYCLE-001'):continue
            # El quiosco es un expositor: su cubierta no tapa su propia placa a altura del ojo.
            if other==iid and iid.startswith('CYCLE-004'):continue
            parts=[transform(model['mechanism_bounds'],r)] if other==iid else components(other)
            for dx,y in itertools.product((-.35,0,.35),(1.1,1.5,1.9)):
                target=[center[0]+dx*(plate[1][0]-plate[0][0]),y,plate[1][2]]
                require(not any(segment_box(observer,target,part) for part in parts),'Cartel obstruido: '+iid+' / '+other)
        if iid.endswith('-main'):
            view=views['panels/'+iid[:9]]
            require(all(abs(float(view['camera_'+a])-observer[i])<.06 for i,a in enumerate('xyz')),'Camara de lectura no corresponde: '+iid)
        panels.append(dict(id=iid,observer=observer,panel_bounds=plate,letter_heights=sign['letter_heights']))
    require(len(panels)==11,'Once placas contando dos quioscos')
    assemblies=[{f'CYCLE-{n:03}-main' for n in group} for group in ((6,9,10),(7,8,11),(12,13),(15,16))]
    joins={frozenset(p) for group in assemblies for p in itertools.combinations(group,2)}
    times={}
    for r in rows:
        if float(r['time_s'])>0:times.setdefault(r['time_s'],{})[r['instance_id']]=r
    require(len(times)==200,'Barrido temporal incompleto')
    for t,step in times.items():
        require(len(step)==8,'Ocho animados por paso')
        for iid,row in step.items():
            box=bounds(row)
            for other in boxes:
                if iid==other or other in paths or other.startswith('CYCLE-001') or frozenset((iid,other)) in joins:continue
                require(not any(overlaps(box,p) for p in components(other,step.get(other))),'Barrido inesperado: '+iid+' / '+other+' t='+t)
    anchors=[(8,7,[0,1.3,2]),(11,7,[0,1.65,-2]),(9,6,[0,2.6,0]),(10,6,[0,1.8,-1.5]),(13,12,[0,2.7,0]),(16,15,[0,1.5,0])]
    for n,stand,offset in anchors:
        a=base[f'CYCLE-{n:03}-main'];b=base[f'CYCLE-{stand:03}-main']
        require(all(abs(float(a[f'm3{i}'])-float(b[f'm3{i}'])-offset[i])<.001 for i in range(3)),'Pivote desalineado')
    entry=views['entry'];eye=[float(entry['camera_'+a]) for a in 'xyz']
    require(eye==[0,1.7,13],'Camara inicial EcoCycle')
    require(any(all(b[0][a]<=eye[a]<=b[1][a] for a in (0,2)) for b in paths.values()),'Entrada fuera de camino')
    require(len(tour)==120,'120 camaras reales de recorrido')
    for row in tour+[dict(camera_x=eye[0],camera_y=eye[1],camera_z=eye[2])]:
        p=[float(row['camera_'+a]) for a in 'xyz'];require(all(math.isfinite(v) for v in p) and abs(p[1]-1.7)<.001,'Altura de visitante')
        require(all(platform[0][a]<=p[a]<=platform[1][a] for a in (0,2)),'Camara fuera del terreno')
        for iid in boxes:
            if iid in paths or iid.startswith(('CYCLE-001','CYCLE-002')):continue
            require(not any(all(b[0][a]<=p[a]<=b[1][a] for a in range(3)) for b in components(iid)),'Camara dentro de objeto: '+iid)
    return dict(platform_m=[48,48],instances=38,paths=8,connected_paths=len(visited),minimum_station_gap_m=min(d['gap_m'] for d in distances),station_distances=distances,
        panels=panels,animation_samples=200,animated_entities=8,tour_samples=len(tour),mount_exceptions=[sorted(p) for p in sorted(joins,key=lambda p:sorted(p))],
        limits='Broad phase AABB conservadora; 0.1 s muestreados, no colision exacta ni prueba continua. Cubierta del quiosco exceptuada para su propia placa.')


def write_plan(path,rows,models):
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1040 750"><rect width="1040" height="750" fill="#eef5ed"/><g font-family="sans-serif" fill="#183d30"><text x="25" y="30" font-size="23">EcoCycle — plano definitivo: 48 × 48 m / 24 modelos / 38 instancias</text>']
    for r in rows:
        if float(r['time_s']):continue
        iid=r['instance_id'];b=bounds(r);color='#d1ddc9' if iid.startswith('CYCLE-001') else '#fff0cb' if iid.startswith('CYCLE-003') else '#8db99c'
        if models[iid[:9]]['mechanism_bounds']:b=transform(models[iid[:9]]['mechanism_bounds'],r)
        def rect(box,fill):
            return f'<rect x="{350+box[0][0]*11:.2f}" y="{350+box[0][2]*11:.2f}" width="{max(1,(box[1][0]-box[0][0])*11):.2f}" height="{max(1,(box[1][2]-box[0][2])*11):.2f}" fill="{fill}" stroke="#3b6654"/>'
        svg.append(rect(b,color))
        for sign in models[iid[:9]]['signs']:svg.append(rect(transform(sign['bounds'],r),'#173d49'))
        if not iid.startswith(('CYCLE-001','CYCLE-003')):svg.append(f'<text x="{350+float(r["m30"])*11:.1f}" y="{350+float(r["m32"])*11:.1f}" font-size="11">{iid[6:9]}</text>')
    for text,y in [('A SW — clasificación',110),('B NO — procesamiento',160),('C NE — recuperación',210),('D SE — reutilización',260),('Eje útil 3.8 m / ramales 2.8 m',330),('Entrada: (0,1.7,13), +Z',380),('Panel lateral / observador 1.7 m',430),('Siete interactivos: 005 014 017',480),('019 021 023 024',510),('Ocho animados: 008 011 016 022',555),('010 013 006 009',585)]:svg.append(f'<text x="650" y="{y}" font-size="17">{text}</text>')
    svg.append('<circle cx="350" cy="493" r="5" fill="#225ba8"/><text x="25" y="700" font-size="14">Límites de vértices importados; +Y vertical / -Z norte. AABB ≠ colisión. No se simula procesamiento industrial.</text></g></svg>')
    path.write_text('\n'.join(svg)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--bounds',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--self-test',action='store_true');p.add_argument('--plan',type=Path);a=p.parse_args()
    def read(path):
        with path.open() as f:return list(csv.DictReader(f))
    rows=read(a.bounds);folder=a.root/'docs/evidence/phase6c/ecocycle';views=read(folder/'views.csv');tour=read(folder/'tour-cameras.csv')
    result=audit(a.root.resolve(),rows,views,tour)
    if a.self_test:
        def move(items,iid,axis,delta):
            for r in items:
                if r['instance_id']==iid:
                    for k in ('min_'+axis,'max_'+axis,'m3'+str('xyz'.index(axis))):r[k]=str(float(r[k])+delta)
        def wrong_width(items):
            r=next(x for x in items if x['instance_id']=='CYCLE-003-main');r['m00']='0.8'
            for k in ('min_x','max_x'):r[k]=str(float(r[k])*.8)
        mutations=[('camino desconectado',lambda r:move(r,'CYCLE-003-path-6','x',3)),('fuera del terreno',lambda r:move(r,'CYCLE-014-main','x',-40)),
            ('ancho incorrecto',wrong_width),
            ('cartel obstruido',lambda r:(move(r,'CYCLE-024-stock-0','x',-26.6),move(r,'CYCLE-024-stock-0','z',-3)))]
        for name,change in mutations:
            bad=copy.deepcopy(rows);change(bad)
            try:audit(a.root.resolve(),bad,views,tour)
            except ValueError as error:
                if name=='cartel obstruido':require('Cartel obstruido' in str(error),'La prueba negativa debe ejercitar visibilidad')
                if name=='ancho incorrecto':require('Ancho incorrecto' in str(error),'La prueba negativa debe ejercitar anchuras')
                print('Expected rejection:',name,str(error))
            else:raise AssertionError('Mutation accepted: '+name)
    a.report.write_text(json.dumps(result,indent=2)+'\n')
    if a.plan:write_plan(a.plan,rows,{m['model_id']:m for m in json.loads((a.root/'docs/assets/ECOCYCLE_EXPORT_REPORT.json').read_text())['models']})
    print('ECOCYCLE SPACE PASS: 48x48 m; 38 instances; eight connected paths; 11 panels; 200 animation steps; 120 tour cameras')
if __name__=='__main__':main()
