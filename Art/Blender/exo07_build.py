# USMC EXO-07 - Marine heavy exosuit, built by script for the Anthem-style game.
#
# HOW TO RUN (Blender 4.x or 5.x):
#   1. Open Blender. Close the splash screen.
#   2. Click the "Scripting" tab at the top of the window.
#   3. In the text editor in the middle, click "Open" and pick this file (exo07_build.py).
#   4. Click the Run Script button (the triangle "play" icon), or press Alt+P with the mouse over the text.
#   5. Click the "Layout" tab to go back. The suit appears in the middle of the scene.
#      Press Numpad . (period) with the mouse over the 3D view to zoom to it, and Z > Material Preview to see colors.
#
# Running it again rebuilds the suit from scratch (it deletes only the objects it made).
#
# How it's laid out: the suit is split into the same pieces as the game's suit (pelvis, torso, head, upper arms,
# forearms, thighs, shins, feet, and the HMG-88), each hanging from an empty at its joint, with the same joint
# positions the game uses. Rotate an empty (say "RightShoulder") to pose that limb. Later each piece is exported
# to Unreal and dropped onto the matching joint in the game, so the game's own animation moves it.
#
# All sizes below are in centimeters in the game's axes: X forward, Y to the suit's right, Z up.
# The P() function turns them into Blender's meters and axes (the suit faces -Y in Blender, Unreal's convention
# for import).

import bpy
import bmesh
import math
from mathutils import Matrix, Vector

SUIT = "EXO07"
CM = 0.01

# Pose to leave the suit in when the script finishes: "rest" (arms down, like the game when idle)
# or "ready" (HMG raised and aimed, like the game's combat stance).
POSE = "ready"

# Set to a folder path (for example r"C:\Users\josep\Documents\AnthemGame\ArtSource\Marines\EXO07") to also
# export one FBX file per piece for Unreal. Leave as None to skip.
EXPORT_FOLDER = None


# ------------------------------------------------------------------------------------------------------
# Game space to Blender space
# ------------------------------------------------------------------------------------------------------

# Game (forward, right, up) -> Blender (x, y, z): forward is -Y, the suit's right is -X.
GAME_TO_BLENDER = Matrix(((0.0, -1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0)))


def P(x, y, z):
    """A point in game centimeters, as a Blender position in meters."""
    return GAME_TO_BLENDER @ Vector((x, y, z)) * CM


def game_rotation(pitch=0.0, yaw=0.0, roll=0.0):
    """Unreal's FRotator(pitch, yaw, roll) as a 3x3 matrix in game space (degrees).
    Positive pitch tips +X up toward +Z, positive yaw turns +X toward +Y, positive roll tips +Y up toward +Z."""
    p, y, r = math.radians(pitch), math.radians(yaw), math.radians(roll)
    roll_m = Matrix(((1, 0, 0), (0, math.cos(r), -math.sin(r)), (0, math.sin(r), math.cos(r))))
    pitch_m = Matrix(((math.cos(p), 0, -math.sin(p)), (0, 1, 0), (math.sin(p), 0, math.cos(p))))
    yaw_m = Matrix(((math.cos(y), -math.sin(y), 0), (math.sin(y), math.cos(y), 0), (0, 0, 1)))
    return yaw_m @ pitch_m @ roll_m


def blender_rotation(pitch=0.0, yaw=0.0, roll=0.0):
    """The same rotation, for a Blender object."""
    return GAME_TO_BLENDER @ game_rotation(pitch, yaw, roll) @ GAME_TO_BLENDER.transposed()


# ------------------------------------------------------------------------------------------------------
# Materials
# ------------------------------------------------------------------------------------------------------

def srgb(r, g, b):
    """0-255 sRGB, as Blender's linear color."""
    def lin(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin(r), lin(g), lin(b), 1.0)


def bsdf_input(node, *names):
    for name in names:
        if name in node.inputs:
            return node.inputs[name]
    return None


def make_material(name, color, roughness=0.6, metallic=0.0, emission=None, strength=0.0):
    full = f"{SUIT}_{name}"
    mat = bpy.data.materials.get(full) or bpy.data.materials.new(full)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission:
        bsdf_input(bsdf, "Emission Color", "Emission").default_value = emission
        bsdf_input(bsdf, "Emission Strength").default_value = strength
    mat.diffuse_color = color  # what the Solid view shows
    return mat


def make_camo_material(name, colors, pixel=0.022, scale=9.0):
    """Digital woodland camo: noise read through a coarse grid so it breaks into square pixels.
    Blender-only for now; for Unreal it gets baked to a texture later."""
    full = f"{SUIT}_{name}"
    mat = bpy.data.materials.get(full) or bpy.data.materials.new(full)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    for node in list(nodes):
        if node.type != "OUTPUT_MATERIAL":
            nodes.remove(node)
    out = next(n for n in nodes if n.type == "OUTPUT_MATERIAL")

    geometry = nodes.new("ShaderNodeNewGeometry")
    snap = nodes.new("ShaderNodeVectorMath")
    snap.operation = "SNAP"
    snap.inputs[1].default_value = (pixel, pixel, pixel)
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = scale
    noise.inputs["Detail"].default_value = 1.5
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.interpolation = "CONSTANT"
    stops = [(0.0, colors[0]), (0.43, colors[1]), (0.53, colors[2]), (0.62, colors[3])]
    elements = ramp.color_ramp.elements
    while len(elements) < len(stops):
        elements.new(0.5)
    for element, (position, color) in zip(elements, stops):
        element.position = position
        element.color = color
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.72

    links.new(geometry.outputs["Position"], snap.inputs[0])
    links.new(snap.outputs["Vector"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    mat.diffuse_color = colors[1]
    return mat


M = {}


def build_materials():
    M["camo"] = make_camo_material("Camo", [srgb(74, 58, 40), srgb(96, 98, 64), srgb(50, 66, 38), srgb(28, 29, 25)])
    M["olive"] = make_material("OliveDrab", srgb(88, 90, 60), roughness=0.7)
    M["gunmetal"] = make_material("Gunmetal", srgb(48, 48, 48), roughness=0.45, metallic=0.8)
    M["worn"] = make_material("WornMetal", srgb(132, 130, 120), roughness=0.35, metallic=1.0)
    M["rubber"] = make_material("Rubber", srgb(22, 22, 22), roughness=0.85)
    M["silver"] = make_material("Insignia", srgb(170, 166, 152), roughness=0.3, metallic=1.0)
    M["visor"] = make_material("Visor", srgb(26, 90, 255), roughness=0.2, emission=srgb(40, 110, 255), strength=12.0)
    M["thruster"] = make_material("ThrusterGlow", srgb(60, 180, 255), roughness=0.3, emission=srgb(60, 180, 255), strength=6.0)
    M["flag_red"] = make_material("FlagRed", srgb(160, 30, 38), roughness=0.8)
    M["flag_white"] = make_material("FlagWhite", srgb(225, 220, 208), roughness=0.8)
    M["flag_blue"] = make_material("FlagBlue", srgb(30, 40, 95), roughness=0.8)


# ------------------------------------------------------------------------------------------------------
# Shapes. Every shape is placed relative to its joint, in game centimeters.
# ------------------------------------------------------------------------------------------------------

COLLECTION = None
JOINTS = {}


def link(obj):
    COLLECTION.objects.link(obj)


def finish(name, bm, joint, material, bevel):
    mesh = bpy.data.meshes.new(f"{SUIT}_{name}")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(f"{SUIT}_{name}", mesh)
    obj.data.materials.append(M[material])
    link(obj)
    pivot = JOINTS[joint]
    obj.parent = pivot
    obj.matrix_parent_inverse = Matrix.Identity(4)
    if bevel > 0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel * CM
        mod.segments = 2
        mod.limit_method = "ANGLE"
        mod.angle_limit = math.radians(40)
    obj["game_joint"] = joint
    return obj


def place(bm, center, rotation):
    """Rotates the shape about its center (game space) and moves it to its spot relative to the joint."""
    rot = game_rotation(*rotation)
    c = Vector(center)
    for v in bm.verts:
        v.co = GAME_TO_BLENDER @ (rot @ v.co + c) * CM


def box(name, joint, center, size, material, rotation=(0, 0, 0), taper=(1.0, 1.0), bevel=1.2):
    """A box size = (depth front-to-back, width, height) cm. taper scales the TOP face (depth, width)."""
    bm = bmesh.new()
    dx, dy, dz = size[0] / 2, size[1] / 2, size[2] / 2
    corners = []
    for z in (-dz, dz):
        tx, ty = (taper if z > 0 else (1.0, 1.0))
        for x, y in ((-dx, -dy), (dx, -dy), (dx, dy), (-dx, dy)):
            corners.append(bm.verts.new((x * tx, y * ty, z)))
    b, t = corners[:4], corners[4:]
    bm.faces.new(list(reversed(b)))
    bm.faces.new(t)
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((b[i], b[j], t[j], t[i]))
    bm.normal_update()
    place(bm, center, rotation)
    return finish(name, bm, joint, material, bevel)


def cylinder(name, joint, center, radius, length, material, rotation=(0, 0, 0), top_radius=None, segments=20, bevel=0.6):
    """A cylinder along Z (before rotation). top_radius makes it a cone or tapered barrel."""
    bm = bmesh.new()
    top = radius if top_radius is None else top_radius
    bottom_ring, top_ring = [], []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        bottom_ring.append(bm.verts.new((math.cos(a) * radius, math.sin(a) * radius, -length / 2)))
        top_ring.append(bm.verts.new((math.cos(a) * top, math.sin(a) * top, length / 2)))
    bm.faces.new(list(reversed(bottom_ring)))
    if top > 0.05:
        bm.faces.new(top_ring)
    for i in range(segments):
        j = (i + 1) % segments
        face = bm.faces.new((bottom_ring[i], bottom_ring[j], top_ring[j], top_ring[i]))
        face.smooth = True
    bm.normal_update()
    place(bm, center, rotation)
    return finish(name, bm, joint, material, bevel)


def ellipsoid(name, joint, center, radii, material, rotation=(0, 0, 0)):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=14, radius=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * radii[0], v.co.y * radii[1], v.co.z * radii[2]))
    for f in bm.faces:
        f.smooth = True
    place(bm, center, rotation)
    return finish(name, bm, joint, material, 0)


# Along X (pointing forward) and along Y (pointing sideways), for cylinders.
FORWARD = (90, 0, 0)
SIDEWAYS = (0, 0, 90)


# ------------------------------------------------------------------------------------------------------
# Joints, matching the game's suit frame
# ------------------------------------------------------------------------------------------------------

def joint(name, parent, offset):
    empty = bpy.data.objects.new(f"{SUIT}_{name}", None)
    empty.empty_display_type = "PLAIN_AXES"
    empty.empty_display_size = 0.08
    link(empty)
    if parent:
        empty.parent = JOINTS[parent]
        empty.matrix_parent_inverse = Matrix.Identity(4)
    empty.location = P(*offset)
    JOINTS[name] = empty
    return empty


def build_joints():
    # The capsule center is the origin; feet are 96 cm below it, so lift the whole suit to stand on the floor.
    root = joint("VisualRoot", None, (0, 0, 0))
    root.location = P(0, 0, 96)
    joint("TorsoPivot", "VisualRoot", (0, 0, 2))
    joint("HeadPivot", "TorsoPivot", (4, 0, 70))
    for side, s in (("Left", -1), ("Right", 1)):
        joint(f"{side}Shoulder", "TorsoPivot", (0, 46 * s, 56))
        joint(f"{side}Elbow", f"{side}Shoulder", (0, 0, -40))
        joint(f"{side}Hip", "VisualRoot", (0, 19 * s, -14))
        joint(f"{side}Knee", f"{side}Hip", (0, 0, -40))
        joint(f"{side}Ankle", f"{side}Knee", (0, 0, -34))


# ------------------------------------------------------------------------------------------------------
# The suit
# ------------------------------------------------------------------------------------------------------

def build_pelvis():
    j = "VisualRoot"
    box("PelvisCore", j, (0, 0, -10), (30, 44, 20), "gunmetal")
    box("Belt", j, (0, 0, 0), (40, 56, 8), "gunmetal", bevel=1.5)
    box("Codpiece", j, (15, 0, -15), (9, 22, 20), "camo", rotation=(8, 0, 0), taper=(1.0, 1.3))
    box("RearPlate", j, (-18, 0, -12), (6, 36, 18), "camo", rotation=(-8, 0, 0))
    for side, s in (("L", -1), ("R", 1)):
        box(f"BeltPouch{side}", j, (16, 17 * s, -4), (8, 12, 12), "olive", bevel=1.5)
        box(f"Tasset{side}", j, (3, 28 * s, -15), (24, 4, 22), "camo", rotation=(0, 0, -12 * s))


def build_torso():
    j = "TorsoPivot"
    box("Abdomen", j, (0, 0, 12), (30, 40, 24), "gunmetal")
    for i, z in enumerate((5, 12, 19)):
        box(f"AbPlate{i}", j, (16, 0, z), (6, 30 - i * 2, 6), "olive", bevel=1.0)
    box("ChestCore", j, (2, 0, 42), (48, 76, 44), "camo", taper=(1.0, 1.06), bevel=2.0)
    box("ChestArmor", j, (27, 0, 45), (12, 70, 40), "camo", rotation=(-6, 0, 0), taper=(0.9, 0.86), bevel=2.0)
    box("ChestArmorRim", j, (25, 0, 24), (10, 64, 4), "worn", rotation=(-6, 0, 0))
    for side, s in (("L", -1), ("R", 1)):
        box(f"SideRib{side}", j, (0, 39 * s, 36), (30, 6, 30), "gunmetal")
        box(f"CollarBone{side}", j, (10, 22 * s, 66), (22, 20, 8), "olive", rotation=(0, 0, 10 * s))
    box("NeckGuard", j, (-4, 0, 72), (36, 50, 12), "camo", taper=(0.8, 0.7), bevel=2.0)

    # Eagle, globe and anchor on the chest.
    cylinder("EmblemDisc", j, (33.5, 0, 46), 8.5, 2, "silver", rotation=(84, 0, 0), segments=28)
    ellipsoid("EmblemGlobe", j, (35, 0, 46), (2.5, 4.5, 4.5), "worn")
    box("EmblemEagle", j, (35, 0, 52.5), (2, 16, 3), "silver", rotation=(-6, 0, 0), taper=(1, 0.5), bevel=0.4)
    box("EmblemAnchor", j, (35, 0, 39.5), (2, 3, 10), "silver", rotation=(-6, 0, 0), bevel=0.4)

    # Jet pack frame and nozzles, then the assault pack on top.
    box("JetPack", j, (-30, 0, 44), (24, 62, 50), "gunmetal")
    for side, s in (("L", -1), ("R", 1)):
        cylinder(f"Nozzle{side}", j, (-36, 20 * s, 16), 8.5, 18, "gunmetal", top_radius=7.0)
        cylinder(f"NozzleGlow{side}", j, (-36, 20 * s, 6.5), 6.5, 1.0, "thruster", bevel=0)
        box(f"PackPouch{side}", j, (-52, 37 * s, 42), (18, 10, 30), "olive", bevel=2.0)
        box(f"PackStrap{side}", j, (-14, 22 * s, 58), (34, 6, 4), "rubber", bevel=0.5)
    box("AssaultPack", j, (-54, 0, 50), (22, 66, 50), "camo", bevel=2.5)
    box("PackLid", j, (-54, 0, 77), (24, 62, 6), "olive", bevel=1.5)
    box("PackBackPouch", j, (-66, 0, 44), (4, 40, 28), "olive", bevel=1.0)
    for i in range(3):
        box(f"PackLight{i}", j, (-68.3, 8 + i * 5, 60), (1, 3, 2), "visor", bevel=0)
    cylinder("Antenna", j, (-58, 24, 100), 0.8, 50, "gunmetal", segments=8, bevel=0)
    ellipsoid("AntennaTip", j, (-58, 24, 125), (1.4, 1.4, 1.4), "gunmetal")


def build_head():
    j = "HeadPivot"
    cylinder("Neck", j, (0, 0, 2), 9, 10, "rubber")
    ellipsoid("Dome", j, (0, 0, 18), (17, 15, 17), "camo")
    box("Brow", j, (12, 0, 27), (16, 32, 7), "olive", rotation=(-12, 0, 0), taper=(0.9, 0.85), bevel=1.5)
    box("VisorRecess", j, (17.5, 0, 18), (3, 26, 8), "rubber", bevel=0.5)
    box("Faceplate", j, (14, 0, 11), (10, 26, 12), "gunmetal", taper=(1.0, 1.1), bevel=1.5)
    box("Jaw", j, (15, 0, 3), (12, 22, 8), "olive", taper=(1.0, 1.25), bevel=1.5)
    for i, y in enumerate((-4, 0, 4)):
        box(f"JawVent{i}", j, (21.2, y, 4), (1, 2, 5), "rubber", bevel=0)
    for side, s in (("L", -1), ("R", 1)):
        box(f"Cheek{side}", j, (8, 15 * s, 12), (18, 5, 16), "camo", rotation=(0, 6 * s, 0), bevel=1.0)
        # Angled eye slits: the inner ends sit lower, for the scowl.
        box(f"Eye{side}", j, (19.2, 6 * s, 18), (2, 10, 3.5), "visor", rotation=(0, 0, 12 * s), bevel=0.3)
        cylinder(f"EarModule{side}", j, (0, 16 * s, 17), 5, 4, "gunmetal", rotation=SIDEWAYS)
        box(f"HornBase{side}", j, (-1, 9 * s, 32), (10, 6, 6), "gunmetal")
        cylinder(f"Horn{side}", j, (-3, 10 * s, 45), 4.0, 26, "worn", rotation=(15, 0, -10 * s), top_radius=0.4, segments=12)


def build_arm(side, s):
    sh, el = f"{side}Shoulder", f"{side}Elbow"
    ellipsoid(f"{side}ShoulderBall", sh, (0, 6 * s, 4), (18, 16, 15), "gunmetal")
    box(f"{side}PauldronShell", sh, (0, 10 * s, 10), (50, 30, 36), "camo", taper=(0.9, 0.85), bevel=2.5)
    box(f"{side}PauldronRim", sh, (0, 12 * s, 29), (46, 26, 4), "worn", bevel=1.0)
    box(f"{side}PauldronLip", sh, (0, 23 * s, -7), (44, 4, 14), "olive", rotation=(0, 0, -10 * s))
    cylinder(f"{side}UpperArm", sh, (0, 0, -22), 8.5, 34, "gunmetal")
    box(f"{side}BicepArmor", sh, (3, 2 * s, -21), (20, 20, 20), "camo", bevel=1.5)
    ellipsoid(f"{side}ElbowPad", el, (-6, 0, -1), (8, 8, 8), "olive")
    cylinder(f"{side}ElbowJoint", el, (0, 0, 0), 7.5, 20, "rubber", rotation=SIDEWAYS)
    box(f"{side}ForearmGuard", el, (4, 0, -18), (30, 28, 30), "camo", taper=(1.06, 1.06), bevel=2.0)
    box(f"{side}ForearmPlate", el, (19.5, 0, -18), (2, 18, 22), "olive", bevel=0.8)
    cylinder(f"{side}Wrist", el, (2, 0, -35), 9, 4, "gunmetal")
    box(f"{side}Hand", el, (3, 0, -43), (15, 12, 14), "gunmetal")
    box(f"{side}Fingers", el, (8, 0, -52), (7, 11, 9), "gunmetal", rotation=(-20, 0, 0), bevel=0.8)
    box(f"{side}Thumb", el, (9, -5 * s, -44), (5, 4, 8), "gunmetal", bevel=0.6)


def build_flag():
    # U.S. flag patch on the outside of the left shoulder shell, canton toward the front.
    j, y = "LeftShoulder", -25.4
    box("FlagField", j, (2, y, 10), (22, 1, 14), "flag_white", bevel=0)
    for i in range(4):
        box(f"FlagStripe{i}", j, (2, y - 0.15, 16 - i * 4), (22, 1, 2), "flag_red", bevel=0)
    box("FlagCanton", j, (8, y - 0.3, 13.5), (10, 1, 7), "flag_blue", bevel=0)


def build_leg(side, s):
    hip, knee, ankle = f"{side}Hip", f"{side}Knee", f"{side}Ankle"
    box(f"{side}ThighCore", hip, (0, 0, -20), (26, 24, 40), "gunmetal")
    box(f"{side}ThighArmor", hip, (4, 0, -20), (32, 30, 34), "camo", taper=(1.06, 1.06), bevel=2.0)
    box(f"{side}ThighSidePlate", hip, (1, 16 * s, -16), (22, 4, 26), "olive")
    cylinder(f"{side}KneeJoint", knee, (0, 0, 0), 8, 22, "rubber", rotation=SIDEWAYS)
    box(f"{side}KneeGuard", knee, (14, 0, 1), (12, 22, 20), "olive", rotation=(-10, 0, 0), taper=(1, 0.85), bevel=2.0)
    box(f"{side}ShinArmor", knee, (5, 0, -19), (30, 28, 28), "camo", taper=(1.05, 1.1), bevel=2.0)
    box(f"{side}Calf", knee, (-12, 0, -18), (10, 22, 24), "gunmetal")
    cylinder(f"{side}CalfJet", knee, (-17, 0, -26), 5, 12, "gunmetal", top_radius=6)
    box(f"{side}Boot", ankle, (8, 0, -1), (42, 26, 14), "gunmetal", bevel=2.0)
    box(f"{side}ToeCap", ankle, (25, 0, -2), (10, 24, 10), "olive", bevel=2.0)
    box(f"{side}Heel", ankle, (-11, 0, -2), (8, 22, 12), "gunmetal")
    box(f"{side}Sole", ankle, (8, 0, -7), (44, 27, 2), "rubber", bevel=0.5)


def build_hmg():
    # HMG-88 in the right hand. In the elbow's space -Z runs down the forearm and out along the barrels.
    j = "RightElbow"
    box("HmgReceiver", j, (4, 0, -64), (24, 16, 56), "gunmetal", bevel=1.5)
    box("HmgCover", j, (17, 0, -60), (4, 14, 40), "worn", bevel=0.8)
    box("HmgHandle", j, (24, 0, -64), (4, 4, 18), "gunmetal", bevel=0.8)
    for i, z in enumerate((-56, -72)):
        box(f"HmgHandlePost{i}", j, (20.5, 0, z), (6, 3, 3), "gunmetal", bevel=0.5)
    box("HmgStock", j, (2, 0, -32), (16, 10, 12), "gunmetal", bevel=1.5)
    cylinder("HmgDrum", j, (-14, -2, -58), 14, 16, "olive", rotation=SIDEWAYS, segments=28, bevel=1.0)
    cylinder("HmgDrumCap", j, (-14, -10.5, -58), 10, 1.2, "worn", rotation=SIDEWAYS, segments=28, bevel=0.3)
    box("HmgFeed", j, (-4, 0, -62), (6, 10, 10), "gunmetal")
    box("HmgFrontGrip", j, (-10, 0, -92), (10, 6, 14), "gunmetal", rotation=(-10, 0, 0))
    cylinder("HmgShroud", j, (5, 0, -104), 7.5, 30, "gunmetal", segments=24)
    for i in range(3):
        cylinder(f"HmgShroudBand{i}", j, (5, 0, -96 - i * 8), 8.0, 1.2, "worn", segments=24, bevel=0.2)
    for i in range(3):
        a = 2 * math.pi * i / 3
        cylinder(f"HmgBarrel{i}", j, (5 + 3.2 * math.cos(a), 3.2 * math.sin(a), -129), 1.75, 24, "gunmetal", segments=12, bevel=0)
    cylinder("HmgMuzzleBrake", j, (5, 0, -142), 5, 4, "gunmetal", segments=20)


def build():
    build_pelvis()
    build_torso()
    build_head()
    for side, s in (("Left", -1), ("Right", 1)):
        build_arm(side, s)
        build_leg(side, s)
    build_flag()
    build_hmg()


# ------------------------------------------------------------------------------------------------------
# Poses (same angles as the game's procedural animation)
# ------------------------------------------------------------------------------------------------------

def set_joint(name, pitch=0.0, yaw=0.0, roll=0.0):
    obj = JOINTS[name]
    loc = obj.location.copy()
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = blender_rotation(pitch, yaw, roll).to_quaternion()
    obj.location = loc


def pose(kind):
    for name in JOINTS:
        if name != "VisualRoot":
            set_joint(name)
    if kind == "ready":
        # Combat stance: gun arm aimed straight ahead, left hand up on the front grip, feet planted wide.
        set_joint("RightShoulder", 90, 0, -4)
        set_joint("RightElbow", 2)
        set_joint("LeftShoulder", 60, 0, 4)
        set_joint("LeftElbow", 45)
        set_joint("TorsoPivot", -4, 8, 0)
        set_joint("HeadPivot", 4, -8, 0)
        set_joint("LeftHip", 12, 0, 6)
        set_joint("LeftKnee", -16)
        set_joint("LeftAnkle", 4)
        set_joint("RightHip", -6, 0, -6)
        set_joint("RightKnee", -10)
        set_joint("RightAnkle", 16)
    else:
        # Idle: arms hanging a little out from the body.
        set_joint("LeftShoulder", 4, 0, 9)
        set_joint("RightShoulder", 4, 0, -9)
        set_joint("LeftElbow", 18)
        set_joint("RightElbow", 18)


# ------------------------------------------------------------------------------------------------------
# Export: one FBX per joint, so each piece can be hung on the matching joint in the game
# ------------------------------------------------------------------------------------------------------

def export_pieces(folder):
    import os
    os.makedirs(folder, exist_ok=True)
    pose("rest")
    bpy.context.view_layer.update()
    for name, pivot in JOINTS.items():
        parts = [o for o in COLLECTION.objects if o.type == "MESH" and o.parent == pivot]
        if not parts:
            continue
        bpy.ops.object.select_all(action="DESELECT")
        for o in parts:
            o.select_set(True)
        # The piece's origin is its joint, so it lines up when placed on the game's pivot.
        saved = pivot.matrix_world.copy()
        pivot.matrix_world = Matrix.Identity(4)
        bpy.context.view_layer.update()
        bpy.ops.export_scene.fbx(filepath=os.path.join(folder, f"SM_{SUIT}_{name}.fbx"), use_selection=True,
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", use_mesh_modifiers=True,
                                 object_types={"MESH"}, axis_forward="-Y", axis_up="Z")
        pivot.matrix_world = saved
    pose(POSE)


# ------------------------------------------------------------------------------------------------------

def main():
    global COLLECTION
    # Remove what an earlier run made, and nothing else.
    old = bpy.data.collections.get(SUIT)
    if old:
        for obj in list(old.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.collections.remove(old)
    COLLECTION = bpy.data.collections.new(SUIT)
    bpy.context.scene.collection.children.link(COLLECTION)
    JOINTS.clear()

    build_materials()
    build_joints()
    build()
    pose(POSE)
    if EXPORT_FOLDER:
        export_pieces(EXPORT_FOLDER)
    print(f"{SUIT}: built {sum(1 for o in COLLECTION.objects if o.type == 'MESH')} parts")


if __name__ == "__main__":
    main()
