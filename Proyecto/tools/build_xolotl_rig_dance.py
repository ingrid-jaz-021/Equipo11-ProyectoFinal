import math
import os
import struct

import bpy
from mathutils import Vector


PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODEL_DIR = os.path.join(PROJECT_DIR, "Models", "EntornoCompleto", "Laguna", "Xolotl")
SOURCE_BLEND = os.path.join(MODEL_DIR, "Xolotl.blend")
OUTPUT_BLEND = os.path.join(MODEL_DIR, "XolotlDanza.blend")
OUTPUT_FBX = os.path.join(MODEL_DIR, "XolotlDanza.fbx")
OUTPUT_VERTEX_ANIMATION = os.path.join(MODEL_DIR, "XolotlDanza.xanim")
FPS = 30
CYCLE_FRAMES = 90
KEY_STEP = 3


def point_segment_distance(point, start, end):
    segment = end - start
    length_squared = segment.length_squared
    if length_squared < 1.0e-10:
        return (point - start).length
    amount = max(0.0, min(1.0, (point - start).dot(segment) / length_squared))
    return (point - (start + amount * segment)).length


def make_rig():
    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = CYCLE_FRAMES + 1
    scene.render.fps = FPS

    meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    if not meshes:
        raise RuntimeError("Xolotl.blend no contiene mallas.")

    arm_data = bpy.data.armatures.new("XolotlDanceSkeleton")
    armature = bpy.data.objects.new("XolotlDanceRig", arm_data)
    scene.collection.objects.link(armature)
    armature.matrix_world = mathutils_identity()

    specifications = {
        "Root": ((0.0, 0.0, 0.72), (0.0, 0.0, 1.02), None),
        "Spine": ((0.0, 0.0, 1.02), (0.0, 0.0, 1.40), "Root"),
        "Chest": ((0.0, 0.0, 1.40), (0.0, 0.0, 1.76), "Spine"),
        "Neck": ((0.0, 0.0, 1.76), (0.0, 0.0, 2.04), "Chest"),
        "Head": ((0.0, 0.0, 2.04), (0.0, 0.0, 2.62), "Neck"),
    }
    for side, sign in (("L", 1.0), ("R", -1.0)):
        specifications["UpperArm." + side] = ((0.34 * sign, 0.0, 1.72), (0.67 * sign, 0.0, 1.67), "Chest")
        specifications["Forearm." + side] = ((0.67 * sign, 0.0, 1.67), (0.96 * sign, -0.01, 1.51), "UpperArm." + side)
        specifications["Hand." + side] = ((0.96 * sign, -0.01, 1.51), (1.19 * sign, -0.04, 1.40), "Forearm." + side)
        specifications["Thigh." + side] = ((0.22 * sign, 0.0, 0.92), (0.25 * sign, 0.0, 0.56), "Root")
        specifications["Shin." + side] = ((0.25 * sign, 0.0, 0.56), (0.25 * sign, 0.0, 0.16), "Thigh." + side)
        specifications["Foot." + side] = ((0.25 * sign, 0.0, 0.16), (0.25 * sign, -0.20, 0.06), "Shin." + side)

    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="EDIT")
    edit_bones = {}
    for name, (head, tail, _parent) in specifications.items():
        bone = arm_data.edit_bones.new(name)
        bone.head = Vector(head)
        bone.tail = Vector(tail)
        bone.use_deform = True
        edit_bones[name] = bone
    for name, (_head, _tail, parent_name) in specifications.items():
        if parent_name:
            edit_bones[name].parent = edit_bones[parent_name]
            edit_bones[name].use_connect = (name.startswith("Forearm.") or name.startswith("Hand.") or
                                             name.startswith("Shin.") or name.startswith("Foot."))
    bpy.ops.object.mode_set(mode="OBJECT")

    sigmas = {
        "Root": 0.34, "Spine": 0.30, "Chest": 0.32, "Neck": 0.24, "Head": 0.48,
        "UpperArm.L": 0.23, "Forearm.L": 0.20, "Hand.L": 0.19,
        "UpperArm.R": 0.23, "Forearm.R": 0.20, "Hand.R": 0.19,
        "Thigh.L": 0.23, "Shin.L": 0.20, "Foot.L": 0.18,
        "Thigh.R": 0.23, "Shin.R": 0.20, "Foot.R": 0.18,
    }
    segments = {
        name: (Vector(head), Vector(tail)) for name, (head, tail, _parent) in specifications.items()
    }
    for obj in meshes:
        for group in list(obj.vertex_groups):
            obj.vertex_groups.remove(group)
        groups = {name: obj.vertex_groups.new(name=name) for name in specifications}
        starts = {name: Vector(specifications[name][0]) for name in specifications}
        ends = {name: Vector(specifications[name][1]) for name in specifications}
        for vertex in obj.data.vertices:
            point = obj.matrix_world @ vertex.co
            if obj.name == "Plumas" or obj.name == "EXPORT":
                groups["Head"].add([vertex.index], 1.0, "REPLACE")
                continue

            influence = []
            for name, (start, end) in segments.items():
                is_arm = name.startswith(("UpperArm.", "Forearm.", "Hand."))
                is_leg = name.startswith(("Thigh.", "Shin.", "Foot."))
                if is_arm and (abs(point.x) < 0.26 or (point.x > 0.0) != name.endswith(".L")):
                    continue
                if is_leg and point.z > 1.28:
                    continue
                if name == "Head" and point.z < 1.82:
                    continue
                if name in ("Root", "Spine", "Chest", "Neck") and point.z > 2.35:
                    continue
                distance = point_segment_distance(point, start, end)
                sigma = sigmas[name]
                weight = math.exp(-0.5 * (distance / sigma) ** 2)
                if weight > 0.025:
                    influence.append((weight, name))
            influence.sort(reverse=True)
            influence = influence[:4]
            total = sum(value for value, _name in influence)
            if total <= 1.0e-8:
                nearest = min(segments, key=lambda name: point_segment_distance(point, *segments[name]))
                groups[nearest].add([vertex.index], 1.0, "REPLACE")
            else:
                for value, name in influence:
                    groups[name].add([vertex.index], value / total, "REPLACE")

        preserve = obj.matrix_world.copy()
        obj.parent = armature
        obj.matrix_world = preserve
        modifier = obj.modifiers.new("XolotlDanceArmature", "ARMATURE")
        modifier.object = armature
        modifier.use_deform_preserve_volume = True

    if len(arm_data.bones) != 17:
        raise RuntimeError("El rig generado no tiene los 17 huesos esperados.")
    return armature, meshes, specifications


def mathutils_identity():
    from mathutils import Matrix
    return Matrix.Identity(4)


def animate(armature, specifications):
    scene = bpy.context.scene
    action = bpy.data.actions.new("XolotlSwingDanceLoop")
    armature.animation_data_create()
    armature.animation_data.action = action
    poses = armature.pose.bones
    for pose in poses:
        pose.rotation_mode = "XYZ"

    key_frames = list(range(1, CYCLE_FRAMES + 1, KEY_STEP))
    key_frames.append(CYCLE_FRAMES + 1)
    for frame in key_frames:
        phase = 2.0 * math.pi * (frame - 1) / CYCLE_FRAMES
        left = math.sin(phase)
        right = math.sin(phase + math.pi)
        bounce = 0.5 - 0.5 * math.cos(phase * 2.0)
        sway = math.sin(phase)

        pose_values = {
            "Root": ((0.0, 0.0, 0.045 * bounce), (0.0, 0.0, 0.075 * sway)),
            "Spine": ((0.0, 0.0, 0.0), (0.055 * sway, 0.025 * sway, 0.035 * bounce)),
            "Chest": ((0.0, 0.0, 0.0), (0.025 * sway, -0.055 * sway, 0.075 * sway)),
            "Neck": ((0.0, 0.0, 0.0), (0.035 * bounce, 0.025 * sway, 0.0)),
            "Head": ((0.0, 0.0, 0.0), (-0.04 * bounce, 0.025 * sway, -0.035 * sway)),
            "UpperArm.L": ((0.0, 0.0, 0.0), (0.10 + 0.95 * left, 0.05 + 0.15 * left, 0.12 * left)),
            "Forearm.L": ((0.0, 0.0, 0.0), (0.18 + 0.65 * max(0.0, left), 0.06 * left, 0.04 * left)),
            "Hand.L": ((0.0, 0.0, 0.0), (0.0, 0.0, 0.12 * left)),
            "UpperArm.R": ((0.0, 0.0, 0.0), (-0.10 + 0.95 * right, -0.05 - 0.15 * right, -0.12 * right)),
            "Forearm.R": ((0.0, 0.0, 0.0), (0.18 + 0.65 * max(0.0, right), 0.06 * right, -0.04 * right)),
            "Hand.R": ((0.0, 0.0, 0.0), (0.0, 0.0, -0.12 * right)),
            "Thigh.L": ((0.0, 0.0, 0.0), (0.48 * left, 0.035 * left, 0.025 * left)),
            "Shin.L": ((0.0, 0.0, 0.0), (0.58 * max(0.0, right), 0.0, 0.0)),
            "Foot.L": ((0.0, 0.0, 0.0), (-0.12 * max(0.0, right), 0.0, 0.0)),
            "Thigh.R": ((0.0, 0.0, 0.0), (0.48 * right, -0.035 * left, -0.025 * left)),
            "Shin.R": ((0.0, 0.0, 0.0), (0.58 * max(0.0, left), 0.0, 0.0)),
            "Foot.R": ((0.0, 0.0, 0.0), (-0.12 * max(0.0, left), 0.0, 0.0)),
        }
        for name, (location, rotation) in pose_values.items():
            pose = poses[name]
            pose.location = location if name == "Root" else (0.0, 0.0, 0.0)
            pose.rotation_euler = rotation
            if name == "Root":
                pose.keyframe_insert(data_path="location", frame=frame, group=name)
            pose.keyframe_insert(data_path="rotation_euler", frame=frame, group=name)

    scene.frame_set(1)
    return action


def to_engine_vector(value):
    return (float(value.x), float(value.z), float(-value.y))


def write_vertex_animation(meshes):
    scene = bpy.context.scene
    key_frames = list(range(1, CYCLE_FRAMES + 2, KEY_STEP))
    depsgraph = bpy.context.evaluated_depsgraph_get()
    sampled = {obj.name: [] for obj in meshes}
    materials = {}

    for obj in meshes:
        material = obj.data.materials[0] if obj.data.materials else None
        if not material:
            materials[obj.name] = (obj.name, "", (0.8, 0.8, 0.8), (0.0, 0.0, 0.0), 1.0)
            continue
        texture_path = ""
        if material.node_tree:
            image_node = next((node for node in material.node_tree.nodes
                               if node.type == "TEX_IMAGE" and node.image), None)
            if image_node:
                texture_path = os.path.basename(bpy.path.abspath(image_node.image.filepath))
        diffuse = tuple(float(v) for v in material.diffuse_color[:3])
        opacity = float(material.diffuse_color[3])
        materials[obj.name] = (material.name, texture_path, diffuse, (0.0, 0.0, 0.0), opacity)

    for frame in key_frames:
        scene.frame_set(frame)
        depsgraph.update()
        for obj in meshes:
            evaluated = obj.evaluated_get(depsgraph)
            mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
            mesh.calc_loop_triangles()
            uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None
            normal_matrix = evaluated.matrix_world.to_3x3().inverted().transposed()
            values = []
            for triangle in mesh.loop_triangles:
                for loop_index in triangle.loops:
                    loop = mesh.loops[loop_index]
                    position = evaluated.matrix_world @ mesh.vertices[loop.vertex_index].co
                    normal = (normal_matrix @ loop.normal).normalized()
                    uv = uv_layer[loop_index].uv if uv_layer else (0.0, 0.0)
                    values.append((to_engine_vector(position), to_engine_vector(normal), (float(uv.x), float(uv.y))))
            evaluated.to_mesh_clear()
            if not values or len(values) % 3:
                raise RuntimeError("Malla triangulada invalida: " + obj.name)
            if sampled[obj.name] and len(values) != len(sampled[obj.name][0]):
                raise RuntimeError("La topologia cambia durante el baile: " + obj.name)
            sampled[obj.name].append(values)

    def write_string(stream, value):
        encoded = value.encode("utf-8")
        stream.write(struct.pack("<I", len(encoded)))
        stream.write(encoded)

    with open(OUTPUT_VERTEX_ANIMATION, "wb") as stream:
        stream.write(b"XOLXANM1")
        stream.write(struct.pack("<IfII", 1, CYCLE_FRAMES / FPS, len(key_frames), len(meshes)))
        for obj in meshes:
            material_name, texture_path, diffuse, emissive, opacity = materials[obj.name]
            frames = sampled[obj.name]
            base = frames[0]
            vertex_count = len(base)
            write_string(stream, obj.name)
            write_string(stream, material_name)
            write_string(stream, texture_path)
            stream.write(struct.pack("<7f", *diffuse, *emissive, opacity))
            stream.write(struct.pack("<II", vertex_count, vertex_count))
            for position, normal, uv in base:
                stream.write(struct.pack("<8f", *position, *normal, *uv))
            stream.write(struct.pack("<{}I".format(vertex_count), *range(vertex_count)))
            for frame in frames:
                for position, normal, _uv in frame:
                    stream.write(struct.pack("<6f", *position, *normal))
    print("Created runtime vertex animation:", OUTPUT_VERTEX_ANIMATION)


def main():
    bpy.ops.wm.open_mainfile(filepath=SOURCE_BLEND)
    armature, meshes, specifications = make_rig()
    action = animate(armature, specifications)

    scene = bpy.context.scene
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=OUTPUT_BLEND)

    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.export_scene.fbx(
        filepath=OUTPUT_FBX,
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        axis_forward="-Z",
        axis_up="Y",
        add_leaf_bones=False,
        bake_anim=True,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False,
        bake_anim_step=1.0,
        bake_anim_simplify_factor=0.0,
        path_mode="COPY",
    )
    write_vertex_animation(meshes)
    print("Created rig with {} bones and {} mesh objects.".format(len(armature.data.bones), len(meshes)))
    print("Created cyclic dance action on {} bones: {}.".format(len(armature.pose.bones), action.name))
    print("Saved Blender source:", OUTPUT_BLEND)
    print("Exported runtime FBX:", OUTPUT_FBX)


if __name__ == "__main__":
    main()
