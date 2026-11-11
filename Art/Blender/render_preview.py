# Renders preview pictures of a suit built by one of the build scripts (used for checking the model).
# Usage: python render_preview.py <build_script.py> <output_folder>
import importlib.util
import math
import os
import sys

import bpy
from mathutils import Vector

script, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
spec = importlib.util.spec_from_file_location("suit", script)
suit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(suit)
suit.main()

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 48
scene.cycles.use_denoising = True
scene.render.resolution_x = 700
scene.render.resolution_y = 900

world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.32, 0.34, 0.37, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6

floor_mesh = bpy.data.meshes.new("Floor")
floor_mesh.from_pydata([(-5, -5, 0), (5, -5, 0), (5, 5, 0), (-5, 5, 0)], [], [(0, 1, 2, 3)])
floor = bpy.data.objects.new("Floor", floor_mesh)
floor_mat = bpy.data.materials.new("FloorMat")
floor_mat.use_nodes = True
floor_mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.18, 0.18, 0.19, 1)
floor.data.materials.append(floor_mat)
scene.collection.objects.link(floor)


def add_light(name, kind, loc, energy, size=1.0):
    data = bpy.data.lights.new(name, kind)
    data.energy = energy
    if kind == "AREA":
        data.size = size
    obj = bpy.data.objects.new(name, data)
    obj.location = loc
    direction = Vector((0, 0, 1.0)) - Vector(loc)
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(obj)


add_light("Key", "AREA", (-3, -3.5, 4), 900, 2.5)
add_light("Fill", "AREA", (3.5, -2, 2), 300, 3)
add_light("Rim", "AREA", (0, 4, 3.5), 600, 2)

cam_data = bpy.data.cameras.new("Cam")
cam_data.lens = 50
cam = bpy.data.objects.new("Cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam


def shoot(name, angle_deg, pose, distance=6.2, height=1.2, target_z=1.0):
    suit.pose(pose)
    bpy.context.view_layer.update()
    a = math.radians(angle_deg)
    # Angle 0 = straight in front of the suit (the suit faces -Y).
    cam.location = (math.sin(a) * distance, -math.cos(a) * distance, height)
    direction = Vector((0, 0, target_z)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = os.path.join(out_dir, name + ".png")
    bpy.ops.render.render(write_still=True)


views = sys.argv[3].split(",") if len(sys.argv) > 3 else ["ready34:35:ready", "front:0:rest", "side:90:rest", "back:180:rest"]
for view in views:
    name, angle, pose = view.split(":")
    shoot(name, float(angle), pose)
