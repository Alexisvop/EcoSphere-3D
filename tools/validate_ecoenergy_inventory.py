"""Valida archivos, hashes, instancias, interacciones y animaciones de EcoEnergy."""
import argparse
import copy
import csv
import hashlib
import json
import math
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def rows(path):
    with path.open(newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))


def confined(root, name):
    path = (root / name).resolve()
    require(path.is_relative_to(root), 'Ruta fuera del proyecto: ' + name)
    return path


def split_ids(text):
    return [part for part in (text or '').split(';') if part]


def validate(root, catalog, instances, interactions, animations, imports, usage, require_definitive=False):
    expected = {f'ENERGY-{i:03}' for i in range(1, 29)}
    require(len(catalog) == 28 and {r['model_id'] for r in catalog} == expected,
            'Catálogo EcoEnergy requiere exactamente ENERGY-001..ENERGY-028')
    require(len(imports) == 28 and {r['model_id'] for r in imports} == expected,
            'Informe real de importación EcoEnergy incompleto')
    imported = {r['model_id']: r for r in imports}
    exported = {r['model_id']: r for r in json.loads(
        (root / 'docs/assets/ECOENERGY_EXPORT_REPORT.json').read_text(encoding='utf-8')
    )['models']}
    require(len({r['geometry_signature'] for r in exported.values()}) == 28,
            'Geometrías fuente duplicadas en EcoEnergy')

    valid_paths = {r['glb_file']: r['model_id'] for r in catalog}
    physical = {p.resolve() for p in (root / 'assets/installations/ecoenergy/models').rglob('*.glb')}
    require(len(valid_paths) == 28 and physical == {confined(root, p) for p in valid_paths},
            'El catálogo EcoEnergy debe corresponder exactamente a los 28 GLB físicos')

    counts = {key: 0 for key in expected}
    instance_map = {}
    for item in instances:
        iid = item['instance_id']
        require(iid and iid not in instance_map, 'ID de instancia vacío/duplicado: ' + str(iid))
        require(not iid.startswith('PLAZA-') and iid != 'sample', 'Instancia ajena mezclada en EcoEnergy: ' + iid)
        instance_map[iid] = item
        require(item['model_id'] in expected and item['owner'] == 'ecoenergy',
                'Propietario/modelo de instancia inválido: ' + iid)
        require(valid_paths.get(item['glb_file']) == item['model_id'],
                'Instancia/ruta GLB inconsistente: ' + iid)
        values = {k: float(item[k]) for k in ['x', 'y', 'z', 'qw', 'qx', 'qy', 'qz', 'sx', 'sy', 'sz']}
        require(all(math.isfinite(v) for v in values.values()), 'Transformación no finita: ' + iid)
        require(all(values[k] > .0001 for k in ['sx', 'sy', 'sz']), 'Escala inválida: ' + iid)
        require(abs(sum(values[k] ** 2 for k in ['qw', 'qx', 'qy', 'qz']) - 1) < .0001,
                'Quaternion no normalizado: ' + iid)
        require(item['interactive'] in {'true', 'false'}, 'Flag interactive inválido: ' + iid)
        require(item['animation_family'] in {'none', 'simple', 'keyframe', 'procedural'},
                'Familia de animación inválida en instancia: ' + iid)
        counts[item['model_id']] += 1

    # Validar las 6 interacciones educativas definitivas.
    expected_inter = {f'INT-ENERGY-{i:03}' for i in range(1, 7)}
    require(len(interactions) == 6 and {r['interaction_id'] for r in interactions} == expected_inter,
            'Se requieren exactamente 6 interacciones INT-ENERGY-001..006')
    inter_by_model = {k: [] for k in expected}
    inter_instances = set()
    for inter in interactions:
        iid = inter['instance_id']
        require(iid in instance_map and iid not in inter_instances,
                'Instancia interactiva inexistente o duplicada: ' + iid)
        inter_instances.add(iid)
        require(instance_map[iid]['interactive'] == 'true' and instance_map[iid]['model_id'] == inter['model_id'],
                'Relación instancia/modelo interactivo inconsistente: ' + iid)
        require(bool(inter['name']) and bool(inter['operations']) and bool(inter['educational_explanation']),
                'Metadatos educativos incompletos en interacción: ' + inter['interaction_id'])
        inter_by_model[inter['model_id']].append(inter['interaction_id'])
        if require_definitive:
            require(inter['status'] == 'definitive' and inter['validated'] == 'true',
                    'Interacción no validada: ' + inter['interaction_id'])
            for col in ('evidence_before', 'evidence_selected', 'evidence_manipulated'):
                png = confined(root, inter[col])
                require(png.is_file() and png.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),
                        f'Evidencia PNG inválida ({col}): ' + inter['interaction_id'])
    require({k for k, v in instance_map.items() if v['interactive'] == 'true'} == inter_instances,
            'Las instancias con interactive=true deben coincidir con ECOENERGY_INTERACTIONS.csv')

    # Validar las 8 animaciones definitivas (4 simple, 2 keyframe, 2 procedural).
    expected_anim = {f'ANIM-ENERGY-{i:03}' for i in range(1, 9)}
    require(len(animations) == 8 and {r['animation_id'] for r in animations} == expected_anim,
            'Se requieren exactamente 8 animaciones ANIM-ENERGY-001..008')
    family_counts = {'simple': 0, 'keyframe': 0, 'procedural': 0}
    anim_by_model = {k: [] for k in expected}
    anim_instances = set()
    for anim in animations:
        iid = anim['instance_id']
        fam = anim['family']
        require(iid in instance_map and iid not in anim_instances,
                'Instancia animada inexistente o duplicada: ' + iid)
        anim_instances.add(iid)
        require(fam in family_counts and instance_map[iid]['animation_family'] == fam,
                'Familia de animación inconsistente: ' + anim['animation_id'])
        require(instance_map[iid]['model_id'] == anim['model_id'],
                'Modelo animado inconsistente: ' + anim['animation_id'])
        require(anim['fps_independent'] == 'true' and bool(anim['mechanism']) and bool(anim['parameters']),
                'Metadatos o independencia de FPS faltantes: ' + anim['animation_id'])
        family_counts[fam] += 1
        anim_by_model[anim['model_id']].append(anim['animation_id'])
        if require_definitive:
            require(anim['status'] == 'definitive' and anim['validated'] == 'true',
                    'Animación no validada: ' + anim['animation_id'])
            for col in ('evidence_t0', 'evidence_t1'):
                png = confined(root, anim[col])
                require(png.is_file() and png.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),
                        f'Evidencia PNG inválida ({col}): ' + anim['animation_id'])
    require(family_counts == {'simple': 4, 'keyframe': 2, 'procedural': 2},
            'Conteo por familia debe ser 4 simple, 2 keyframe y 2 procedural')
    require({k for k, v in instance_map.items() if v['animation_family'] != 'none'} == anim_instances,
            'Las instancias animadas deben coincidir con ECOENERGY_ANIMATIONS.csv')

    proof = {r['model_id']: r for r in usage} if usage else {}
    definitive = 0
    for item in catalog:
        ident = item['model_id']
        actual = imported[ident]
        path = confined(root, item['glb_file'])
        require(path.is_file() and path.parent == root / 'assets/installations/ecoenergy/models',
                'GLB faltante o fuera de ecoenergy/models: ' + ident)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        require(digest == item['sha256'] == exported[ident]['sha256'], 'Hash GLB inconsistente: ' + ident)
        source, _, object_id = item['source_file'].partition('#')
        require(source == 'assets/installations/ecoenergy/sources/ecoenergy.blend'
                and confined(root, source).is_file() and object_id == ident,
                'Fuente Blender inválida: ' + ident)
        require(item['originality'] == 'original' and bool(item['provenance']) and bool(item['educational_function']),
                'Procedencia o función educativa ausente: ' + ident)
        require(int(item['triangle_count']) == int(actual['triangles']) == exported[ident]['triangles'] > 0,
                'Triángulos inconsistentes: ' + ident)
        for key in ['width_m', 'height_m', 'depth_m']:
            require(math.isclose(float(item[key]), float(actual[key]), rel_tol=.0005, abs_tol=.0001),
                    'Dimensiones inconsistentes: ' + ident)
        require(int(item['instance_count']) == counts[ident] > 0,
                'Conteo de instancias inconsistente: ' + ident)
        require(sorted(split_ids(item['interaction_ids'])) == sorted(inter_by_model[ident]),
                'Relación interaction_ids inconsistente en catálogo: ' + ident)
        require(sorted(split_ids(item['animation_ids'])) == sorted(anim_by_model[ident]),
                'Relación animation_ids inconsistente en catálogo: ' + ident)
        if item['status'] == 'definitive':
            require(item['validated'] == 'true', 'Modelo definitivo no validado: ' + ident)
            require(ident in proof and int(proof[ident]['changed_pixels']) >= 30,
                    'Sin uso real en framebuffer: ' + ident)
            require(int(proof[ident]['instances']) == counts[ident],
                    'Uso real no coincide con instancias: ' + ident)
            evidence = confined(root, item['evidence_file'])
            require(evidence.is_file() and evidence.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),
                    'Evidencia PNG ausente/inválida: ' + ident)
            definitive += 1
        else:
            require(item['validated'] == 'false', 'Modelo provisional no debe declararse validado: ' + ident)
    require(not require_definitive or definitive == 28, 'No hay 28 recursos EcoEnergy definitivos verificados')
    return definitive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--import-report', type=Path, required=True)
    parser.add_argument('--usage-report', type=Path)
    parser.add_argument('--require-definitive', action='store_true')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    catalog = rows(root / 'docs/assets/ECOENERGY_INVENTORY.csv')
    instances = rows(root / 'docs/assets/ECOENERGY_INSTANCES.csv')
    interactions = rows(root / 'docs/assets/ECOENERGY_INTERACTIONS.csv')
    animations = rows(root / 'docs/assets/ECOENERGY_ANIMATIONS.csv')
    imports = rows(args.import_report)
    usage = rows(args.usage_report) if args.usage_report else []
    count = validate(root, catalog, instances, interactions, animations, imports, usage, args.require_definitive)

    if args.self_test:
        mutations = [
            ('modelo duplicado', lambda c, i, x, a: c[1].update(model_id=c[0]['model_id'])),
            ('definitivo sin validar', lambda c, i, x, a: c[0].update(status='definitive', validated='false')),
            ('GLB faltante', lambda c, i, x, a: c[0].update(glb_file='assets/installations/ecoenergy/models/missing.glb')),
            ('hash corrupto', lambda c, i, x, a: c[0].update(sha256='0' * 64)),
            ('instancia duplicada', lambda c, i, x, a: i[1].update(instance_id=i[0]['instance_id'])),
            ('conteo inconsistente', lambda c, i, x, a: c[0].update(instance_count='999')),
            ('ruta fuera del proyecto', lambda c, i, x, a: c[0].update(glb_file='../outside.glb')),
            ('pose no finita', lambda c, i, x, a: i[0].update(x='nan')),
            ('interaccion huerfana', lambda c, i, x, a: x[0].update(instance_id='ENERGY-999-missing')),
            ('familia animacion alterada', lambda c, i, x, a: a[0].update(family='procedural')),
        ]
        for name, mutate in mutations:
            c = copy.deepcopy(catalog)
            i = copy.deepcopy(instances)
            x = copy.deepcopy(interactions)
            a = copy.deepcopy(animations)
            mutate(c, i, x, a)
            try:
                validate(root, c, i, x, a, imports, usage, args.require_definitive)
            except (ValueError, OSError):
                print('Expected rejection:', name)
            else:
                raise AssertionError('No se rechazó: ' + name)

    print(f'ECOENERGY INVENTORY PASS: 28 modelos únicos; {len(instances)} instancias; '
          f'6 interacciones; 8 animaciones (4S/2K/2P); {count} definitivos verificados')


if __name__ == '__main__':
    main()
