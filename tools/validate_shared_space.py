"""6D imported AABB, original paths, reading rays and sampled moving mechanisms."""
import argparse
import copy
import csv
import itertools
import json
import html
import xml.etree.ElementTree as ET
from pathlib import Path
from validate_aquabio_space import bounds,transform,gap,overlaps,segment_box,require


def audit(root,rows):
    base={};moving=[]
    for r in rows:
        if float(r['time_s'])==0:base[(r['owner'],r['instance_id'])]=r
        else:moving.append(r)
    require(len(base)==252 and len(moving)==4800,'252 base and 200 samples of 24 animations required')
    shared={k:r for k,r in base.items() if k[1].startswith('SHARED-')};require(len(shared)==12,'Twelve shared bounds')
    boxes={k:bounds(r) for k,r in base.items()}
    floors={'hub':('PLAZA-001','PLAZA-002'),'ecoenergy':('ENERGY-001',),'aquabio':('AQUA-001',),'ecocycle':('CYCLE-001',)}
    path_prefix={'hub':'PLAZA-003','ecoenergy':'ENERGY-003','aquabio':'AQUA-003','ecocycle':'CYCLE-003'}
    reports={}
    for owner,area in [('aquabio','AQUABIO'),('ecocycle','ECOCYCLE')]:
        reports[owner]={m['model_id']:m for m in json.loads((root/f'docs/assets/{area}_EXPORT_REPORT.json').read_text())['models']}
    placements=[];gaps=[]
    for key,row in shared.items():
        owner,iid=key;box=boxes[key];platform=next(b for k,b in boxes.items() if k[0]==owner and k[1].startswith(floors[owner][0]))
        require(all(platform[0][a]-.001<=box[0][a]<=box[1][a]<=platform[1][a]+.001 for a in (0,2)),'Shared outside original ground: '+iid)
        if owner=='hub':require(max((x*x+z*z)**.5 for x,z in itertools.product((box[0][0],box[1][0]),(box[0][2],box[1][2])))<=10.5,'Shared outside main plaza pavement')
        obstacles=[]
        for other,b in boxes.items():
            if other[0]!=owner or other==key or other[1].startswith(floors[owner]):continue
            if other[1].startswith(path_prefix[owner]):
                require(not overlaps(box,b,axes=(0,2)),'Shared/path overlap: '+iid+' / '+other[1]);continue
            require(not overlaps(box,b),'Shared/object overlap: '+iid+' / '+other[1])
            distance=gap(box,b);gaps.append(distance)
            require(distance>=.25-.001,'Shared clearance below 0.25 m: '+iid+' / '+other[1])
            obstacles.append((other,b))
        centre=[sum(p)/2 for p in zip(*box)];eye=[centre[0],1.7,centre[2]+3]
        require(all(platform[0][a]<=eye[a]<=platform[1][a] for a in (0,2)),'Shared viewer outside ground: '+iid)
        for other,b in obstacles:
            require(not (b[0][0]<=eye[0]<=b[1][0] and b[0][2]<=eye[2]<=b[1][2]),'Shared viewer footprint blocked: '+iid+' / '+other[1])
            require(not segment_box(eye,centre,b),'Shared reading ray blocked: '+iid+' / '+other[1])
        placements.append(dict(owner=owner,instance_id=iid,bounds=box,observer=eye,target=centre))
    for r in moving:
        for key in shared:
            if key[0]==r['owner']:require(not overlaps(boxes[key],bounds(r)),'Shared/animated sweep overlap: '+key[1]+' / '+r['instance_id'])
    old_panels=0
    for (owner,iid),r in base.items():
        if owner not in reports or iid.startswith('SHARED-'):continue
        code=iid[:8 if owner=='aquabio' else 9];m=reports[owner][code]
        for sign in m.get('signs',[]):
            plate=transform(sign['bounds'],r);centre=[sum(p)/2 for p in zip(*plate)];eye=[centre[0],1.7,centre[2]+4.5]
            for dx,y in itertools.product((-.35,0,.35),(1.1,1.5,1.9)):
                target=[centre[0]+dx*(plate[1][0]-plate[0][0]),y,plate[1][2]]
                for key in shared:
                    if key[0]==owner:require(not segment_box(eye,target,boxes[key]),'Shared blocks original panel: '+key[1]+' / '+iid)
            old_panels+=1
    return dict(instances=252,shared_instances=12,minimum_shared_clearance_m=min(gaps),animation_samples=200,original_panel_views_checked=old_panels,placements=placements,limits='Conservative imported AABBs, sample times and reading rays; no exact triangle collision or continuous physical simulation. GPU captures provide separate visual evidence.')


def write_plan(path,rows,result):
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="1060" viewBox="0 0 1120 1060">',
         '<rect width="1120" height="1060" fill="#f4f3ec"/>',
         '<text x="24" y="30" font-family="sans-serif" font-size="22">EcoSphere 6D — distribución definitiva desde AABB importadas</text>']
    colors=['#d79126','#258caa','#5a975e','#87654a','#aa633a']
    for index,owner in enumerate(['hub','ecoenergy','aquabio','ecocycle']):
        ox=25+index%2*550;oy=60+index//2*480
        svg += [f'<g transform="translate({ox},{oy})" font-family="sans-serif">',f'<text x="0" y="18" font-size="19">{owner}</text>',
                '<rect x="18" y="32" width="440" height="425" fill="#e3e8d9" stroke="#8b9683"/>']
        for r in rows:
            if r['owner']!=owner or float(r['time_s'])!=0:continue
            b=bounds(r);iid=r['instance_id'];shared=iid.startswith('SHARED-')
            floor=iid.startswith(('PLAZA-001','PLAZA-002','ENERGY-001','AQUA-001','CYCLE-001'))
            color=colors[int(iid[7:10])-1] if shared else '#c3c9bb' if floor else '#d5ba91' if '-003-' in iid else '#718276'
            x=235+b[0][0]*8;y=244+b[0][2]*8;w=(b[1][0]-b[0][0])*8;h=(b[1][2]-b[0][2])*8
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{w:.3f}" height="{h:.3f}" fill="{color}" opacity="{1 if shared or floor else .36}" stroke="#4c5d4e" stroke-width=".4"/>')
            if shared:svg.append(f'<text x="{x+8:.3f}" y="{y-3:.3f}" font-size="10">{html.escape(iid[:10])}</text>')
        for view in result['placements']:
            if view['owner']!=owner:continue
            eye=view['observer'];target=view['target'];x=235+eye[0]*8;y=244+eye[2]*8
            svg += [f'<line x1="{x:.3f}" y1="{y:.3f}" x2="{235+target[0]*8:.3f}" y2="{244+target[2]*8:.3f}" stroke="#1f4a7b"/>',f'<circle cx="{x:.3f}" cy="{y:.3f}" r="2" fill="#1f4a7b"/>']
        svg.append('</g>')
    svg += ['<text x="25" y="1040" font-family="sans-serif" font-size="12">Metros. Gris: contenido previo; marrón: caminos; color: compartidos; azul: observación a 1.7 m. AABB conservadoras.</text>','</svg>']
    text='\n'.join(svg)+'\n';ET.fromstring(text);path.write_text(text)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--bounds',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--self-test',action='store_true')
    p.add_argument('--plan',type=Path)
    a=p.parse_args()
    with a.bounds.open() as f:rows=list(csv.DictReader(f))
    result=audit(a.root,rows);rejections=0
    if a.self_test:
        bad=copy.deepcopy(rows);r=next(r for r in bad if r['owner']=='hub' and r['instance_id'].startswith('SHARED-'))
        for axis in ('x','z'):
            midpoint=(float(r['min_'+axis])+float(r['max_'+axis]))/2
            for end in ('min_','max_'):r[end+axis]=str(float(r[end+axis])-midpoint)
        for invalid in [bad,[r for r in rows if not r['instance_id'].startswith('SHARED-001')],rows[:-1]]:
            try:audit(a.root,invalid)
            except ValueError:rejections+=1
            else:raise AssertionError('Negative shared space case accepted')
    result['negative_rejections']=rejections;a.report.write_text(json.dumps(result,indent=2)+'\n')
    if a.plan:write_plan(a.plan,rows,result)
    print('SHARED SPACE PASS: 12 added instances; original circulation/panel views; 200 motion samples; negative rejections',rejections)


if __name__=='__main__':main()
