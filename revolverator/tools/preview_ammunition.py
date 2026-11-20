# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Blender background preview of the generated GLB (not a concept mockup)."""
import math
from pathlib import Path
import sys
import bpy
from mathutils import Matrix, Vector

source, output = sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(Path(source).resolve()))
# OpenRAC GLBs retain the game's Z-up axes. Undo the standard glTF Y-up
# conversion that Blender's importer applies, for an upright native preview.
for root in [o for o in bpy.context.scene.objects if o.parent is None]:
    root.matrix_world = Matrix.Rotation(-math.pi/2, 4, 'X')@root.matrix_world
objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
points = [o.matrix_world@v.co for o in objects for v in o.data.vertices]
low = Vector([min(p[i] for p in points) for i in range(3)])
high = Vector([max(p[i] for p in points) for i in range(3)])
centre, extent = (low+high)*.5, max(high-low)
bpy.ops.object.camera_add(location=centre+Vector((1.8,-2.8,1.9))*extent)
camera = bpy.context.object
camera.rotation_euler = (centre-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = extent*1.5
bpy.context.scene.camera = camera
for vector, energy in [((0,-2,3),250), ((2,1,1.5),125)]:
    bpy.ops.object.light_add(type='AREA', location=centre+Vector(vector)*extent)
    light = bpy.context.object
    light.data.energy = energy*extent*extent
    light.data.size = extent*2
    light.rotation_euler = (centre-light.location).to_track_quat('-Z','Y').to_euler()
scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 900
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.world = bpy.data.worlds.new('World')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.075,.09,.12,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = .5
scene.view_settings.view_transform = 'Standard'
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(Path(output).resolve())
bpy.ops.render.render(write_still=True)
