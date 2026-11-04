"""Inventario EcoCycle: recursos, procedencia, integridad, registro e imágenes del framebuffer."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from validate_ecoenergy_inventory import rows, require, confined, split_ids


def validate(root,tables,imports,definitive):
    catalog,instances,interactions,animations=tables;expected={f'CYCLE-{n:03}' for n in range(1,25)}
    require(len(catalog)==24 and {r['model_id'] for r in catalog}==expected,'24 modelos CYCLE unicos')
    exported={m['model_id']:m for m in json.loads((root/'docs/assets/ECOCYCLE_EXPORT_REPORT.json').read_text())['models']}
    require(set(exported)==expected and len({m['geometry_signature'] for m in exported.values()})==24,'Geometrias no distintivas')
    imported={r['model_id']:r for r in imports};require(len(imports)==24 and set(imported)==expected,'Importacion incompleta')
    placed={};counts={mid:0 for mid in expected}
    for r in instances:
        iid=r['instance_id'];mid=r['model_id'];require(iid not in placed and iid.startswith(mid+'-') and mid in expected,'ID duplicado/ajeno')
        require(r['owner']=='ecocycle' and r['glb_file']==f'assets/installations/ecocycle/models/{mid}.glb','Ruta/owner incorrectos')
        require(all(math.isfinite(float(r[k])) for k in ('x','y','z','qw','qx','qy','qz','sx','sy','sz')),'Pose no finita')
        require(abs(sum(float(r[k])**2 for k in ('qw','qx','qy','qz'))-1)<.0001 and min(float(r[k]) for k in ('sx','sy','sz'))>.0001,'Escala/cuaternion invalido')
        require(r['interactive'] in ('true','false') and r['animation_family'] in ('none','simple','keyframe','procedural'),'Flags invalidos')
        placed[iid]=r;counts[mid]+=1
    require(len(placed)==38,'38 instancias requeridas')
    def png(name):
        p=confined(root,name);require(p.is_file() and p.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),'PNG faltante/invalido: '+name)
    relations={mid:{'interaction_ids':[],'animation_ids':[]} for mid in expected}
    for data,prefix,key,relation in ((interactions,'INT','interaction_id','interaction_ids'),(animations,'ANIM','animation_id','animation_ids')):
        count=7 if prefix=='INT' else 8;require(len(data)==count and {r[key] for r in data}=={f'{prefix}-CYCLE-{i:03}' for i in range(1,count+1)},'IDs de acciones incorrectos')
        seen=set()
        for r in data:
            iid=r['instance_id'];mid=r['model_id'];require(iid in placed and iid not in seen and placed[iid]['model_id']==mid,'Accion huerfana/duplicada');seen.add(iid)
            relations[mid][relation].append(r[key]);require(r.get('educational_explanation',r.get('mechanism')),'Sin explicacion educativa')
            if prefix=='INT':require(placed[iid]['interactive']=='true' and r['operations'],'Interaccion sin registro/acciones')
            else:require(placed[iid]['interactive']=='false' and placed[iid]['animation_family']==r['family'] and r['fps_independent']=='true' and r['parameters'],'Animacion incompatible')
            if definitive:
                require(r['status']=='definitive' and r['validated']=='true','Accion sin validar')
                for k,v in r.items():
                    if k.startswith('evidence'):png(v)
        required={iid for iid,r in placed.items() if (r['interactive']=='true' if prefix=='INT' else r['animation_family']!='none')}
        require(seen==required,'Acciones distintas del registro')
    require({f:sum(r['family']==f for r in animations) for f in ('simple','keyframe','procedural')}=={'simple':4,'keyframe':2,'procedural':2},'Familias incorrectas')
    paths={confined(root,r['glb_file']) for r in catalog}
    require(paths=={p.resolve() for p in (root/'assets/installations/ecocycle/models').glob('*.glb')},'Archivos fisicos/catalogo distintos')
    usage={r['model_id']:r for r in rows(root/'docs/evidence/phase6c/ecocycle/model-use.csv')} if definitive else {}
    for r in catalog:
        mid=r['model_id'];m=exported[mid];p=confined(root,r['glb_file'])
        require(hashlib.sha256(p.read_bytes()).hexdigest()==r['sha256']==m['sha256'],'Hash incorrecto')
        source,_,obj=r['source_file'].partition('#')
        require(source=='assets/installations/ecocycle/sources/ecocycle.blend' and confined(root,source).is_file() and obj==mid,'Fuente incorrecta')
        require(r['originality']=='original' and r['provenance'] and r['license'] and r['educational_function'],'Procedencia/licencia incompleta')
        require(int(r['triangle_count'])==int(imported[mid]['triangles'])==m['triangles']>0,'Triangulos distintos')
        for i,k in enumerate(('width_m','height_m','depth_m')):
            require(math.isclose(float(r[k]),float(imported[mid][k]),rel_tol=.0005,abs_tol=.0001) and math.isclose(float(r[k]),m['dimensions_m'][i],rel_tol=.0005,abs_tol=.0001),'Dimensiones distintas')
        require(int(r['instance_count'])==counts[mid]>0,'Conteo incorrecto')
        for key in ('interaction_ids','animation_ids'):require(sorted(split_ids(r[key]))==sorted(relations[mid][key]),'Relacion inconsistente')
        if definitive:
            require(r['status']=='definitive' and r['validated']=='true' and mid in usage and int(usage[mid]['changed_pixels'])>=30 and int(usage[mid]['instances'])==counts[mid],'Falta uso real/validacion')
            png(r['evidence_file'])
    if definitive:
        bounds_rows=rows(root/'docs/evidence/phase6c/ecocycle-space-bounds.csv')
        base={r['instance_id']:r for r in bounds_rows if float(r['time_s'])==0};at2={r['instance_id']:r for r in bounds_rows if float(r['time_s'])==2}
        capture=rows(root/'docs/evidence/phase6c/ecocycle/animation-states.csv');require(len(capture)==76,'Matrices capturadas incompletas')
        for r in capture:
            require(r['stage'] in ('t0','t2') and r['time_s'] in ('0','2'),'Tiempo de captura incorrecto')
            expected_row=(at2.get(r['instance_id'],base[r['instance_id']]) if r['stage']=='t2' else base[r['instance_id']])
            for c in range(4):
                for a in range(4):require(abs(float(r[f'm{c}{a}'])-float(expected_row[f'm{c}{a}']))<1e-4,'CPU/framebuffer difieren')
    return len(catalog)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--import-report',type=Path,required=True);p.add_argument('--require-definitive',action='store_true');p.add_argument('--self-test',action='store_true');a=p.parse_args();root=a.root.resolve()
    tables=[rows(root/f'docs/assets/ECOCYCLE_{n}.csv') for n in ('INVENTORY','INSTANCES','INTERACTIONS','ANIMATIONS')];imports=rows(a.import_report);validate(root,tables,imports,a.require_definitive)
    if a.self_test:
        for label,t,i,change in [('hash',0,0,{'sha256':'0'*64}),('ruta',0,0,{'glb_file':'../escape.glb'}),('procedencia',0,0,{'license':''}),('modelo duplicado',0,1,{'model_id':'CYCLE-001'}),('instancia duplicada',1,1,{'instance_id':tables[1][0]['instance_id']}),('pose',1,0,{'sx':'0'}),('accion huerfana',2,0,{'instance_id':'unknown'}),('familia',3,0,{'family':'procedural'})]:
            bad=copy.deepcopy(tables);bad[t][i].update(change)
            try:validate(root,bad,imports,a.require_definitive)
            except (ValueError,OSError,KeyError):print('Expected rejection:',label)
            else:raise AssertionError('Mutation accepted: '+label)
    print('ECOCYCLE INVENTORY PASS: 24 distinct models; 38 instances; 7 interactions; 4S/2K/2P; hashes/provenance/imports/evidence')
if __name__=='__main__':main()
