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
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
TEXTURES = ROOT / 'assets/common/textures/presentation'
MANIFEST = TEXTURES / 'manifest.json'
SIZE = 128
SOURCE_COMMIT = '0732ad343434f2983f2d98a2454dc516b09488b2'
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
                wave = math.sin(2*b + .8*math.sin(a) + .25*math.sin(3*a-b))
                value = (224 + 13*math.sin(a+2*b) + 8*math.sin(3*a-b)
                         + 24*math.exp(-((wave-.4)/.16)**2)
                         - 28*math.exp(-((wave+.7)/.24)**2))
            elif kind == 'wood':
                flow = 32*a + .65*math.sin(b) + .25*math.sin(2*b+a)
                value = (231 + 9*math.sin(flow) + 7*math.sin(2*a+b)
                         - 65*max(0, math.sin(flow))**4
                         - 18*max(0, math.sin(2*flow+.4*math.cos(b)))**14)
            elif kind == 'concrete':
                value = (237 + 9*math.sin(a)*math.cos(2*b) + 6*math.sin(3*a+2*b)
                         + 3*math.cos(5*a-b)
                         - 7*max(0, math.sin(7*a+2*b)*math.sin(8*b-a))**4)
                if x < 2 or y < 2:
                    value -= 14
            elif kind == 'metal':
                value = (210 + 20*math.sin(4*a+.2*math.sin(b)) + 10*math.sin(3*b+.2*math.sin(a))
                         + 12*math.sin(24*a+2*math.sin(b)) + 6*math.sin(32*b+.4*math.sin(a)))
            elif kind == 'painted':
                value = (237 + 9*math.sin(2*a+.3*math.sin(b))*math.cos(3*b)
                         + 5*math.cos(5*a-b) - 12*max(0, math.cos(8*b+.2*math.sin(a)))**6)
            elif kind == 'belt':
                value = 244 + 5*math.cos(2*a) - 142*max(0, math.cos(6*b))**2 - 8*max(0, math.cos(12*b))**12
            else:
                raise ValueError(kind)
            code = max(0, min(255, round(value)))
            pixels.extend((max(0, code-3), code, min(255, code+3), 255) if kind == 'water' else (code, code, code, 255))
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
    assert sys.argv[1:] in ([], ['--check'], ['--regenerate']), 'Usage: apply_presentation_textures.py [--check|--regenerate]'
    regenerate = '--regenerate' in sys.argv
    images = {kind: png(kind) for models in TARGETS.values() for mapping in models.values() for kind in mapping.values()}
    previous = json.loads(MANIFEST.read_text())
    expected_paths = {f'assets/installations/{z}/models/{mid}.glb': mapping
                      for z, models in TARGETS.items() for mid, mapping in models.items()}
    assert {p: r['materials'] for p, r in previous['models'].items()} == expected_paths
    existing_images = {k: (TEXTURES / (k + '.png')).read_bytes() for k in images}
    assert previous['textures'] == {k: sha(v) for k, v in sorted(existing_images.items())}
    for path, record in previous['models'].items():
        raw = (ROOT / path).read_bytes()
        assert sha(raw) == record['sha256']
        verify(raw, record, existing_images)
    if not regenerate:
        assert existing_images == images, 'Run --regenerate after reviewing generator changes'
        print('PASS: 6 original textures, 14 GLB; geometry/UV/nodes/original materials/images preserved')
        return
    assert subprocess.check_output(['git', 'rev-parse', 'origin/main'], cwd=ROOT, text=True).strip() == SOURCE_COMMIT, 'Review changed main before regenerating'
    originals = {path: subprocess.check_output(['git', 'show', SOURCE_COMMIT + ':' + path], cwd=ROOT)
                 for path in expected_paths}
    # Verify every input before writing any asset; never append to the current textured GLB.
    assert all(sha(raw) == previous['models'][path]['original_sha256'] for path, raw in originals.items()), 'Original GLB hash mismatch'
    manifest = {'source_commit': SOURCE_COMMIT, 'textures': {k: sha(v) for k, v in sorted(images.items())}, 'models': {}}
    generated = {}
    for zone, models in TARGETS.items():
        for mid, mapping in models.items():
            relative = f'assets/installations/{zone}/models/{mid}.glb'
            raw, record = apply(originals[relative], mapping, images)
            generated[relative] = raw
            manifest['models'][relative] = record
    for kind, raw in images.items():
        (TEXTURES / (kind + '.png')).write_bytes(raw)
    for path, raw in generated.items():
        (ROOT / path).write_bytes(raw)
    for zone, models in TARGETS.items():
        inventory = ROOT / f'docs/assets/{zone.upper()}_INVENTORY.csv'
        with inventory.open(newline='', encoding='utf-8') as stream:
            reader = csv.DictReader(stream)
            fields, rows = reader.fieldnames, list(reader)
        report_path = ROOT / f'docs/assets/{zone.upper()}_EXPORT_REPORT.json'
        report = json.loads(report_path.read_text())
        for row in rows:
            if row['model_id'] in models and manifest['models'][row['glb_file']]['sha256'] != previous['models'][row['glb_file']]['sha256']:
                row['sha256'] = manifest['models'][row['glb_file']]['sha256']
                if '; metal finish' not in row['provenance']:
                    row['provenance'] += '; metal finish'
                row['evidence_file'] = f"docs/evidence/presentation-metal-finish/C-improved/assets/{row['model_id']}.png"
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
