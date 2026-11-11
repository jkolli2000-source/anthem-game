# USMC EXO-07 - Marine heavy exosuit, built by script for the Anthem-style game. (v2: closer to the character sheet)
#
# HOW TO RUN (Blender 4.x or 5.x):
#   1. Open Blender. Close the splash screen.
#   2. Click the "Scripting" tab at the top of the window.
#   3. In the text editor in the middle, click "Open" and pick this file (exo07_build.py).
#   4. Click the Run Script button (the triangle "play" icon), or press Alt+P with the mouse over the text.
#   5. Click the "Layout" tab to go back. The suit appears in the middle of the scene.
#      Delete Blender's default cube if it's there (click it, press X, Delete): it hides the legs.
#      Press Home with the mouse over the 3D view to fit the suit on screen, and Z > Material Preview to see colors.
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


def mix_rgb(nodes, links, blend, fac, a, b):
    """Color mix node (blend MIX or MULTIPLY). a and b are node outputs or colors."""
    node = nodes.new("ShaderNodeMix")
    node.data_type = "RGBA"
    node.blend_type = blend
    for socket, value in ((node.inputs[0], fac), (node.inputs[6], a), (node.inputs[7], b)):
        if isinstance(value, bpy.types.NodeSocket):
            links.new(value, socket)
        else:
            socket.default_value = value
    return node.outputs[2]


def make_armor_material(name, color=None, camo=None, roughness=0.65, metallic=0.0, wear=0.12, pixel=0.02, scale=7.0):
    """Painted armor with grime in the corners and scratched-through bare metal, like the character sheet.
    color: a plain paint color. camo: four colors for digital camo instead (noise read through a coarse grid
    so it breaks into square pixels). Blender-only for now; for Unreal it gets baked to a texture later."""
    full = f"{SUIT}_{name}"
    mat = bpy.data.materials.get(full) or bpy.data.materials.new(full)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    for node in list(nodes):
        if node.type != "OUTPUT_MATERIAL":
            nodes.remove(node)
    out = next(n for n in nodes if n.type == "OUTPUT_MATERIAL")
    geometry = nodes.new("ShaderNodeNewGeometry")

    if camo:
        snap = nodes.new("ShaderNodeVectorMath")
        snap.operation = "SNAP"
        snap.inputs[1].default_value = (pixel, pixel, pixel)
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = scale
        noise.inputs["Detail"].default_value = 2.0
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.interpolation = "CONSTANT"
        stops = [(0.0, camo[0]), (0.42, camo[1]), (0.52, camo[2]), (0.61, camo[3])]
        elements = ramp.color_ramp.elements
        while len(elements) < len(stops):
            elements.new(0.5)
        for element, (position, c) in zip(elements, stops):
            element.position = position
            element.color = c
        links.new(geometry.outputs["Position"], snap.inputs[0])
        links.new(snap.outputs["Vector"], noise.inputs["Vector"])
        links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        paint = ramp.outputs["Color"]
    else:
        paint = color

    # Grime: darker in the creases (ambient occlusion) and in big dirty smudges.
    ao = nodes.new("ShaderNodeAmbientOcclusion")
    ao.inputs["Distance"].default_value = 0.12
    ao_ramp = nodes.new("ShaderNodeValToRGB")
    ao_ramp.color_ramp.elements[0].color = (0.35, 0.33, 0.3, 1)
    links.new(ao.outputs["AO"], ao_ramp.inputs["Fac"])
    dirt = nodes.new("ShaderNodeTexNoise")
    dirt.inputs["Scale"].default_value = 3.0
    dirt.inputs["Detail"].default_value = 6.0
    dirt_ramp = nodes.new("ShaderNodeValToRGB")
    dirt_ramp.color_ramp.elements[0].position = 0.35
    dirt_ramp.color_ramp.elements[0].color = (0.62, 0.58, 0.52, 1)
    dirt_ramp.color_ramp.elements[1].position = 0.7
    links.new(dirt.outputs["Fac"], dirt_ramp.inputs["Fac"])
    grimy = mix_rgb(nodes, links, "MULTIPLY", 1.0, paint, ao_ramp.outputs["Color"])
    grimy = mix_rgb(nodes, links, "MULTIPLY", 1.0, grimy, dirt_ramp.outputs["Color"])

    # Scratches: thin streaks where the paint is worn through to bare metal.
    scratch = nodes.new("ShaderNodeTexNoise")
    scratch.inputs["Scale"].default_value = 40.0
    scratch.inputs["Detail"].default_value = 8.0
    stretch = nodes.new("ShaderNodeMapping")
    stretch.inputs["Scale"].default_value = (1.0, 1.0, 8.0)
    links.new(geometry.outputs["Position"], stretch.inputs["Vector"])
    links.new(stretch.outputs["Vector"], scratch.inputs["Vector"])
    scratch_ramp = nodes.new("ShaderNodeValToRGB")
    scratch_ramp.color_ramp.elements[0].position = 0.72 - wear * 0.5
    scratch_ramp.color_ramp.elements[1].position = 0.74 - wear * 0.5
    links.new(scratch.outputs["Fac"], scratch_ramp.inputs["Fac"])
    bare = srgb(150, 146, 134)
    final = mix_rgb(nodes, links, "MIX", scratch_ramp.outputs["Color"], grimy, bare)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    links.new(final, bsdf.inputs["Base Color"])
    rough = nodes.new("ShaderNodeMapRange")
    rough.inputs["To Min"].default_value = roughness
    rough.inputs["To Max"].default_value = 0.32
    links.new(scratch_ramp.outputs["Color"], rough.inputs["Value"])
    links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    metal = nodes.new("ShaderNodeMapRange")
    metal.inputs["To Min"].default_value = metallic
    metal.inputs["To Max"].default_value = 1.0
    links.new(scratch_ramp.outputs["Color"], metal.inputs["Value"])
    links.new(metal.outputs["Result"], bsdf.inputs["Metallic"])
    # Small dents and brush marks.
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.15
    bump.inputs["Distance"].default_value = 0.002
    links.new(scratch.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    mat.diffuse_color = camo[1] if camo else color
    return mat


M = {}


def build_materials():
    # Colors picked from the character sheet: dark woodland MARPAT, matte olive drab, dark gunmetal, worn metal.
    M["camo"] = make_armor_material("Camo", camo=[srgb(68, 52, 35), srgb(74, 74, 49), srgb(38, 45, 29), srgb(24, 24, 20)], wear=0.03)
    M["olive"] = make_armor_material("OliveDrab", srgb(72, 72, 48), roughness=0.75, wear=0.06)
    M["gunmetal"] = make_armor_material("Gunmetal", srgb(40, 40, 38), roughness=0.5, metallic=0.75, wear=0.06)
    M["worn"] = make_armor_material("WornMetal", srgb(112, 108, 96), roughness=0.4, metallic=1.0, wear=0.2)
    M["medal"] = make_material("Medal", srgb(160, 154, 136), roughness=0.42, metallic=0.55)
    M["rubber"] = make_material("Rubber", srgb(20, 20, 19), roughness=0.85)
    M["silver"] = make_material("Insignia", srgb(176, 172, 160), roughness=0.28, metallic=1.0)
    M["visor"] = make_material("Visor", srgb(26, 90, 255), roughness=0.2, emission=srgb(40, 110, 255), strength=14.0)
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


def finish(name, bm, joint, material, bevel, segments=2):
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
        mod.segments = segments
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


def box(name, joint, center, size, material, rotation=(0, 0, 0), taper=(1.0, 1.0), bevel=1.2, round=2):
    """A box size = (depth front-to-back, width, height) cm. taper scales the TOP face (depth, width).
    bevel is how far the edges are worn down (cm); round is how many steps (1 = a flat chamfer, 4 = rounded)."""
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
    return finish(name, bm, joint, material, bevel, round)


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
    box("PelvisCore", j, (0, 0, -10), (32, 46, 22), "gunmetal", bevel=3)
    box("Belt", j, (0, 0, 0), (44, 60, 9), "gunmetal", bevel=2.5)
    box("BeltBuckle", j, (22.5, 0, 0), (2, 12, 7), "worn", bevel=0.8)
    box("Codpiece", j, (17, 0, -15), (10, 24, 22), "camo", rotation=(10, 0, 0), taper=(1.0, 1.35), bevel=3, round=1)
    box("RearPlate", j, (-20, 0, -13), (7, 40, 20), "camo", rotation=(-10, 0, 0), bevel=3, round=1)
    for side, s in (("L", -1), ("R", 1)):
        box(f"BeltPouch{side}", j, (18, 19 * s, -5), (9, 13, 13), "olive", bevel=2.5, round=3)
        box(f"BeltPouchFlap{side}", j, (22.6, 19 * s, 0), (2, 12, 4), "olive", bevel=0.8)
        box(f"SidePouch{side}", j, (-6, 31 * s, -4), (16, 7, 14), "olive", bevel=2.5, round=3)
        box(f"Tasset{side}", j, (5, 30 * s, -18), (26, 5, 24), "camo", rotation=(0, 0, -14 * s), bevel=2.5, round=1)
        box(f"TassetRim{side}", j, (5, 31.5 * s, -30), (26, 5, 3), "worn", rotation=(0, 0, -14 * s), bevel=0.8)


def build_torso():
    j = "TorsoPivot"
    # Belly: dark under-suit with stacked plates.
    box("Abdomen", j, (0, 0, 12), (32, 44, 26), "gunmetal", bevel=3)
    for i, z in enumerate((4, 11, 18)):
        box(f"AbPlate{i}", j, (17, 0, z), (6, 32 - i * 2, 6.5), "olive", bevel=1.5, round=1)
    for side, s in (("L", -1), ("R", 1)):
        box(f"Oblique{side}", j, (6, 21 * s, 12), (22, 8, 22), "camo", rotation=(0, 0, -6 * s), bevel=2, round=1)

    # The barrel chest: a big rounded core with two heavy chest plates angled out to the sides.
    box("ChestCore", j, (0, 0, 44), (54, 84, 50), "camo", taper=(0.95, 1.05), bevel=9, round=4)
    for side, s in (("L", -1), ("R", 1)):
        box(f"ChestPlate{side}", j, (25, 21 * s, 46), (14, 40, 34), "camo", rotation=(-8, -14 * s, 0),
            taper=(0.9, 0.92), bevel=3.5, round=1)
        box(f"ChestPlateRim{side}", j, (26, 21 * s, 28), (12, 36, 3.5), "worn", rotation=(-8, -14 * s, 0), bevel=1)
        box(f"SideRib{side}", j, (-2, 41 * s, 34), (32, 8, 26), "gunmetal", bevel=2.5)
        for i in range(3):
            box(f"RibVent{side}{i}", j, (8 - i * 7, 45.2 * s, 34), (4, 1, 18), "rubber", bevel=0)
    box("Sternum", j, (28, 0, 38), (8, 12, 30), "gunmetal", bevel=2, round=1)

    # High collar around the head, so the helmet sits down between the shoulders.
    box("Collar", j, (-8, 0, 70), (40, 66, 18), "camo", taper=(0.8, 0.78), bevel=5, round=3)
    for side, s in (("L", -1), ("R", 1)):
        box(f"CollarCowl{side}", j, (4, 25 * s, 72), (32, 12, 18), "camo", rotation=(0, 0, 18 * s),
            taper=(0.85, 0.7), bevel=3.5, round=2)

    # Eagle, globe and anchor medallion on the chest.
    cylinder("EmblemDisc", j, (33.5, 0, 50), 9.5, 2.5, "medal", rotation=(82, 0, 0), segments=32)
    cylinder("EmblemRing", j, (34.6, 0, 50), 9.5, 1.0, "worn", rotation=(82, 0, 0), top_radius=8.0, segments=32, bevel=0)
    ellipsoid("EmblemGlobe", j, (37.5, 0, 49.5), (2.5, 4.8, 4.8), "silver")
    box("EmblemEagle", j, (37.5, 0, 56.5), (2, 17, 3), "silver", rotation=(-8, 0, 0), taper=(1, 0.45), bevel=0.4)
    box("EmblemEagleHead", j, (38, 0, 59), (2, 3, 3), "silver", bevel=0.5)
    box("EmblemAnchor", j, (37.5, 0, 43), (2, 3, 11), "silver", rotation=(-8, 0, 0), bevel=0.4)
    box("EmblemAnchorArm", j, (37.5, 0, 39), (2, 10, 2), "silver", rotation=(-8, 0, 0), bevel=0.4)

    # Jet pack frame and nozzles, then the big assault pack (power / ammo / comms).
    box("JetPack", j, (-32, 0, 44), (24, 64, 52), "gunmetal", bevel=3)
    for side, s in (("L", -1), ("R", 1)):
        cylinder(f"Nozzle{side}", j, (-38, 21 * s, 15), 9, 18, "gunmetal", top_radius=7.5)
        cylinder(f"NozzleGlow{side}", j, (-38, 21 * s, 5.5), 7, 1.0, "thruster", bevel=0)
        box(f"PackPouch{side}", j, (-56, 39 * s, 42), (20, 12, 34), "olive", bevel=3, round=3)
        box(f"PackPouchFlap{side}", j, (-56, 39.5 * s, 58), (21, 12, 5), "olive", bevel=1.5)
        box(f"PackStrap{side}", j, (-14, 24 * s, 62), (36, 7, 5), "rubber", bevel=0.8)
        box(f"PackFrame{side}", j, (-70, 32 * s, 48), (4, 4, 62), "gunmetal", bevel=1)
    box("AssaultPack", j, (-56, 0, 50), (26, 68, 56), "camo", bevel=4, round=3)
    box("PackLid", j, (-56, 0, 80), (28, 64, 8), "olive", bevel=2.5, round=2)
    box("PackBackPanel", j, (-70, 0, 46), (5, 44, 34), "gunmetal", bevel=1.5)
    for i in range(4):
        box(f"PackSlot{i}", j, (-72.8, -14 + i * 9, 40), (1, 6, 20), "rubber", bevel=0)
    for i in range(3):
        box(f"PackLight{i}", j, (-72.8, 6 + i * 5, 58), (1, 3, 2), "visor", bevel=0)
    cylinder("Antenna", j, (-60, 26, 106), 0.8, 50, "gunmetal", segments=8, bevel=0)
    cylinder("AntennaBase", j, (-60, 26, 84), 2.5, 6, "gunmetal", segments=12)
    ellipsoid("AntennaTip", j, (-60, 26, 131), (1.4, 1.4, 1.4), "gunmetal")


def build_head():
    # The helmet sits low, down in the collar, for the hunched heavy look.
    j = "HeadPivot"
    cylinder("Neck", j, (0, 0, -2), 9, 8, "rubber")
    ellipsoid("Skull", j, (-1, 0, 11), (16, 14.5, 15), "gunmetal")
    box("HelmetShell", j, (-2, 0, 14), (32, 30, 24), "camo", taper=(0.8, 0.78), bevel=6, round=3)
    box("Crest", j, (0, 0, 26.5), (28, 6, 4), "olive", rotation=(-4, 0, 0), bevel=1.5, round=1)
    # Heavy brow over angled, scowling eye slits.
    box("Brow", j, (13, 0, 18.5), (12, 30, 7), "worn", rotation=(-18, 0, 0), taper=(0.8, 0.82), bevel=2, round=1)
    box("VisorRecess", j, (15.5, 0, 13), (4, 25, 7), "rubber", bevel=0.8)
    for side, s in (("L", -1), ("R", 1)):
        box(f"Eye{side}", j, (17.8, 6.2 * s, 13.5), (1.5, 10, 3.2), "visor", rotation=(0, 0, 14 * s), bevel=0.3)
    box("Faceplate", j, (14, 0, 5), (10, 24, 10), "gunmetal", rotation=(10, 0, 0), taper=(1.0, 1.15), bevel=2, round=1)
    box("Jaw", j, (14.5, 0, -2), (11, 18, 7), "olive", taper=(1.0, 1.4), bevel=2, round=1)
    for i, y in enumerate((-5, -2.5, 0, 2.5, 5)):
        box(f"JawVent{i}", j, (19.4, y, 4), (1, 1.4, 6), "rubber", rotation=(10, 0, 0), bevel=0)
    for side, s in (("L", -1), ("R", 1)):
        box(f"Cheek{side}", j, (6, 13.5 * s, 6), (22, 5, 18), "camo", rotation=(0, 8 * s, 10 * s), bevel=2, round=1)
        cylinder(f"EarModule{side}", j, (-2, 17.5 * s, 12), 5.5, 4, "gunmetal", rotation=SIDEWAYS, segments=24)
        cylinder(f"EarBolt{side}", j, (-2, 19.8 * s, 12), 2.5, 1.5, "worn", rotation=SIDEWAYS, segments=16, bevel=0.2)
        # Swept blade horns.
        box(f"HornBase{side}", j, (-3, 9 * s, 24), (12, 6, 6), "gunmetal", bevel=1.5)
        box(f"Horn{side}", j, (-6, 10 * s, 38), (16, 3.5, 26), "worn", rotation=(-16, 0, -6 * s),
            taper=(0.12, 0.4), bevel=0.8, round=1)


def build_arm(side, s):
    sh, el = f"{side}Shoulder", f"{side}Elbow"
    # Massive rounded pauldrons that rise up past the collar.
    ellipsoid(f"{side}ShoulderBall", sh, (0, 6 * s, 2), (19, 17, 16), "gunmetal")
    box(f"{side}Pauldron", sh, (-2, 10 * s, 10), (56, 44, 44), "camo", taper=(0.82, 0.78), bevel=10, round=4)
    box(f"{side}PauldronTop", sh, (-2, 12 * s, 30), (42, 28, 6), "olive", bevel=2.5, round=2)
    box(f"{side}PauldronRim", sh, (-2, 15 * s, -11), (58, 42, 6), "worn", bevel=2)
    box(f"{side}PauldronPlate", sh, (-2, 32 * s, 6), (44, 5, 30), "camo", bevel=2, round=1)
    for i, x in enumerate((-18, 14)):
        cylinder(f"{side}PauldronBolt{i}", sh, (x, 35.5 * s, 16), 1.6, 1.5, "worn", rotation=SIDEWAYS, segments=10, bevel=0)
    cylinder(f"{side}UpperArm", sh, (0, 0, -22), 10, 32, "gunmetal")
    box(f"{side}BicepArmor", sh, (3, 3 * s, -24), (26, 26, 22), "camo", bevel=4, round=3)
    ellipsoid(f"{side}ElbowPad", el, (-8, 0, 0), (9, 10, 9), "olive")
    cylinder(f"{side}ElbowJoint", el, (0, 0, 0), 8.5, 24, "gunmetal", rotation=SIDEWAYS)
    # Huge gauntlets that flare out toward the fist.
    box(f"{side}Forearm", el, (2, 0, -20), (38, 36, 34), "camo", taper=(0.86, 0.86), bevel=5, round=3)
    box(f"{side}ForearmPlate", el, (21, 0, -21), (3, 22, 26), "olive", bevel=1.2, round=1)
    box(f"{side}ForearmCuff", el, (2, 0, -37), (38, 36, 4), "worn", bevel=1.2)
    cylinder(f"{side}Wrist", el, (2, 0, -40), 9.5, 5, "gunmetal")
    box(f"{side}Hand", el, (3, 0, -48), (19, 16, 15), "gunmetal", bevel=3, round=2)
    box(f"{side}Knuckles", el, (11, 0, -55), (7, 16, 8), "worn", rotation=(-20, 0, 0), bevel=1.5)
    box(f"{side}Fingers", el, (6, 0, -57), (9, 15, 8), "gunmetal", rotation=(-20, 0, 0), bevel=1.5)
    box(f"{side}Thumb", el, (10, -7 * s, -48), (6, 5, 10), "gunmetal", bevel=1.2)


def build_flag():
    # U.S. flag patch on the outside of the left pauldron, canton toward the front.
    j, y = "LeftShoulder", -35.0
    rot = (0, 0, 0)
    box("FlagField", j, (2, y, 6), (24, 1, 15), "flag_white", rotation=rot, bevel=0)
    for i in range(4):
        box(f"FlagStripe{i}", j, (2, y - 0.2, 12.5 - i * 4.3), (24, 1, 2.1), "flag_red", rotation=rot, bevel=0)
    box("FlagCanton", j, (8.5, y - 0.4, 9.7), (11, 1, 7.6), "flag_blue", rotation=rot, bevel=0)


def build_leg(side, s):
    hip, knee, ankle = f"{side}Hip", f"{side}Knee", f"{side}Ankle"
    box(f"{side}ThighCore", hip, (0, 0, -20), (28, 26, 40), "gunmetal", bevel=3)
    box(f"{side}ThighArmor", hip, (3, 1 * s, -21), (38, 36, 36), "camo", taper=(1.06, 1.06), bevel=6, round=3)
    box(f"{side}ThighSidePlate", hip, (1, 19 * s, -18), (24, 5, 26), "olive", bevel=1.5, round=1)
    cylinder(f"{side}KneeJoint", knee, (0, 0, 0), 9, 26, "gunmetal", rotation=SIDEWAYS)
    box(f"{side}KneePad", knee, (15, 0, 2), (14, 26, 22), "olive", rotation=(-12, 0, 0), taper=(1, 0.85), bevel=5, round=3)
    box(f"{side}KneePadRim", knee, (18.5, 0, -8), (6, 24, 3), "worn", rotation=(-12, 0, 0), bevel=1)
    # Shins flare out toward the boot.
    box(f"{side}Shin", knee, (4, 0, -19), (38, 36, 30), "camo", taper=(0.84, 0.84), bevel=5, round=3)
    box(f"{side}ShinPlate", knee, (21, 0, -18), (3, 18, 22), "olive", rotation=(-6, 0, 0), bevel=1.2, round=1)
    box(f"{side}Calf", knee, (-15, 0, -16), (10, 26, 24), "gunmetal", bevel=2)
    cylinder(f"{side}CalfJet", knee, (-20, 0, -26), 5.5, 12, "gunmetal", top_radius=6.5)
    cylinder(f"{side}AnkleJoint", ankle, (0, 0, 6), 8, 30, "gunmetal", rotation=SIDEWAYS)
    box(f"{side}AnkleArmor", ankle, (2, 0, 9), (34, 36, 10), "camo", bevel=3, round=2)
    box(f"{side}Boot", ankle, (10, 0, 1), (56, 36, 18), "gunmetal", taper=(0.9, 0.95), bevel=5, round=3)
    box(f"{side}ToeCap", ankle, (32, 0, -1), (12, 32, 13), "camo", bevel=4, round=3)
    box(f"{side}Heel", ankle, (-16, 0, -1), (10, 30, 14), "olive", bevel=3, round=2)
    box(f"{side}Sole", ankle, (10, 0, -7), (58, 37, 2), "rubber", bevel=0.8)


def build_hmg():
    # HMG-88 in the right hand. In the elbow's space -Z runs down the forearm and out along the barrel,
    # and +X is the top of the forearm (up, when the gun is raised).
    j = "RightElbow"
    box("HmgGrip", j, (10, 0, -48), (14, 6, 9), "gunmetal", bevel=1)
    box("HmgReceiver", j, (24, 0, -62), (26, 18, 76), "gunmetal", bevel=2.5, round=1)
    box("HmgTopCover", j, (38, 0, -66), (4, 17, 60), "worn", bevel=1.5)
    for side, s in (("L", -1), ("R", 1)):
        box(f"HmgSidePlate{side}", j, (24, 9.6 * s, -70), (18, 2, 44), "olive", bevel=0.8)
        for i in range(4):
            box(f"HmgSideGroove{side}{i}", j, (24, 10.8 * s, -54 - i * 10), (14, 0.8, 2), "rubber", bevel=0)
    box("HmgHandle", j, (49, 0, -66), (4, 5, 34), "gunmetal", bevel=1.2)
    for i, z in enumerate((-52, -80)):
        box(f"HmgHandlePost{i}", j, (44, 0, z), (8, 4, 4), "gunmetal", bevel=0.8)
    box("HmgStock", j, (22, 0, -18), (22, 14, 14), "gunmetal", bevel=2, round=1)
    box("HmgStockPad", j, (22, 0, -10), (24, 15, 3), "rubber", bevel=1)
    # Ammo drum on the side and the feed into the receiver.
    cylinder("HmgDrum", j, (8, -17, -72), 14, 15, "olive", rotation=SIDEWAYS, segments=32, bevel=1.5)
    cylinder("HmgDrumCap", j, (8, -25, -72), 10, 1.5, "worn", rotation=SIDEWAYS, segments=32, bevel=0.4)
    cylinder("HmgDrumHub", j, (8, -26.2, -72), 3.5, 1.5, "gunmetal", rotation=SIDEWAYS, segments=16, bevel=0.2)
    box("HmgFeed", j, (16, -10, -72), (10, 8, 12), "gunmetal", bevel=1)
    # Long barrel jacket with cooling holes.
    cylinder("HmgShroud", j, (28, 0, -124), 8.5, 48, "gunmetal", segments=28)
    cylinder("HmgShroudCollar", j, (28, 0, -101), 9.8, 4, "worn", segments=28, bevel=0.6)
    for row in range(4):
        for k in range(8):
            a = 2 * math.pi * (k + 0.5 * (row % 2)) / 8
            cylinder(f"HmgHole{row}_{k}", j, (28 + 8.3 * math.cos(a), 8.3 * math.sin(a), -110 - row * 9), 1.6, 0.8,
                     "rubber", rotation=(-90, math.degrees(a), 0), segments=10, bevel=0)
    cylinder("HmgBarrel", j, (28, 0, -154), 4.5, 16, "gunmetal", segments=20)
    cylinder("HmgMuzzleBrake", j, (28, 0, -164), 6.5, 9, "gunmetal", segments=20)
    for i in range(2):
        box(f"HmgBrakeSlot{i}", j, (28, 0, -162 - i * 4), (3, 14, 1.5), "rubber", bevel=0)
    box("HmgFrontGrip", j, (10, 0, -96), (14, 7, 10), "gunmetal", bevel=1.5)


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
        # Like the character sheet: HMG held low across the hip, left hand on top of the gun, wide heavy stance.
        set_joint("RightShoulder", 28, 0, -10)
        set_joint("RightElbow", 62, 0, 0)
        set_joint("LeftShoulder", 52, 28, 0)
        set_joint("LeftElbow", 58)
        set_joint("TorsoPivot", -8, 10, 0)
        set_joint("HeadPivot", 6, -6, 0)
        set_joint("LeftHip", 16, 0, 9)
        set_joint("LeftKnee", -26)
        set_joint("LeftAnkle", 10)
        set_joint("RightHip", -8, 0, -9)
        set_joint("RightKnee", -14)
        set_joint("RightAnkle", 22)
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
