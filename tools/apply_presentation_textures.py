"""Original, deterministic base-color textures; append to GLB without re-exporting geometry.

Run from the repository root. --check verifies the committed assets and preserved data.
The Blender sources remain the modeling originals; the GLB are presentation assets.
"""
import copy
import csv
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
TEXTURES = ROOT / 'assets/common/textures/presentation'
MANIFEST = TEXTURES / 'manifest.json'
SIZE = 128
TARGETS = {
    'aquabio': {
        'AQUA-001': {'Aqua_stone': 'concrete'},
        'AQUA-003': {'Aqua_stone': 'concrete'},
        'AQUA-010': {'Aqua_water': 'water'},
        'AQUA-023': {'Aqua_water': 'water', 'Aqua_stone': 'concrete'},
        'AQUA-027': {'Aqua_bark': 'wood'},
    },
    'ecoenergy': {
        'ENERGY-001': {'Energy_concrete': 'concrete'},
        'ENERGY-003': {'Energy_pavement': 'concrete'},
        'ENERGY-004': {'Energy_aluminum': 'metal', 'Energy_steel': 'metal', 'Energy_concrete': 'concrete'},
        'ENERGY-005': {'Energy_aluminum': 'metal'},
        'ENERGY-010': {'Energy_steel': 'painted', 'Energy_aluminum': 'metal', 'Energy_concrete': 'concrete'},
    },
    'ecocycle': {
        'CYCLE-001': {'Cycle_ground': 'concrete', 'Cycle_stone': 'concrete'},
        'CYCLE-005': {'Cycle_blue': 'painted', 'Cycle_yellow': 'painted', 'Cycle_white': 'painted', 'Cycle_steel': 'metal'},
        'CYCLE-007': {'Cycle_charcoal': 'belt', 'Cycle_steel': 'metal'},
        'CYCLE-023': {'Cycle_wood': 'wood', 'Cycle_steel': 'metal'},
    },
}


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def canonical(doc):
    return json.dumps(doc, sort_keys=True, separators=(',', ':')).encode()


def png(kind):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))

    pixels = bytearray()
    for y in range(SIZE):
        pixels.append(0)
        for x in range(SIZE):
            u, v = x / SIZE, y / SIZE
            a, b = math.tau * u, math.tau * v
            if kind == 'water':
                wave = math.cos(3*b + .7*math.sin(2*a))
                value = 235 + 8*math.sin(a + b) + 12*max(0, wave)**12
            elif kind == 'wood':
                grain = math.sin(8*a + .7*math.sin(b) + .3*math.sin(3*b))
                value = 240 + 8*grain - 22*max(0, grain)**14
            elif kind == 'concrete':
                value = 249 + 3*math.sin(a)*math.cos(2*b)
                if x % 64 < 2 or y % 64 < 2:
                    value -= 24
            elif kind == 'metal':
                value = 246 + 5*math.sin(24*b) + 3*math.sin(3*b)
            elif kind == 'painted':
                value = 248 + 4*math.sin(2*a)*math.cos(3*b) - 7*max(0, math.cos(8*b))**16
            elif kind == 'belt':
                value = 243 + 4*math.cos(a) - 27*max(0, math.cos(8*b))**12
            else:
                raise ValueError(kind)
            code = max(0, min(255, round(value)))
            pixels.extend((code, code, code, 255))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', SIZE, SIZE, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(pixels, 9)) + chunk(b'IEND', b''))


def unpack(raw):
    magic, version, length, json_size, tag = struct.unpack_from('<5I', raw)
    assert (magic, version, length, tag) == (0x46546C67, 2, len(raw), 0x4E4F534A)
    bin_size, tag = struct.unpack_from('<2I', raw, 20 + json_size)
    assert tag == 0x004E4942 and 28 + json_size + bin_size == len(raw)
    return json.loads(raw[20:20 + json_size]), raw[28 + json_size:]


def verify(raw, record, images):
    doc, binary = unpack(raw)
    original = copy.deepcopy(doc)
    assert sha(binary[:record['original_bin_bytes']]) == record['original_bin_sha256']
    for material in original['materials']:
        if material['name'] not in record['materials']:
            continue
        kind = record['materials'][material['name']]
        binding = material['pbrMetallicRoughness'].pop('baseColorTexture')
        texture = doc['textures'][binding['index']]
        image = doc['images'][texture['source']]
        view = doc['bufferViews'][image['bufferView']]
        assert binary[view['byteOffset']:view['byteOffset'] + view['byteLength']] == images[kind]
        assert doc['samplers'][texture['sampler']] == {
            'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}
    for key, count in record['original_array_counts'].items():
        if count is None:
            original.pop(key, None)
        else:
            original[key] = original[key][:count]
    original['buffers'][0]['byteLength'] = record['original_buffer_length']
    assert sha(canonical(original)) == record['original_document_sha256'], 'Non-texture data changed'


def apply(raw, mapping, images):
    doc, binary = unpack(raw)
    record = {
        'materials': mapping, 'original_sha256': sha(raw),
        'original_bin_bytes': len(binary), 'original_bin_sha256': sha(binary),
        'original_buffer_length': doc['buffers'][0]['byteLength'],
        'original_document_sha256': sha(canonical(doc)),
        'original_array_counts': {k: len(doc[k]) if k in doc else None
                                  for k in ('bufferViews', 'images', 'textures', 'samplers')},
    }
    bindings = {}
    assert set(mapping) <= {m['name'] for m in doc['materials']}
    for kind in sorted(set(mapping.values())):
        binary += b'\0' * (-len(binary) % 4)
        view_index = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(images[kind])})
        binary += images[kind]
        image_index = len(doc.setdefault('images', []))
        doc['images'].append({'name': 'presentation-' + kind, 'bufferView': view_index, 'mimeType': 'image/png'})
        sampler_index = len(doc.setdefault('samplers', []))
        doc['samplers'].append({'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497})
        bindings[kind] = len(doc.setdefault('textures', []))
        doc['textures'].append({'source': image_index, 'sampler': sampler_index})
    for index, material in enumerate(doc['materials']):
        if material['name'] in mapping:
            pbr = material['pbrMetallicRoughness']
            assert 'baseColorTexture' not in pbr, 'Do not replace an existing texture'
            assert all('TEXCOORD_0' in p['attributes'] for m in doc['meshes']
                       for p in m['primitives'] if p.get('material') == index)
            pbr['baseColorTexture'] = {'index': bindings[mapping[material['name']]]}
    doc['buffers'][0]['byteLength'] = len(binary)
    encoded = canonical(doc)
    encoded += b' ' * (-len(encoded) % 4)
    binary += b'\0' * (-len(binary) % 4)
    result = (struct.pack('<5I', 0x46546C67, 2, 28 + len(encoded) + len(binary), len(encoded), 0x4E4F534A)
              + encoded + struct.pack('<2I', len(binary), 0x004E4942) + binary)
    verify(result, record, images)
    record.update(sha256=sha(result), bytes_added=len(result) - len(raw))
    return result, record


def main():
    assert sys.argv[1:] in ([], ['--check']), 'Usage: apply_presentation_textures.py [--check]'
    check = '--check' in sys.argv
    images = {kind: png(kind) for models in TARGETS.values() for mapping in models.values() for kind in mapping.values()}
    if check or MANIFEST.exists():
        manifest = json.loads(MANIFEST.read_text())
        assert manifest['textures'] == {k: sha(v) for k, v in sorted(images.items())}
        expected_paths = {f'assets/installations/{z}/models/{mid}.glb': mapping
                          for z, models in TARGETS.items() for mid, mapping in models.items()}
        assert {p: r['materials'] for p, r in manifest['models'].items()} == expected_paths
        for kind, raw in images.items():
            assert (TEXTURES / (kind + '.png')).read_bytes() == raw
        for path, record in manifest['models'].items():
            raw = (ROOT / path).read_bytes()
            assert sha(raw) == record['sha256']
            verify(raw, record, images)
        print('PASS: 6 original textures, 14 GLB; geometry/UV/nodes/original materials/images preserved')
        return
    TEXTURES.mkdir(parents=True, exist_ok=True)
    for kind, raw in images.items():
        (TEXTURES / (kind + '.png')).write_bytes(raw)
    manifest = {'textures': {k: sha(v) for k, v in sorted(images.items())}, 'models': {}}
    for zone, models in TARGETS.items():
        for mid, mapping in models.items():
            relative = f'assets/installations/{zone}/models/{mid}.glb'
            path = ROOT / relative
            raw, record = apply(path.read_bytes(), mapping, images)
            path.write_bytes(raw)
            manifest['models'][relative] = record
        inventory = ROOT / f'docs/assets/{zone.upper()}_INVENTORY.csv'
        with inventory.open(newline='', encoding='utf-8') as stream:
            reader = csv.DictReader(stream)
            fields, rows = reader.fieldnames, list(reader)
        report_path = ROOT / f'docs/assets/{zone.upper()}_EXPORT_REPORT.json'
        report = json.loads(report_path.read_text())
        for row in rows:
            if row['model_id'] in models:
                row['sha256'] = manifest['models'][row['glb_file']]['sha256']
                row['provenance'] += '; base-color textures: tools/apply_presentation_textures.py'
                row['evidence_file'] = f"docs/evidence/presentation-textures/after/{zone}/assets/{row['model_id']}.png"
        for model in report['models']:
            if model['model_id'] in models:
                path = ROOT / f"assets/installations/{zone}/models/{model['model_id']}.glb"
                model.update(sha256=sha(path.read_bytes()), bytes=path.stat().st_size)
        with inventory.open('w', newline='', encoding='utf-8') as stream:
            writer = csv.DictWriter(stream, fieldnames=fields, lineterminator='\n')
            writer.writeheader()
            writer.writerows(rows)
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n')
    print('Applied 6 original textures to 14 GLB; preserved binary data and material parameters')


if __name__ == '__main__':
    main()
