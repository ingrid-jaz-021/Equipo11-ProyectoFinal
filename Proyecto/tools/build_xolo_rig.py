"""Rig quadruped, smooth anatomical weights, baked locomotion + Sniff/Spin/Dash keyframes."""
import bpy,math,json,shutil,traceback,sys
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Models'/'XoloRig';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'tools'))
from mocap_retarget import eng,set_world,pose_matrix

def smooth(t):t=max(0,min(1,t));return t*t*t*(t*(t*6-15)+10)
def mix(a,b,t):return a+(b-a)*t
def keys(t,points):
    for (a,x),(b,y) in zip(points,points[1:]):
        if t<=b:return mix(x,y,smooth((t-a)/(b-a)))
    return points[-1][1]
def distance(p,a,b):
    d=b-a;t=max(0,min(1,(p-a).dot(d)/max(d.length_squared,1e-8)))
    return(p-a-d*t).length

def turn_angle(t):return 360*smooth((t-1.65)/1.05)
def turn_weight(t):return smooth((t-1.65)/.12)*smooth((2.70-t)/.12)
def turn_foot(t,offset):
    angle=turn_angle(t);step=angle/70+offset;index=math.floor(step);phase=step-index
    planted=max(0,(index-offset)*70);next_angle=max(0,(index+1-offset)*70)
    swing=max(0,min(1,(phase-.55)/.45));weight=turn_weight(t)
    foot_angle=mix(planted,next_angle,smooth(swing))
    return mix(angle,foot_angle,weight),math.sin(math.pi*swing)**2*.06*weight

def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.obj_import(filepath=str(ROOT/'Models'/'xoloitzcuintle.obj'),forward_axis='NEGATIVE_Z',up_axis='Y')
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    allp=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
    raw=[Vector((p.x,p.z,-p.y)) for p in allp]
    lo=Vector([min(p[i] for p in raw) for i in range(3)]);hi=Vector([max(p[i] for p in raw) for i in range(3)])
    h=hi.y-lo.y;cx=(lo.x+hi.x)*.5;cz=(lo.z+hi.z)*.5
    for o in meshes:
        for v in o.data.vertices:
            p=o.matrix_world@v.co;p=Vector((p.x,p.z,-p.y))
            v.co=eng(((cz-p.z)/h,(p.y-lo.y)/h,(p.x-cx)/h))
        o.matrix_world=Matrix.Identity(4);o.data.update()
    (OUT/'textures').mkdir(exist_ok=True)
    for material,source,dest in [('Material.001','xolo_piel (2).png','xolo_skin.png'),('Material.002','Nariz y ojos.jpg','xolo_features.jpg')]:
        shutil.copy2(ROOT/'Models'/source,OUT/'textures'/dest)
        m=bpy.data.materials[material];m.use_nodes=True
        nodes=m.node_tree.nodes;bsdf=nodes.get('Principled BSDF')
        image=nodes.new('ShaderNodeTexImage');image.image=bpy.data.images.load(str(OUT/'textures'/dest),check_existing=True)
        m.node_tree.links.new(image.outputs['Color'],bsdf.inputs['Base Color']);bsdf.inputs['Roughness'].default_value=.75
    arm=bpy.data.armatures.new('XoloSkeleton');rig=bpy.data.objects.new('XoloRig',arm)
    bpy.context.collection.objects.link(rig);bpy.context.view_layer.objects.active=rig;rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    definitions={}
    def bone(name,a,b,parent=None):
        eb=arm.edit_bones.new(name);eb.head=eng(a);eb.tail=eng(b)
        if parent:eb.parent=arm.edit_bones[parent]
        definitions[name]=(Vector(a),Vector(b))
    bone('Root',(0,0,0),(0,.1,0))
    bone('Hips',(0,.55,-.30),(0,.57,-.08),'Root')
    bone('Spine',(0,.57,-.08),(0,.58,.17),'Hips')
    bone('Chest',(0,.58,.17),(0,.59,.30),'Spine')
    bone('Neck',(0,.59,.30),(0,.76,.43),'Chest')
    bone('Head',(0,.76,.43),(0,.71,.67),'Neck')
    bone('Tail.01',(0,.60,-.39),(0,.44,-.52),'Hips')
    bone('Tail.02',(0,.44,-.52),(0,.29,-.64),'Tail.01')
    bone('Tail.03',(0,.29,-.64),(0,.27,-.71),'Tail.02')
    for side,sign in [('L',1),('R',-1)]:
        x=.105*sign
        bone('FrontUpper.'+side,(x,.55,.26),(x,.32,.25),'Chest')
        bone('FrontLower.'+side,(x,.32,.25),(x,.07,.27),'FrontUpper.'+side)
        bone('FrontPaw.'+side,(x,.07,.27),(x,.025,.36),'FrontLower.'+side)
        bone('HindUpper.'+side,(x,.54,-.33),(x,.34,-.24),'Hips')
        bone('HindLower.'+side,(x,.34,-.24),(x,.13,-.43),'HindUpper.'+side)
        bone('HindFoot.'+side,(x,.13,-.43),(x,.055,-.40),'HindLower.'+side)
        bone('HindPaw.'+side,(x,.055,-.40),(x,.025,-.33),'HindFoot.'+side)
        bone('Ear.'+side,(sign*.075,.86,.47),(sign*.08,1,.46),'Head')
    bpy.ops.object.mode_set(mode='OBJECT')
    paw_indices={};nose_indices={}
    for o in meshes:
        for b in arm.bones:o.vertex_groups.new(name=b.name)
        paw_indices[o.name]=[];nose_indices[o.name]=set()
        for poly in o.data.polygons:
            if o.data.materials[poly.material_index].name=='Material.002':nose_indices[o.name].update(poly.vertices)
        for v in o.data.vertices:
            p=Vector((v.co.x,v.co.z,-v.co.y));x,y,z=p;side='L' if x>=0 else 'R'
            if y<.08:paw_indices[o.name].append(v.index)
            if y>.86 and z>.30:
                w=smooth((y-.86)/.10);weights={'Head':1-w,'Ear.'+side:w}
            elif z>.36 and y>.57:
                w=smooth((z-.32)/.15);weights={'Neck':1-w,'Head':w}
            elif z<-.43 and (abs(x)<.07 or z<-.54) and y>.18:
                names=['Tail.01','Tail.02','Tail.03'];weights={n:math.exp(-distance(p,*definitions[n])**2/(2*.035**2)) for n in names}
            elif y<.47:
                names=['FrontUpper.','FrontLower.','FrontPaw.'] if z>0 else ['HindUpper.','HindLower.','HindFoot.','HindPaw.']
                weights={n+side:math.exp(-distance(p,*definitions[n+side])**2/(2*.035**2)) for n in names}
                body=smooth((y-.40)/.10)
                weights={n:w*(1-body) for n,w in weights.items()};weights['Chest' if z>0 else 'Hips']=body
            else:
                chest=smooth((z+.14)/.44);neck=smooth((z-.22)/.18)*smooth((y-.48)/.19)
                weights={'Hips':(1-chest)*.55,'Spine':(1-chest)*.45,'Chest':chest*(1-neck),'Neck':chest*neck}
            weights=sorted(weights.items(),key=lambda pair:pair[1],reverse=True)[:4];total=sum(w for n,w in weights)
            if total<1e-12:weights=[('Hips',1)];total=1
            for name,w in weights:
                if w/total>1e-6:o.vertex_groups[name].add([v.index],w/total,'REPLACE')
        mod=o.modifiers.new('XoloSkin','ARMATURE');mod.object=rig;o.parent=rig
    bind={b.name:b.matrix_local.to_quaternion() for b in arm.bones}
    def local_rotate(name,axis,angle):
        pb=rig.pose.bones[name];local=pb.bone.matrix_local.to_3x3().inverted()@eng(axis)
        pb.rotation_quaternion=Quaternion(local.normalized(),angle)
    def root_rotation():return pose_matrix(rig.pose.bones['Root']).to_quaternion()@bind['Root'].inverted()
    def align(name,direction):
        q=root_rotation();a,b=definitions[name];rest=q@eng(b-a)
        set_world(rig,name,rest.rotation_difference(direction.normalized())@q@bind[name])
    def ik(upper,lower,target,pole,foot_angle=None):
        pb=rig.pose.bones[upper];base=pose_matrix(pb).translation
        rootq=root_rotation();footq=rootq if foot_angle is None else Quaternion(eng((0,1,0)),math.radians(foot_angle))
        target=footq@eng(target)
        a=(definitions[upper][1]-definitions[upper][0]).length;b=(definitions[lower][1]-definitions[lower][0]).length
        delta=target-base;d=max(.001,min(a+b-.0001,delta.length));direction=delta.normalized()
        bend=rootq@eng(pole);bend=(bend-direction*bend.dot(direction)).normalized()
        cosine=max(-1,min(1,(a*a+d*d-b*b)/(2*a*d)))
        elbow=base+direction*(a*cosine)+bend*(a*math.sqrt(max(0,1-cosine*cosine)))
        endpoint=base+direction*d;align(upper,elbow-base);align(lower,endpoint-elbow)
    scene=bpy.context.scene;scene.render.fps=60;rig.animation_data_create();clips=[];sniff_heights=[];turn_samples=[]
    for name,count in [('Idle',120),('Walk',72),('Run',42),('Special',228)]:
        action=bpy.data.actions.new(name);action.use_fake_user=True;rig.animation_data.action=action;previous={}
        for f in range(count+1):
            scene.frame_set(f);t=f/60;phase=f/count;run=1 if name=='Run' else 0
            for pb in rig.pose.bones:pb.rotation_mode='QUATERNION';pb.location=(0,0,0);pb.rotation_quaternion=(1,0,0,0);pb.scale=(1,1,1)
            crouch=0;neck=0;head=0;spin=0;locomotion=name in ['Walk','Run']
            if name=='Special':
                crouch=keys(t,[(0,0),(.50,-.11),(1.25,-.11),(1.60,0),(3.8,0)])-.045*turn_weight(t)
                neck=keys(t,[(0,0),(.55,100),(1.20,100),(1.60,0),(3.8,0)])
                head=keys(t,[(0,0),(.55,80),(.80,77),(1,83),(1.25,80),(1.60,0),(3.8,0)])
                spin=turn_angle(t)
                locomotion=2.70<t<3.60;run=1;phase=(t-2.70)/.70
                set_world(rig,'Root',Quaternion(eng((0,1,0)),math.radians(spin))@bind['Root'])
            move_weight=keys(t,[(0,0),(2.70,0),(2.82,1),(3.30,1),(3.60,0),(3.8,0)]) if name=='Special' else 1
            bob=.010*math.sin(phase*4*math.pi)*move_weight if locomotion else .002*math.sin(t*math.pi)
            rig.pose.bones['Hips'].location=rig.data.bones['Hips'].matrix_local.to_3x3().inverted()@eng((0,crouch+bob,0))
            chest_yaw=head_yaw=spin
            if name=='Special':
                # Anticipacion independiente; el torso arrastra la cadera despues
                # de mirar. Son rotaciones reales de huesos, no otro giro global.
                anticipation=keys(t,[(0,0),(1.42,0),(1.62,30),(1.77,24),(1.96,0),(3.8,0)])
                chest_yaw=turn_angle(t+.015)+keys(t,[(0,0),(1.54,0),(1.70,8),(1.90,0),(3.8,0)])
                hips_yaw=turn_angle(t-.018)
                head_yaw=max(chest_yaw-35,min(chest_yaw+35,turn_angle(t+.05)+anticipation))
                for name_bone,yaw,lean in [('Hips',hips_yaw,-3),('Spine',mix(hips_yaw,chest_yaw,.5),-2),('Chest',chest_yaw,-1)]:
                    set_world(rig,name_bone,Quaternion(eng((0,1,0)),math.radians(yaw))@
                        Quaternion(eng((0,0,1)),math.radians(lean)*turn_weight(t))@bind[name_bone])
            for side,offset in [('L',0),('R',.5)]:
                for front in [True,False]:
                    cycle=(phase+offset+(0 if front else (.5 if run else .25)))%1
                    reach=(.11 if not run else .19)*move_weight;duty=.66 if not run else .45
                    if not locomotion:shift=0;lift=0
                    elif cycle<duty:shift=mix(reach,-reach,cycle/duty);lift=0
                    else:
                        u=(cycle-duty)/(1-duty);shift=mix(-reach,reach,smooth(u));lift=math.sin(math.pi*u)**2*(.075 if not run else .14)*move_weight
                    prefix='Front' if front else 'Hind';upper=prefix+'Upper.'+side;lower=prefix+'Lower.'+side
                    endpoint=definitions[lower][1].copy();endpoint.y+=lift;endpoint.z+=shift
                    foot_angle=None
                    if name=='Special' and 1.65<t<2.70:
                        offset_turn={('L',True):0,('R',False):.25,('R',True):.5,('L',False):.75}[(side,front)]
                        foot_angle,step_lift=turn_foot(t,offset_turn);endpoint.y+=step_lift
                    ik(upper,lower,endpoint,(0,0,-1 if front else 1),foot_angle)
                    q=root_rotation()
                    if foot_angle is not None:q=Quaternion(eng((0,1,0)),math.radians(foot_angle))
                    for paw in (['FrontPaw.'] if front else ['HindFoot.','HindPaw.']):set_world(rig,paw+side,q@bind[paw+side])
                local_rotate('Ear.'+side,(0,0,1),math.radians(1.5)*math.sin(phase*4*math.pi+offset))
            q=root_rotation()
            neck_yaw=mix(chest_yaw,head_yaw,.55)
            set_world(rig,'Neck',Quaternion(eng((0,1,0)),math.radians(neck_yaw))@Quaternion(eng((1,0,0)),math.radians(neck))@bind['Neck'])
            set_world(rig,'Head',Quaternion(eng((0,1,0)),math.radians(head_yaw))@Quaternion(eng((1,0,0)),math.radians(head))@bind['Head'])
            wag=t*5 if name=='Special' else 2*math.pi*f/count*(2 if name=='Idle' else 1)
            for i in range(1,4):local_rotate('Tail.%02d'%i,(0,1,0),math.radians(5+i*2)*math.sin(wag-i*.4))
            bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get()
            minimum=min(o.evaluated_get(deps).data.vertices[i].co.z for o in meshes for i in paw_indices[o.name])
            rig.pose.bones['Hips'].location+=rig.data.bones['Hips'].matrix_local.to_3x3().inverted()@Vector((0,0,-minimum))
            bpy.context.view_layer.update()
            if name=='Special' and 1.55<t<2.75 and f%3==0:
                turn_samples.append({'time':t,'root':spin,'chest':chest_yaw,'head':head_yaw,
                    'front_paw_y':pose_matrix(rig.pose.bones['FrontPaw.L']).translation.z,
                    'hind_paw_y':pose_matrix(rig.pose.bones['HindPaw.R']).translation.z})
            if name=='Special' and .6<t<1.25:
                deps=bpy.context.evaluated_depsgraph_get()
                sniff_heights.append(min(o.evaluated_get(deps).data.vertices[i].co.z for o in meshes for i in nose_indices[o.name]))
            for pb in rig.pose.bones:
                if pb.name in previous and previous[pb.name].dot(pb.rotation_quaternion)<0:pb.rotation_quaternion.negate()
                previous[pb.name]=pb.rotation_quaternion.copy()
                for prop in ['location','rotation_quaternion','scale']:pb.keyframe_insert(data_path=prop,frame=f,group=pb.name)
        clips.append({'name':name,'frames':count,'seconds':count/60})
    rig.animation_data.action=bpy.data.actions['Idle'];scene.frame_set(0);scene.frame_start=0;scene.frame_end=228
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
    for o in meshes:o.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'XoloRig.blend'),check_existing=False)
    bpy.ops.export_scene.fbx(filepath=str(OUT/'XoloRig.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},
        axis_forward='-Z',axis_up='Y',apply_unit_scale=False,apply_scale_options='FBX_SCALE_ALL',add_leaf_bones=False,
        bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=True,
        bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0,path_mode='RELATIVE',use_triangles=True)
    bpy.ops.export_scene.gltf(filepath=str(OUT/'XoloRig.glb'),export_format='GLB',use_selection=True,
        export_animations=True,export_animation_mode='ACTIONS',export_skins=True,export_influence_nb=4)
    (OUT/'rig-manifest.json').write_text(json.dumps({'bones':list(arm.bones.keys()),'clips':clips,
        'normalized_height':1,'forward':'+Z','max_weights':4,'sniff_nose_height':{'min':min(sniff_heights),'max':max(sniff_heights)},
        'special_phases':{'sniff':[0,1.6],'anticipation':[1.42,1.65],'spin':[1.65,2.70],'dash':[2.70,3.8]},
        'progressive_turn':turn_samples},indent=2))
    (OUT/'rig-status.txt').write_text('OK')
    if(OUT/'rig-error.txt').exists():(OUT/'rig-error.txt').unlink()
try:main()
except Exception:(OUT/'rig-error.txt').write_text(traceback.format_exc());raise
