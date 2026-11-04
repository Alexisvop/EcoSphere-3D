"""Valida archivos e inventarios de plaza con stdlib; no modifica recursos."""
import argparse
import copy
import csv
import hashlib
import json
import math
from pathlib import Path


def require(condition,message):
    if not condition: raise ValueError(message)


def rows(path):
    with path.open(newline='') as stream: return list(csv.DictReader(stream))


def confined(root,name):
    path=(root/name).resolve()
    require(path.is_relative_to(root),'Ruta fuera del proyecto: '+name)
    return path


def validate(root,catalog,instances,imports,usage,require_definitive=False):
    expected={f'PLAZA-{i:03}' for i in range(1,26)}
    require(len(catalog)==25 and {r['model_id'] for r in catalog}==expected,'IDs de modelos vacíos/duplicados/incompletos')
    variant_ids={'PLAZA-008-aquabio','PLAZA-008-ecocycle'}
    require(len(imports)==27 and {r['model_id'] for r in imports}==expected|variant_ids,'Informe real de importación incompleto')
    imported={r['model_id']:r for r in imports}
    exported={r['model_id']:r for r in json.loads((root/'docs/assets/PLAZA_EXPORT_REPORT.json').read_text())['models']}
    require(len({r['geometry_signature'] for r in exported.values()})==25,'Geometrías fuente duplicadas')
    counts={key:0 for key in expected};seen=set()
    variants=rows(root/'docs/assets/PLAZA_VARIANTS.csv')
    require(len(variants)==2 and len({v['variant_id'] for v in variants})==2,'Variantes de arco incompletas/duplicadas')
    valid_paths={r['glb_file']:r['model_id'] for r in catalog}
    for variant in variants:
        require(variant['base_model_id']=='PLAZA-008' and variant['counted_as_new_model']=='false','Variante contada como modelo nuevo')
        path=confined(root,variant['glb_file'])
        require(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==variant['sha256'],'Variante GLB ausente/corrupta')
        require(int(imported[variant['variant_id']]['triangles'])==int(variant['triangle_count'])>0,'Variante no importada')
        require(variant.get('status')=='definitive' and variant.get('validated')=='true','Variante sin validar')
        require(confined(root,variant['evidence_file']).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),'Variante sin evidencia actual')
        valid_paths[variant['glb_file']]=variant['base_model_id']
    physical={p.resolve() for p in (root/'assets/plaza/models').rglob('*.glb')}
    require(len(valid_paths)==27 and physical=={confined(root,p) for p in valid_paths},
            'El catálogo debe corresponder exactamente a los 27 GLB físicos')
    for item in instances:
        require(item['instance_id'] and item['instance_id'] not in seen,'ID de instancia vacío/duplicado')
        seen.add(item['instance_id'])
        require(item['model_id'] in expected and item['owner']=='hub','Propietario/modelo de instancia inválido')
        require(valid_paths.get(item['glb_file'])==item['model_id'],'Instancia/ruta de recurso inconsistente')
        values={key:float(item[key]) for key in ['x','y','z','qw','qx','qy','qz','sx','sy','sz']}
        require(all(math.isfinite(v) for v in values.values()),'Transformación no finita')
        require(all(values[k]>.0001 for k in ['sx','sy','sz']),'Escala inválida')
        require(abs(sum(values[k]**2 for k in ['qw','qx','qy','qz'])-1)<.0001,'Quaternion no normalizado')
        counts[item['model_id']]+=1
    proof={r['model_id']:r for r in usage} if usage else {}
    definitive=0
    for item in catalog:
        ident=item['model_id'];actual=imported[ident]
        path=confined(root,item['glb_file'])
        require(path.is_file() and path.parent==root/'assets/plaza/models','GLB faltante o fuera del catálogo: '+ident)
        digest=hashlib.sha256(path.read_bytes()).hexdigest()
        require(digest==item['sha256']==exported[ident]['sha256'],'Hash GLB inconsistente: '+ident)
        source,_,object_id=item['source_file'].partition('#')
        require(confined(root,source).is_file() and object_id==ident,'Fuente editable inválida: '+ident)
        require(item['originality']=='original' and bool(item['provenance']),'Procedencia ausente: '+ident)
        require(int(item['triangle_count'])==int(actual['triangles'])==exported[ident]['triangles']>0,'Triángulos inconsistentes: '+ident)
        for key in ['width_m','height_m','depth_m']:
            require(math.isclose(float(item[key]),float(actual[key]),rel_tol=.0005,abs_tol=.0001),'Escala/ejes inconsistentes: '+ident)
        require(int(item['instance_count'])==counts[ident]>0,'Conteo de instancias inconsistente: '+ident)
        if item['status']=='definitive':
            require(item['validated']=='true','Modelo definitivo no validado: '+ident)
            require(ident in proof and int(proof[ident]['changed_pixels'])>=30,'Sin uso real de framebuffer: '+ident)
            require(int(proof[ident]['instances'])==counts[ident],'Uso real no coincide con instancias: '+ident)
            evidence=confined(root,item['evidence_file'])
            require(evidence.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),'Evidencia PNG ausente/inválida: '+ident)
            definitive+=1
        else:
            require(item['validated']=='false','Estado provisional no debe declararse validado: '+ident)
    require(not require_definitive or definitive==25,'No hay 25 recursos definitivos verificados')
    return definitive


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--import-report',type=Path,required=True)
    parser.add_argument('--usage-report',type=Path)
    parser.add_argument('--require-definitive',action='store_true')
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args();root=args.root.resolve()
    catalog=rows(root/'docs/assets/PLAZA_INVENTORY.csv')
    instances=rows(root/'docs/assets/PLAZA_INSTANCES.csv')
    imports=rows(args.import_report)
    usage=rows(args.usage_report) if args.usage_report else []
    count=validate(root,catalog,instances,imports,usage,args.require_definitive)
    for line in (root/'docs/assets/PLAZA_RENDER_BASELINE.sha256').read_text().splitlines():
        digest,name=line.split(maxsplit=1)
        require(hashlib.sha256(confined(root,name).read_bytes()).hexdigest()==digest,'Renderer/shader modificado: '+name)
    if args.self_test:
        mutations=[
            ('modelo duplicado',lambda c,i:c[1].update(model_id=c[0]['model_id'])),
            ('definitivo sin validar',lambda c,i:c[0].update(status='definitive',validated='false')),
            ('GLB faltante',lambda c,i:c[0].update(glb_file='assets/plaza/models/missing.glb')),
            ('hash corrupto',lambda c,i:c[0].update(sha256='0'*64)),
            ('instancia duplicada',lambda c,i:i[1].update(instance_id=i[0]['instance_id'])),
            ('conteo inconsistente',lambda c,i:c[0].update(instance_count='999')),
            ('ruta fuera del proyecto',lambda c,i:c[0].update(glb_file='../outside.glb')),
            ('pose no finita',lambda c,i:i[0].update(x='nan')),
        ]
        for name,mutate in mutations:
            c,i=copy.deepcopy(catalog),copy.deepcopy(instances);mutate(c,i)
            try: validate(root,c,i,imports,usage,args.require_definitive)
            except (ValueError,OSError): print('Expected rejection:',name)
            else: raise AssertionError('No se rechazó: '+name)
    print(f'PLAZA INVENTORY PASS: 25 modelos / 27 GLB importados; {len(instances)} instancias; {count} definitivos con evidencia; render/shaders intactos')


if __name__=='__main__': main()
