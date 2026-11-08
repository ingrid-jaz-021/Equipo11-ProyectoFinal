"""Create a real Blender armature, deform weights and three baked animation clips.
Run: blender-launcher.exe --background --python build_nina_rig.py
Original OBJ/BLEND are preserved. Outputs go to Models/NinaRig.
"""
import bpy, bmesh, math, json, shutil, traceback, hashlib, struct
from pathlib import Path
from mathutils import Vector, Quaternion
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Models" / "NinaRig"
OUT.mkdir(parents=True, exist_ok=True)
def eng(v): return Vector((v[0], -v[2], v[1]))
def smooth(t):
    t=max(0.0,min(1.0,t))
    return t*t*t*(t*(t*6-15)+10)
def mix(a,b,t): return a+(b-a)*t
def reshape_shoulders(meshes):
    """Keep real upper/forearm volume; shrink the entire hand from its TRUE wrist.
    The source hand begins at x=.39, not .54: the thumb/palm must shrink too.
    """
    source=(.12,.17,.28,.39,.60)
    target=(.12,.17,.285,.39,.474)
    def longitudinal(x):
        if x<source[0]:return target[0]+(x-source[0])
        for i in range(len(source)-1):
            if x<=source[i+1]:return mix(target[i],target[i+1],(x-source[i])/(source[i+1]-source[i]))
        return target[-1]+(x-source[-1])*.40
    changed={}
    for obj in meshes:
        if obj.name!='Hand_L.003':continue
        count=0
        for v in obj.data.vertices:
            x,y,z=v.co.x,v.co.z,-v.co.y;a=abs(x);sign=1 if x>=0 else -1
            radius=mix(.95,.40,smooth((a-.37)/.04))
            v.co=eng((sign*longitudinal(a),.685+(y-.685)*radius,.004+(z-.004)*radius));count+=1
        obj.data.update();changed[obj.name]=count
    hand=[v.co for o in meshes if o.name=='Hand_L.003' for v in o.data.vertices if v.co.x>.391]
    hand_size=[max(p[i] for p in hand)-min(p[i] for p in hand) for i in range(3)]
    if max(hand_size)>.11:raise RuntimeError('Las manos siguen fuera de proporcion.')
    return {'modified_vertices':changed,'t_pose_joint_x':{'shoulder':.17,'elbow':.285,'wrist':.39,'palm_tip':.474},
        'upper_arm_length':.115,'forearm_length':.105,'arm_radial_scale':[.95,.40],
        'hand_source_wrist_x':.39,'hand_scale_xyz':.40,'hand_bounds_size_blender':hand_size,
        'shoulder_to_palm_length':.304,'previous_shoulder_to_palm_length':.235,
        'root_weights':'Chest -> UpperArm, smooth transition x=.13.. .195; original shirt has no arm weights'}
def mesh_fingerprint(obj):
    data=bytearray()
    for v in obj.data.vertices:data.extend(struct.pack('<fff',*v.co))
    for p in obj.data.polygons:
        data.extend(struct.pack('<I',len(p.vertices)))
        for index in p.vertices:data.extend(struct.pack('<I',index))
    for uv in obj.data.uv_layers.active.data:data.extend(struct.pack('<ff',*uv.uv))
    return hashlib.sha256(data).hexdigest()
def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.obj_import(filepath=str(ROOT/"Models"/"niña zocalo.obj"),
        forward_axis='NEGATIVE_Z', up_axis='Y')
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    for obj in meshes:
        bpy.context.view_layer.objects.active=obj
        obj.select_set(True)
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        obj.select_set(False)
    shirt=next(o for o in meshes if o.name=='Hand_L.002')
    original_shirt=mesh_fingerprint(shirt)
    geometry=reshape_shoulders(meshes)
    for obj in meshes:
        if obj.name in {"Hand_L.003","Hand_L.004"}:
            bm=bmesh.new();bm.from_mesh(obj.data)
            bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=2,use_grid_fill=True)
            bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
            bm.to_mesh(obj.data);bm.free();obj.data.update()
    tex=OUT/"textures"
    tex.mkdir(exist_ok=True)
    names={"Black.001":"Ojos_Boca_Niña.png","Teal":"Ojos_Boca_Niña.png",
        "Teal.001":"Textura_Pelo_Niña.png","Shoe":"Textura_PiesNiña.png",
        "white":"Calcetas_Niña.png","skin.001":"Textura_Cabeza_Niña.png",
        "skin.002":"Textura_Brazos_niña.png","skin":"Textura_PiesNiña.png",
        "Pant.002":"Textura_pantalon_niña.png","Tshirt":"Textura_Cuerpo_Niña.png"}
    aliases={}
    for n,filename in names.items():
        if filename not in aliases:
            dst=tex/("texture_%02d.png"%len(aliases))
            shutil.copy2(ROOT/"Models"/"Niña Zocalo"/filename,dst)
            aliases[filename]=dst
        material=bpy.data.materials.get(n)
        if material:
            material.use_nodes=True
            bsdf=material.node_tree.nodes.get("Principled BSDF")
            image=bpy.data.images.load(str(aliases[filename]),check_existing=True)
            node=material.node_tree.nodes.new("ShaderNodeTexImage")
            node.image=image
            material.node_tree.links.new(node.outputs["Color"],bsdf.inputs["Base Color"])
            bsdf.inputs["Roughness"].default_value=0.85
    arm=bpy.data.armatures.new("NinaSkeleton")
    rig=bpy.data.objects.new("NinaRig",arm)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active=rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    def bone(name,head,tail,parent=None,connect=False):
        b=arm.edit_bones.new(name);b.head=eng(head);b.tail=eng(tail)
        if parent: b.parent=arm.edit_bones[parent]
        b.use_connect=connect
        return b
    bone("Root",(0,0,0),(0,0.10,0))
    bone("Hips",(0,0.43,0.015),(0,0.51,0.015),"Root")
    bone("Spine",(0,0.51,0.015),(0,0.64,0.015),"Hips",True)
    bone("Chest",(0,0.64,0.015),(0,0.74,0.015),"Spine",True)
    bone("Neck",(0,0.74,0.015),(0,0.82,0.015),"Chest",True)
    bone("Head",(0,0.82,0.015),(0,1.20,0.015),"Neck",True)
    for side,sign in [("L",1),("R",-1)]:
        bone("Clavicle."+side,(sign*0.045,0.685,0.004),(sign*0.17,0.685,0.004),"Chest")
        bone("UpperArm."+side,(sign*0.17,0.685,0.004),(sign*0.285,0.685,0.004),"Clavicle."+side,True)
        bone("Forearm."+side,(sign*0.285,0.685,0.004),(sign*0.39,0.685,0.004),"UpperArm."+side,True)
        bone("Hand."+side,(sign*0.39,0.685,0.004),(sign*0.474,0.685,0.004),"Forearm."+side,True)
        bone("Thigh."+side,(sign*0.095,0.43,0.015),(sign*0.105,0.255,0.015),"Hips")
        bone("Shin."+side,(sign*0.105,0.255,0.015),(sign*0.105,0.105,0.025),"Thigh."+side,True)
        bone("Foot."+side,(sign*0.105,0.105,0.025),(sign*0.105,0.045,0.145),"Shin."+side,True)
        bone("Toe."+side,(sign*0.105,0.045,0.145),(sign*0.105,0.045,0.205),"Foot."+side,True)
    bpy.ops.object.mode_set(mode='OBJECT')
    for obj in meshes:
        for b in arm.bones: obj.vertex_groups.new(name=b.name)
        # Per-vertex material is necessary on the joined shorts/legs and sleeves.
        vertex_material={}
        for poly in obj.data.polygons:
            material=obj.data.materials[poly.material_index].name if poly.material_index<len(obj.data.materials) else ""
            for vi in poly.vertices: vertex_material.setdefault(vi,material)
        for v in obj.data.vertices:
            x,y,z=v.co.x,v.co.z,-v.co.y
            s="L" if x>=0 else "R";m=vertex_material.get(v.index,"")
            weights={}
            if obj.name=='Hand_L.002':
                # Rigid torso attachment preserves ALL shirt vertices/proportions,
                # including sleeves, during bind-pose baking and locomotion.
                weights={"Chest":1.0}
            elif m=="skin.002":
                armw=smooth((abs(x)-0.13)/0.065)
                elbow=smooth((abs(x)-0.265)/0.04)
                wrist=smooth((abs(x)-0.375)/0.03)
                weights={"Chest":1-armw,"UpperArm."+s:armw*(1-elbow),
                    "Forearm."+s:armw*elbow*(1-wrist),"Hand."+s:armw*elbow*wrist}
            elif y<0.47 and m!="Tshirt":
                leg=1-smooth((y-0.38)/0.08)
                knee=1-smooth((y-0.22)/0.07)
                foot=1-smooth((y-0.085)/0.045)
                weights={"Hips":1-leg,"Thigh."+s:leg*(1-knee),
                    "Shin."+s:leg*knee*(1-foot),"Foot."+s:leg*knee*foot}
            elif m in {"skin.001","Teal.001","Black.001","Teal","nose"} or y>0.78:
                head=smooth((y-0.74)/0.08)
                weights={"Neck":1-head,"Head":head}
            else:
                chest=smooth((y-0.53)/0.14)
                weights={"Spine":1-chest,"Chest":chest}
            total=sum(weights.values())
            for name,w in weights.items():
                if w>0.00001: obj.vertex_groups[name].add([v.index],w/total,'REPLACE')
        mod=obj.modifiers.new("NinaSkin","ARMATURE");mod.object=rig
        mod.use_deform_preserve_volume=False
        obj.parent=rig
    def rotate_world(name,axis,angle):
        pb=rig.pose.bones[name]
        local=pb.bone.matrix_local.to_3x3().inverted()@eng(axis)
        pb.rotation_mode='QUATERNION'
        pb.rotation_quaternion=Quaternion(local.normalized(),angle)
    # Bake a relaxed rest pose, so animation never needs to swing down from a T pose.
    for s,sign in [("L",1),("R",-1)]:
        rotate_world("UpperArm."+s,(0,0,1),-sign*math.radians(76))
    bpy.context.view_layer.update()
    deps=bpy.context.evaluated_depsgraph_get()
    baked={o.name:[v.co.copy() for v in o.evaluated_get(deps).data.vertices] for o in meshes}
    bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    for o in meshes:
        for v,co in zip(o.data.vertices,baked[o.name]): v.co=co
        o.data.update()
    if mesh_fingerprint(shirt)!=original_shirt:
        raise RuntimeError('La geometria/UV original de la playera fue modificada.')
    geometry['original_shirt']={'vertices':len(shirt.data.vertices),'polygons':len(shirt.data.polygons),
        'source_sha256':original_shirt,'export_sha256':mesh_fingerprint(shirt),'unchanged':True}
    import sys
    sys.path.insert(0,str(ROOT/"tools"))
    from mocap_retarget import bake_mocap
    clips=bake_mocap(rig,meshes,OUT)
    scene=bpy.context.scene
    rig.animation_data.action=bpy.data.actions["Idle"]
    scene.frame_start=0;scene.frame_end=60;scene.frame_set(0)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    for o in meshes: o.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"NinaRig.blend"),check_existing=False)
    bpy.ops.export_scene.fbx(filepath=str(OUT/"NinaRig.fbx"),use_selection=True,
        object_types={'ARMATURE','MESH'},axis_forward='-Z',axis_up='Y',
        apply_unit_scale=False,apply_scale_options='FBX_SCALE_ALL',
        add_leaf_bones=False,bake_anim=True,bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,bake_anim_use_all_actions=True,
        bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0,
        path_mode='RELATIVE',embed_textures=False,use_triangles=True)
    bpy.ops.export_scene.gltf(filepath=str(OUT/"NinaRig.glb"),export_format='GLB',
        use_selection=True,export_animations=True,export_animation_mode='ACTIONS',
        export_skins=True,export_influence_nb=4,export_rest_position_armature=True)
    ranges={}
    for name in ["Idle","Walk","Run"]:
        rig.animation_data.action=bpy.data.actions[name]
        ranges[name]={}
        for side in ["L","R"]:
            for joint,names in [("elbow",["UpperArm.","Forearm.","Hand."]),("knee",["Thigh.","Shin.","Foot."])]:
                angles=[]
                for frame in range(int(rig.animation_data.action.frame_range[1])+1):
                    scene.frame_set(frame);bpy.context.view_layer.update()
                    a,b,c=[rig.pose.bones[n+side].head.copy() for n in names]
                    angles.append(math.degrees((b-a).angle(c-b)))
                ranges[name][joint+side]={"min":min(angles),"max":max(angles)}
    (OUT/"rig-manifest.json").write_text(json.dumps({"bones":list(arm.bones.keys()),
        "clips":clips,"meshes":len(meshes),"rest_pose":"relaxed arms, baked in bind pose",
        "joint_ranges_degrees":ranges,"shoulder_geometry":geometry,"deformation_support_loops":2,"foot_contact_baked":True,"motion_source":"CMU captured motion; see mocap-retarget.json"},indent=2),encoding="utf8")
    (OUT/"rig-status.txt").write_text("OK CMU mocap",encoding="utf8")
    if (OUT/"rig-error.txt").exists():(OUT/"rig-error.txt").unlink()
try: main()
except Exception:
    (OUT/"rig-error.txt").write_text(traceback.format_exc(),encoding="utf8")
    raise
