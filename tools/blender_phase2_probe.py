"""Ejecutar en Blender (Scripting) desde este checkout; no guarda ni reemplaza .blend.
También: blender --background --python tools/blender_phase2_probe.py -- /ruta/nueva.glb
"""
import bpy
import math
import sys
from pathlib import Path

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
destination = Path(args[0] if args else Path(__file__).resolve().parents[1] / 'assets/common/models/blender_probe_unlit.glb')
if destination.exists():
    raise RuntimeError(f'No se reemplaza un recurso existente: {destination}')
destination.parent.mkdir(parents=True, exist_ok=True)
original_scene = bpy.context.window.scene
scene = bpy.data.scenes.new('EcoSphere_Phase2_Technical_Probe')
try:
    bpy.context.window.scene = scene
    root = bpy.data.objects.new('Probe_Parent', None)
    scene.collection.objects.link(root)
    root.location = (0.2, 0, 0.15)
    root.rotation_mode = 'QUATERNION'
    from mathutils import Quaternion
    root.rotation_quaternion = Quaternion((0, 0, 1), math.radians(20))
    image = bpy.data.images.new('Probe_Checker_2x2', width=2, height=2, alpha=False)
    # Esquinas distintas para comprobar orientación UV, sin imágenes de terceros.
    image.pixels = [1,0,1,1, 0,1,1,1, 1,1,1,1, 0.05,0.05,0.05,1]
    image.pack()
    material = bpy.data.materials.new('Probe_Unlit_Checker')
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    emission = tree.nodes.new('ShaderNodeBackground')
    texture = tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    texture.interpolation = 'Closest'
    tree.links.new(texture.outputs['Color'], emission.inputs['Color'])
    tree.links.new(emission.outputs[0], output.inputs['Surface'])
    bpy.ops.mesh.primitive_cube_add(size=1.4, location=(0, 0, 0))
    cube = bpy.context.object
    cube.name = 'Probe_Textured_Cube'
    cube.parent = root
    cube.data.materials.append(material)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, radius=0.5, location=(1.5, 0, 0.6))
    sphere = bpy.context.object
    sphere.name = 'Probe_Child_Sphere'
    sphere.parent = cube
    sphere.scale = (0.7, 0.7, 1.2)
    sphere.data.materials.append(material)
    instance = bpy.data.objects.new('Probe_Shared_Cube', cube.data)
    scene.collection.objects.link(instance)
    instance.parent = root
    instance.location = (-1.25, 0, 0.7)
    instance.scale = (0.4, 0.4, 0.4)
    result = bpy.ops.export_scene.gltf(filepath=str(destination), export_format='GLB',
        use_active_scene=True, export_animations=False, export_cameras=False, export_lights=False,
        export_yup=True, export_texcoords=True, export_normals=True, export_materials='EXPORT')
    if 'FINISHED' not in result:
        raise RuntimeError(f'Export failed: {result}')
    print(f'BLENDER PROBE PASS: Blender {bpy.app.version_string}; {destination}; {destination.stat().st_size} bytes; '
          f'scene={scene.name}; parent/cube/sphere/shared cube; UV + packed image; original scene restored')
finally:
    bpy.context.window.scene = original_scene
