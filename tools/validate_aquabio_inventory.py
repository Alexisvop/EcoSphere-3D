"""Inventario AquaBio: archivos, SHA-256, procedencia, poses, acciones y evidencia real."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from validate_ecoenergy_inventory import rows, require, confined, split_ids


def validate(root, catalog, instances, interactions, animations, imports, usage, definitive):
    expected={f'AQUA-{n:03}' for n in range(1,29)}
    require(len(catalog)==28 and {r['model_id'] for r in catalog}==expected,'Se requieren exactamente 28 modelos AQUA')
    require(len(imports)==28 and {r['model_id'] for r in imports}==expected,'Importacion CPU incompleta')
    actual={r['model_id']:r for r in imports}
    exported={r['model_id']:r for r in json.loads((root/'docs/assets/AQUABIO_EXPORT_REPORT.json').read_text())['models']}
    require(set(exported)==expected and len({r['geometry_signature'] for r in exported.values()})==28,'Firmas geometricas no distintivas')
    paths={r['glb_file']:r['model_id'] for r in catalog}
    physical={p.resolve() for p in (root/'assets/installations/aquabio/models').rglob('*.glb')}
    require(len(paths)==28 and physical=={confined(root,p) for p in paths},'GLB fisicos/rutas no coinciden con catalogo')
    counts={k:0 for k in expected};placed={}
    for item in instances:
        iid=item['instance_id'];mid=item['model_id']
        require(iid and iid not in placed and iid.startswith(mid+'-'),'ID de instancia vacio duplicado o ajeno')
        require(mid in expected and item['owner']=='aquabio' and paths.get(item['glb_file'])==mid,'Propietario/modelo/ruta invalido')
        nums={k:float(item[k]) for k in ('x','y','z','qw','qx','qy','qz','sx','sy','sz')}
        require(all(math.isfinite(v) for v in nums.values()),'Pose no finita')
        require(all(nums[k]>.0001 for k in ('sx','sy','sz')),'Escala invalida')
        require(abs(sum(nums[k]**2 for k in ('qw','qx','qy','qz'))-1)<.0001,'Quaternion no normalizado')
        require(item['interactive'] in {'true','false'} and item['animation_family'] in {'none','simple','keyframe','procedural'},'Flags invalidos')
        placed[iid]=item;counts[mid]+=1
    require(len(placed)==52,'Se requieren 52 instancias reales, no modelos adicionales')
    inter_ids={k:[] for k in expected};anim_ids={k:[] for k in expected}
    def png(name):
        p=confined(root,name)
        require(p.is_file() and p.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),'Evidencia PNG ausente/invalida: '+name)
    require(len(interactions)==7 and {r['interaction_id'] for r in interactions}=={f'INT-AQUA-{n:03}' for n in range(1,8)},'Se requieren siete interacciones')
    seen=set()
    for item in interactions:
        iid=item['instance_id'];mid=item['model_id']
        require(iid in placed and iid not in seen and placed[iid]['model_id']==mid and placed[iid]['interactive']=='true','Interaccion huerfana/duplicada')
        require(item['name'] and item['educational_explanation'] and item['operations'],'Interaccion sin explicacion/acciones')
        seen.add(iid);inter_ids[mid].append(item['interaction_id'])
        if definitive:
            require(item['status']=='definitive' and item['validated']=='true','Interaccion sin validar')
            for key in ('evidence_before','evidence_selected','evidence_manipulated'):png(item[key])
    require(seen=={iid for iid,r in placed.items() if r['interactive']=='true'},'Interacciones difieren de instancias')
    require(len(animations)==8 and {r['animation_id'] for r in animations}=={f'ANIM-AQUA-{n:03}' for n in range(1,9)},'Se requieren ocho animaciones')
    seen=set();families={'simple':0,'keyframe':0,'procedural':0}
    for item in animations:
        iid=item['instance_id'];mid=item['model_id'];family=item['family']
        require(iid in placed and iid not in seen and placed[iid]['model_id']==mid,'Animacion huerfana/duplicada')
        require(family in families and placed[iid]['animation_family']==family,'Familia no coincide con descriptor')
        require(placed[iid]['interactive']=='false','Muestra animada debe separarse de interactiva')
        require(item['mechanism'] and item['parameters'] and item['fps_independent']=='true','Animacion sin parametros/independencia temporal')
        seen.add(iid);families[family]+=1;anim_ids[mid].append(item['animation_id'])
        if definitive:
            require(item['status']=='definitive' and item['validated']=='true','Animacion sin validar')
            png(item['evidence_t0']);png(item['evidence_t1'])
    require(families=={'simple':4,'keyframe':2,'procedural':2},'Familias deben ser 4S/2K/2P')
    require(seen=={iid for iid,r in placed.items() if r['animation_family']!='none'},'Animaciones difieren de instancias')
    proof={r['model_id']:r for r in usage};verified=0
    for item in catalog:
        mid=item['model_id'];path=confined(root,item['glb_file']);source,_,object_id=item['source_file'].partition('#')
        require(path.parent==root/'assets/installations/aquabio/models' and path.is_file(),'GLB faltante/fuera de AquaBio')
        require(hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']==exported[mid]['sha256'],'SHA-256 no coincide')
        require(source=='assets/installations/aquabio/sources/aquabio.blend' and confined(root,source).is_file() and object_id==mid,'Fuente Blender invalida')
        require(item['originality']=='original' and item['provenance'] and item['license'] and item['educational_function'],'Procedencia incompleta')
        require(int(item['triangle_count'])==int(actual[mid]['triangles'])==exported[mid]['triangles']>0,'Triangulos no coinciden')
        for i,key in enumerate(('width_m','height_m','depth_m')):
            require(math.isclose(float(item[key]),float(actual[mid][key]),rel_tol=.0005,abs_tol=.0001),'Dimensiones importadas no coinciden')
            require(math.isclose(float(item[key]),exported[mid]['dimensions_m'][i],rel_tol=.0005,abs_tol=.0001),'Dimensiones fuente no coinciden')
        require(int(item['instance_count'])==counts[mid]>0,'Conteo de instancias inconsistente')
        require(sorted(split_ids(item['interaction_ids']))==sorted(inter_ids[mid]),'Relacion interaccion/modelo inconsistente')
        require(sorted(split_ids(item['animation_ids']))==sorted(anim_ids[mid]),'Relacion animacion/modelo inconsistente')
        require(item['status'] in {'provisional','definitive'},'Estado desconocido')
        if item['status']=='definitive':
            require(item['validated']=='true','Definitivo sin validar')
            require(mid in proof and int(proof[mid]['changed_pixels'])>=30 and int(proof[mid]['instances'])==counts[mid],'Sin uso real en framebuffer')
            png(item['evidence_file']);verified+=1
        else:require(item['validated']=='false','Provisional marcado validado')
    require(not definitive or verified==28,'Se requieren 28 definitivos verificados')
    return verified


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--import-report',type=Path,required=True)
    parser.add_argument('--usage-report',type=Path);parser.add_argument('--require-definitive',action='store_true')
    parser.add_argument('--self-test',action='store_true');args=parser.parse_args();root=args.root.resolve()
    tables=[rows(root/('docs/assets/AQUABIO_'+name+'.csv')) for name in ('INVENTORY','INSTANCES','INTERACTIONS','ANIMATIONS')]
    imports=rows(args.import_report);usage=rows(args.usage_report) if args.usage_report and args.usage_report.is_file() else []
    count=validate(root,*tables,imports,usage,args.require_definitive)
    if args.self_test:
        mutations=[('modelo duplicado',0,1,{'model_id':'AQUA-001'}),('sin validar',0,0,{'status':'definitive','validated':'false'}),
            ('GLB faltante',0,0,{'glb_file':'assets/installations/aquabio/models/missing.glb'}),('hash corrupto',0,0,{'sha256':'0'*64}),
            ('instancia duplicada',1,1,{'instance_id':tables[1][0]['instance_id']}),('conteo inconsistente',0,0,{'instance_count':'999'}),
            ('escape de ruta',0,0,{'glb_file':'../outside.glb'}),('pose no finita',1,0,{'x':'nan'}),('escala invalida',1,0,{'sx':'0'}),
            ('interaccion huerfana',2,0,{'instance_id':'AQUA-999-missing'}),('familia invalida',3,0,{'family':'procedural'}),
            ('muestra no separada',1,1,{'interactive':'true'}),('licencia ausente',0,0,{'license':''}),('fuente ajena',0,0,{'source_file':'assets/plaza/sources/plaza.blend#AQUA-001'})]
        for name,table,index,change in mutations:
            altered=copy.deepcopy(tables);altered[table][index].update(change)
            try:validate(root,*altered,imports,usage,args.require_definitive)
            except (ValueError,OSError):print('Expected rejection:',name)
            else:raise AssertionError('No se rechazo: '+name)
    print(f'AQUABIO INVENTORY PASS: 28 modelos; 52 instancias; 7 interacciones; 8 animaciones (4S/2K/2P); {count} definitivos verificados')


if __name__=='__main__':main()
