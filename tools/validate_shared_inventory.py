"""6D catalog checks; filesystem contents, importer output and per-owner instances."""
import argparse
import copy
import csv
import hashlib
import json
from pathlib import Path


def require(ok, message):
    if not ok:
        raise ValueError(message)


def audit(root, models, report, instances, imported):
    ids={f'SHARED-{n:03}' for n in range(1,6)}
    require(len(models)==5 and {m['model_id'] for m in models}==ids,'Exactly five shared IDs')
    require(len(report)==5 and {m['model_id'] for m in report}==ids,'Five export report entries')
    require(len(imported)==5 and {m['model_id'] for m in imported}==ids,'Five imported entries')
    exports={m['model_id']:m for m in report};cpu={m['model_id']:m for m in imported}
    previous=[];previous_shapes=set()
    for area,count in [('PLAZA',25),('ECOENERGY',28),('AQUABIO',28),('ECOCYCLE',24)]:
        with (root/f'docs/assets/{area}_INVENTORY.csv').open() as f:rows=list(csv.DictReader(f))
        require(len(rows)==count,'Original catalog count '+area)
        for row in rows:
            require(row['status']=='definitive','Original model no longer definitive')
            p=root/row['glb_file'];require(hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'],'Original GLB changed: '+str(p))
        previous.extend(rows)
        old=json.loads((root/f'docs/assets/{area}_EXPORT_REPORT.json').read_text())
        previous_shapes.update(m['geometry_signature'] for m in old['models'])
    require(len(previous)==105 and len({m['model_id'] for m in previous+models})==110,'110 unique catalog IDs')
    evidence_root=root/'docs/evidence/phase6d'
    captures={c['file']:c for c in json.loads((evidence_root/'capture-integrity.json').read_text())['captures']}
    signatures=set()
    for m in models:
        ident=m['model_id'];e=exports[ident];c=cpu[ident]
        require(m['status']=='definitive' and m['validated']=='true','Shared model not validated/definitive')
        evidence=root/m['evidence_file'];relative=evidence.relative_to(evidence_root).as_posix()
        require(relative in captures and evidence.is_file(),'Shared framebuffer evidence missing')
        require(hashlib.sha256(evidence.read_bytes()).hexdigest()==captures[relative]['sha256'],'Framebuffer evidence hash differs')
        require(m['originality']=='original' and m['provenance'] and m['license'],'Missing provenance/license')
        require(m['source_file']=='assets/shared/sources/shared.blend' and (root/m['source_file']).is_file(),'Editable shared master missing')
        require(m['glb_file']==f'assets/shared/models/{ident}.glb','Noncanonical shared path')
        raw=(root/m['glb_file']).read_bytes();require(raw[:4]==b'glTF','Expected GLB')
        digest=hashlib.sha256(raw).hexdigest();require(digest==m['sha256']==e['sha256'],'Shared hash mismatch')
        length=int.from_bytes(raw[12:16],'little');doc=json.loads(raw[20:20+length])
        require(not doc.get('images') and not doc.get('textures') and not any('uri' in b for b in doc['buffers']),'External/texture dependency')
        require(set(doc.get('extensionsUsed',[]))<={'KHR_materials_unlit'},'Unexpected glTF extension')
        require(e['geometry_signature'] not in signatures|previous_shapes,'Geometry signature reused')
        signatures.add(e['geometry_signature'])
        require(int(c['triangles'])==int(m['triangle_count'])==e['triangles'] and 0<e['triangles']<3000,'Triangle budget/import mismatch')
        require(int(c['textures'])==0 and len(e['materials'])<=8,'Unexpected texture/material count')
        for key,index in [('width_m',0),('height_m',1),('depth_m',2)]:
            require(abs(float(c[key])-e['dimensions_m'][index])<.0002 and abs(float(m[key])-float(c[key]))<.0002,'Metric dimensions differ')
    require(len(instances)==12 and len({(i['owner'],i['instance_id']) for i in instances})==12,'Twelve unique shared instance IDs')
    owners={'hub':2,'ecoenergy':3,'aquabio':3,'ecocycle':4}
    require({i['owner'] for i in instances}==set(owners),'All four zones must receive resources')
    for owner,count in owners.items():require(sum(i['owner']==owner for i in instances)==count,'Owner shared count: '+owner)
    for ident in ids:
        used={i['owner'] for i in instances if i['model_id']==ident};require(len(used)>=2,'Insufficient reuse: '+ident)
        require(sum(i['model_id']==ident for i in instances)==int(next(m['instance_count'] for m in models if m['model_id']==ident)),'Inventory usage differs')
    require(len({i['owner'] for i in instances if i['model_id']=='SHARED-001'})==4,'Clock in all four zones')
    for i in instances:
        require(i['model_id'] in ids and i['glb_file']==f"assets/shared/models/{i['model_id']}.glb",'Instance canonical path')
        require(i['interactive']=='false' and i['animation_family']=='none','New functionality outside shared static scope')
    original_instances=0
    for area in ['PLAZA','ECOENERGY','AQUABIO','ECOCYCLE']:
        with (root/f'docs/assets/{area}_INSTANCES.csv').open() as f:original_instances+=len(list(csv.DictReader(f)))
    require(original_instances==240,'Original 240 instance rows changed')
    interactions=animations=0
    for area in ['ECOENERGY','AQUABIO','ECOCYCLE']:
        with (root/f'docs/assets/{area}_INTERACTIONS.csv').open() as f:interactions+=len(list(csv.DictReader(f)))
        with (root/f'docs/assets/{area}_ANIMATIONS.csv').open() as f:animations+=len(list(csv.DictReader(f)))
    require(interactions==20 and animations==24,'Original educational interactions/animations changed')
    return dict(unique_models=110,original_models=105,shared_models=5,original_instances=240,shared_instances=12,total_instances=252,educational_interactions=20,animations=24,shared_triangles=sum(e['triangles'] for e in report))


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--import-report',type=Path,required=True);p.add_argument('--self-test',action='store_true')
    a=p.parse_args();root=a.root
    with (root/'docs/assets/SHARED_INVENTORY.csv').open() as f:models=list(csv.DictReader(f))
    report=json.loads((root/'docs/assets/SHARED_EXPORT_REPORT.json').read_text())['models']
    with (root/'docs/assets/SHARED_INSTANCES.csv').open() as f:instances=list(csv.DictReader(f))
    with a.import_report.open() as f:imported=list(csv.DictReader(f))
    result=audit(root,models,report,instances,imported);rejected=0
    if a.self_test:
        cases=[]
        bad=copy.deepcopy(models);bad[0]['validated']='false';cases.append((bad,report,instances,imported))
        bad=copy.deepcopy(models);bad[0]['evidence_file']='docs/evidence/phase6d/shared/missing.png';cases.append((bad,report,instances,imported))
        bad=copy.deepcopy(models);bad[0]['sha256']='0'*64;cases.append((bad,report,instances,imported))
        bad=copy.deepcopy(models);bad[0]['model_id']='../escape';cases.append((bad,report,instances,imported))
        bad=copy.deepcopy(report);bad[1]['geometry_signature']=bad[0]['geometry_signature'];cases.append((models,bad,instances,imported))
        bad=copy.deepcopy(instances);bad[1]=bad[0];cases.append((models,report,bad,imported))
        bad=copy.deepcopy(instances);bad[0]['owner']='unknown';cases.append((models,report,bad,imported))
        bad=copy.deepcopy(instances);bad[0]['interactive']='true';cases.append((models,report,bad,imported))
        bad=copy.deepcopy(instances);bad[0]['glb_file']='assets/copy.glb';cases.append((models,report,bad,imported))
        bad=copy.deepcopy(imported);bad[0]['triangles']='0';cases.append((models,report,instances,bad))
        for case in cases:
            try:audit(root,*case)
            except ValueError:rejected+=1
            else:raise AssertionError('Negative shared inventory case accepted')
    print('SHARED INVENTORY PASS:',json.dumps(result),'negative_rejections=',rejected)


if __name__=='__main__':main()
