"""Exercise regeneration and failures in a temp tree without requiring Git for CTest."""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('textures', ROOT / 'tools/apply_presentation_textures.py')
textures = importlib.util.module_from_spec(spec)
spec.loader.exec_module(textures)


class RegenerationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        manifest_path = 'assets/common/textures/presentation/manifest.json'
        manifest = json.loads((ROOT / manifest_path).read_text())
        paths = [manifest_path, *manifest['models'],
                 *['assets/common/textures/presentation/' + k + '.png' for k in manifest['textures']],
                 *[f'docs/assets/{z}_{suffix}' for z in ('AQUABIO', 'ECOENERGY', 'ECOCYCLE')
                   for suffix in ('INVENTORY.csv', 'EXPORT_REPORT.json')]]
        for path in paths:
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / path).read_bytes())
        self.originals = {}
        for path, record in manifest['models'].items():
            doc, binary = textures.unpack((ROOT / path).read_bytes())
            for material in doc['materials']:
                if material['name'] in record['materials']:
                    material['pbrMetallicRoughness'].pop('baseColorTexture')
            for key, count in record['original_array_counts'].items():
                if count is None:
                    doc.pop(key, None)
                else:
                    doc[key] = doc[key][:count]
            doc['buffers'][0]['byteLength'] = record['original_buffer_length']
            encoded = textures.canonical(doc)
            encoded += b' ' * (-len(encoded) % 4)
            binary = binary[:record['original_bin_bytes']]
            original = (struct.pack('<5I', 0x46546C67, 2, 28+len(encoded)+len(binary), len(encoded), 0x4E4F534A)
                        + encoded + struct.pack('<2I', len(binary), 0x004E4942) + binary)
            self.originals[path] = original
            # Synthetic fixture differs only in JSON whitespace; official hashes stay untouched.
            record['original_sha256'] = textures.sha(original)
        (self.root / manifest_path).write_text(json.dumps(manifest))
        self.addCleanup(patch.stopall)
        patch.object(textures, 'ROOT', self.root).start()
        patch.object(textures, 'TEXTURES', self.root / 'assets/common/textures/presentation').start()
        patch.object(textures, 'MANIFEST', self.root / manifest_path).start()

    def hashes(self):
        return {str(p.relative_to(self.root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in self.root.rglob('*') if p.is_file()}

    def run_tool(self, option):
        def git(args, **kwargs):
            if args == ['git', 'rev-parse', 'origin/main']:
                return textures.SOURCE_COMMIT + '\n'
            self.assertEqual(args[:2], ['git', 'show'])
            return self.originals[args[2].split(':', 1)[1]]
        with patch.object(sys, 'argv', ['textures', option]), \
                patch.object(textures.subprocess, 'check_output', side_effect=git), \
                contextlib.redirect_stdout(io.StringIO()):
            textures.main()

    def test_repeat_and_check_preserve_bytes_without_appending(self):
        self.run_tool('--regenerate')
        expected = self.hashes()
        self.run_tool('--regenerate')
        self.assertEqual(expected, self.hashes())
        self.run_tool('--check')
        self.assertEqual(expected, self.hashes())
        for path in self.originals:
            doc, _ = textures.unpack((self.root / path).read_bytes())
            self.assertLessEqual(len(doc.get('images', [])), 3)

    def test_wrong_original_rejected_before_any_write(self):
        self.originals[next(iter(self.originals))] += b'corruption'
        expected = self.hashes()
        with self.assertRaisesRegex(AssertionError, 'Original GLB hash mismatch'):
            self.run_tool('--regenerate')
        self.assertEqual(expected, self.hashes())

    def test_corrupt_existing_texture_rejected_before_any_write(self):
        (textures.TEXTURES / 'water.png').write_bytes(b'corruption')
        expected = self.hashes()
        with self.assertRaises(AssertionError):
            self.run_tool('--regenerate')
        self.assertEqual(expected, self.hashes())

    def test_metal_only_change_preserves_other_resources_and_materials(self):
        previous_images = {p.stem: p.read_bytes() for p in textures.TEXTURES.glob('*.png')}
        previous_images['metal'] = (ROOT / 'docs/evidence/presentation-metal-finish/metal-before.png').read_bytes()
        manifest = {'source_commit': textures.SOURCE_COMMIT,
                    'textures': {k: textures.sha(v) for k, v in sorted(previous_images.items())}, 'models': {}}
        for zone, models in textures.TARGETS.items():
            for mid, mapping in models.items():
                path = f'assets/installations/{zone}/models/{mid}.glb'
                raw, record = textures.apply(self.originals[path], mapping, previous_images)
                (self.root / path).write_bytes(raw)
                manifest['models'][path] = record
        (textures.TEXTURES / 'metal.png').write_bytes(previous_images['metal'])
        textures.MANIFEST.write_text(json.dumps(manifest))
        before = self.hashes()
        self.run_tool('--regenerate')
        after = self.hashes()
        for path, record in manifest['models'].items():
            if 'metal' not in record['materials'].values():
                self.assertEqual(before[path], after[path])
            doc, _ = textures.unpack((self.root / path).read_bytes())
            original, _ = textures.unpack(self.originals[path])
            for actual, expected in zip(doc['materials'], original['materials']):
                if actual['name'] in record['materials']:
                    actual['pbrMetallicRoughness'].pop('baseColorTexture', None)
                self.assertEqual(expected, actual)
        for kind in ('water', 'wood', 'concrete', 'painted', 'belt'):
            key = f'assets/common/textures/presentation/{kind}.png'
            self.assertEqual(before[key], after[key])


if __name__ == '__main__':
    unittest.main()
