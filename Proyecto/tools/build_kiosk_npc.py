"""Normalize the supplied NPC, skin it, and bake seated eating clips for Assimp."""
import bpy,bmesh,math,json,shutil,traceback,sys
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Models/KioskNpcRig';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'tools'))
from mocap_retarget import eng,set_world,pose_matrix

def smooth(t):
    t=max(0,min(1,t));return t*t*t*(t*(t*6-15)+10)
def blend(a,b,t):return Vector(a).lerp(Vector(b),smooth(t))
def orient(rig,name,direction,up=(0,1,0)):
    y=eng(direction).normalized();z=eng(up);z-=y*z.dot(y)
    if z.length<.01:z=eng((0,0,1))-y*eng((0,0,1)).dot(y)
    z.normalize();x=y.cross(z).normalized()
    set_world(rig,name,Matrix((x,y,z)).transposed().to_quaternion())
def ik(rig,upper,lower,tip,target,hint):
    a=rig.data.bones[upper].length;b=rig.data.bones[lower].length
    origin=pose_matrix(rig.pose.bones[upper]).translation
    delta=eng(target)-origin;distance=max(abs(a-b)+.001,min(a+b-.001,delta.length));d=delta.normalized()
    bend=eng(hint);bend-=d*bend.dot(d);bend.normalize()
    along=(a*a-b*b+distance*distance)/(2*distance)
    elbow=origin+d*along+bend*math.sqrt(max(0,a*a-along*along))
    wrist=origin+d*distance
    def direct(name,v):
        m=pose_matrix(rig.pose.bones[name]);current=m.to_3x3()@Vector((0,1,0))
        set_world(rig,name,current.normalized().rotation_difference(v.normalized())@m.to_quaternion())
    direct(upper,elbow-origin);direct(lower,wrist-elbow)

def main():
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.obj_import(filepath=str(ROOT/'Models/Persona Kiosko/PersonaZoc.obj'),forward_axis='NEGATIVE_Z',up_axis='Y')
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
    raw=[Vector((p.x,p.z,-p.y)) for p in points]
    lo=Vector([min(p[i] for p in raw) for i in range(3)]);hi=Vector([max(p[i] for p in raw) for i in range(3)])
    height=hi.y-lo.y;cx=(lo.x+hi.x)*.5;cz=.007
    for obj in meshes:
        for v in obj.data.vertices:
            p=obj.matrix_world@v.co;p=Vector((p.x,p.z,-p.y))
            v.co=eng(((p.x-cx)/height,(p.y-lo.y)/height,(p.z-cz)/height))
        obj.matrix_world=Matrix.Identity(4)
        if obj.name in {'Male','Male.001','Male.004'}:
            bm=bmesh.new();bm.from_mesh(obj.data)
            bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=1,use_grid_fill=True)
            bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
        obj.data.update()
    textures=OUT/'textures';textures.mkdir(exist_ok=True)
    for material,file in [('Material','Textura_gorra.png'),('Material.001','Textura_playera.png'),('Material.002','Textura_pantalon.png'),('Material.003','Textura_zapatos.png')]:
        dst=textures/file;shutil.copy2(ROOT/'Models/Persona Kiosko'/file,dst)
        mat=bpy.data.materials[material];mat.use_nodes=True;bsdf=mat.node_tree.nodes.get('Principled BSDF')
        image=mat.node_tree.nodes.new('ShaderNodeTexImage');image.image=bpy.data.images.load(str(dst),check_existing=True)
        mat.node_tree.links.new(image.outputs['Color'],bsdf.inputs['Base Color']);bsdf.inputs['Roughness'].default_value=.85
    skin=bpy.data.materials['Material.004'];skin.diffuse_color=(.62,.37,.22,1);skin.use_nodes=True
    skin.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=skin.diffuse_color
    arm=bpy.data.armatures.new('KioskSkeleton');rig=bpy.data.objects.new('KioskNpcRig',arm);bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active=rig;rig.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
    def bone(name,a,b,parent=None,deform=True):
        eb=arm.edit_bones.new(name);eb.head=eng(a);eb.tail=eng(b);eb.use_deform=deform
        if parent:eb.parent=arm.edit_bones[parent]
        eb.align_roll(eng((0,1,0)));return eb
    bone('NpcRoot',(0,0,0),(0,.08,0),deform=False)
    bone('NpcHips',(0,.515,0),(0,.59,0),'NpcRoot')
    bone('NpcSpine',(0,.59,0),(0,.70,0),'NpcHips')
    bone('NpcChest',(0,.70,0),(0,.80,0),'NpcSpine')
    bone('NpcNeck',(0,.80,0),(0,.85,0),'NpcChest')
    bone('NpcHead',(0,.85,0),(0,.99,0),'NpcNeck')
    bone('NpcSeatAnchor',(0,.485,-.015),(0,.515,-.015),'NpcRoot',False)
    bone('NpcScaleBottom',(0,0,0),(0,.02,0),'NpcRoot',False)
    bone('NpcScaleTop',(0,1,0),(0,1.02,0),'NpcRoot',False)
    for side,sign in [('L',1),('R',-1)]:
        bone('NpcClavicle.'+side,(sign*.035,.78,-.02),(sign*.12,.78,-.02),'NpcChest')
        bone('NpcUpperArm.'+side,(sign*.12,.78,-.02),(sign*.245,.725,-.02),'NpcClavicle.'+side)
        bone('NpcForearm.'+side,(sign*.245,.725,-.02),(sign*.34,.692,0),'NpcUpperArm.'+side)
        bone('NpcHand.'+side,(sign*.34,.692,0),(sign*.43,.675,.015),'NpcForearm.'+side)
        bone('NpcThigh.'+side,(sign*.06,.515,0),(sign*.065,.278,.005),'NpcHips')
        bone('NpcShin.'+side,(sign*.065,.278,.005),(sign*.065,.06,.01),'NpcThigh.'+side)
        bone('NpcFoot.'+side,(sign*.065,.06,.01),(sign*.065,.025,.09),'NpcShin.'+side)
    bpy.ops.object.mode_set(mode='OBJECT')
    for obj in meshes:
        for b in arm.bones:
            if b.use_deform:obj.vertex_groups.new(name=b.name)
        for v in obj.data.vertices:
            x,y=v.co.x,v.co.z;side='L' if x>=0 else 'R';a=abs(x)
            if obj.name=='Male.002' or y>.855:weights={'NpcHead':1}
            elif y>.75 and a<.07:weights={'NpcNeck':1}
            elif y>.64 and a>.10:
                armw=smooth((a-.095)/.07);elbow=smooth((a-.22)/.055);wrist=smooth((a-.315)/.05)
                weights={'NpcChest':1-armw,'NpcUpperArm.'+side:armw*(1-elbow),'NpcForearm.'+side:armw*elbow*(1-wrist),'NpcHand.'+side:armw*elbow*wrist}
            elif y<.56:
                leg=1-smooth((y-.49)/.07);knee=1-smooth((y-.25)/.055);foot=1-smooth((y-.055)/.04)
                weights={'NpcHips':1-leg,'NpcThigh.'+side:leg*(1-knee),'NpcShin.'+side:leg*knee*(1-foot),'NpcFoot.'+side:leg*knee*foot}
            else:
                chest=smooth((y-.59)/.12);weights={'NpcSpine':1-chest,'NpcChest':chest}
            for name,w in weights.items():
                if w>1e-6:obj.vertex_groups[name].add([v.index],w,'REPLACE')
        mod=obj.modifiers.new('NpcSkin','ARMATURE');mod.object=rig;obj.parent=rig
    scene=bpy.context.scene;scene.render.fps=60;rig.animation_data_create()
    rest=Vector((-.14,.565,.10));pocket=Vector((-.105,.59,.055));mouth=Vector((-.06,.84,.085))
    clips={'Idle':3.0,'ReachPocket':1.2,'Appear':.6,'RaiseBread':1.2,'Eat':3.3,'ReturnHand':1.4}
    audit={}
    for name,seconds in clips.items():
        action=bpy.data.actions.new(name);action.use_fake_user=True;rig.animation_data.action=action
        count=round(seconds*60);hand_positions=[]
        for f in range(count+1):
            t=f/count;scene.frame_set(f)
            for pb in rig.pose.bones:pb.rotation_mode='QUATERNION';pb.matrix_basis=Matrix.Identity(4)
            # Hips stay in their original frame; thighs point forward and shins
            # vertically down. No whole-body tilting to fake the sitting pose.
            for side,sign in [('L',1),('R',-1)]:
                orient(rig,'NpcThigh.'+side,(sign*.05,-.10,1))
                orient(rig,'NpcShin.'+side,(0,-1,.03),(0,0,1))
                orient(rig,'NpcFoot.'+side,(0,-.35,.94))
            right=rest if name=='Idle' else blend(rest,pocket,t) if name=='ReachPocket' else pocket if name=='Appear' else blend(pocket,mouth,t) if name=='RaiseBread' else mouth if name=='Eat' else blend(mouth,rest,t)
            ik(rig,'NpcUpperArm.R','NpcForearm.R','NpcHand.R',right,(-1,-.2,0))
            ik(rig,'NpcUpperArm.L','NpcForearm.L','NpcHand.L',(.14,.565,.12),(1,-.2,0))
            orient(rig,'NpcHand.R',(0,-.05,1));orient(rig,'NpcHand.L',(0,-.1,1))
            if name=='Eat':
                pb=rig.pose.bones['NpcHead'];pb.rotation_quaternion.rotate(__import__('mathutils').Quaternion((1,0,0),.018*math.sin(t*math.pi*8)))
            bpy.context.view_layer.update()
            hand_positions.append(list(pose_matrix(rig.pose.bones['NpcHand.R']).translation))
            for pb in rig.pose.bones:
                for prop in ['location','rotation_quaternion','scale']:pb.keyframe_insert(data_path=prop,frame=f,group=pb.name)
        audit[name]={'seconds':seconds,'frames':count+1,'hand_first_blender':hand_positions[0],'hand_last_blender':hand_positions[-1]}
    rig.animation_data.action=bpy.data.actions['Idle'];scene.frame_set(0);scene.frame_start=0;scene.frame_end=180
    bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get()
    posed=[v.co.copy() for o in meshes for v in o.evaluated_get(deps).data.vertices]
    if any(not math.isfinite(c) or abs(c)>3 for p in posed for c in p):raise RuntimeError('Deformacion no finita en el NPC.')
    if min(p.z for p in posed)>=.48:raise RuntimeError('Las piernas no bajan desde la banca.')
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
    for obj in meshes:obj.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'KioskNpcRig.blend'),check_existing=False)
    bpy.ops.export_scene.fbx(filepath=str(OUT/'KioskNpcRig.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},axis_forward='-Z',axis_up='Y',
        apply_unit_scale=False,apply_scale_options='FBX_SCALE_ALL',add_leaf_bones=False,bake_anim=True,bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,bake_anim_use_all_actions=True,bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0,path_mode='RELATIVE',use_triangles=True)
    (OUT/'rig-manifest.json').write_text(json.dumps({'clips':audit,'bones':list(arm.bones.keys()),'source_height':height,
        'source_center_x':cx,'seat_anchor':[0,.485,-.015],'normalized_standing_height':1,'posed_floor':min(p.z for p in posed)},indent=2))
    (OUT/'rig-status.txt').write_text('OK seated NPC with six clips')
    if (OUT/'rig-error.txt').exists():(OUT/'rig-error.txt').unlink()
try:main()
except Exception:
    (OUT/'rig-error.txt').write_text(traceback.format_exc());raise
