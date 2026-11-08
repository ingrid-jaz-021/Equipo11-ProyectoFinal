"""CMU BVH -> Nina's existing anatomical skeleton. No synthesized gait angles."""
import math,re,json
from pathlib import Path
import bpy
from mathutils import Vector,Matrix

def eng(v):return Vector((v[0],-v[2],v[1]))

class Capture:
    def __init__(self,file):
        text=Path(file).read_text();hierarchy,motion=text.split('MOTION',1)
        tokens=re.findall(r'[{}]|[^\s{}]+',hierarchy);self.nodes=[];self.ids={};index=1;channel=0
        def parse(parent):
            nonlocal index,channel
            kind=tokens[index];index+=1
            name=tokens[index];index+=1
            if kind=='End':name=self.nodes[parent]['name']+'_End'
            node={'name':name,'parent':parent,'offset':Vector((0,0,0)),'channels':[],'start':channel}
            idx=len(self.nodes);self.nodes.append(node);self.ids[name]=idx
            assert tokens[index]=='{';index+=1
            while tokens[index]!='}':
                token=tokens[index]
                if token=='OFFSET':node['offset']=Vector(map(float,tokens[index+1:index+4]));index+=4
                elif token=='CHANNELS':
                    count=int(tokens[index+1]);node['channels']=tokens[index+2:index+2+count]
                    node['start']=channel;channel+=count;index+=2+count
                else:parse(idx)
            index+=1
        parse(-1)
        lines=motion.strip().splitlines();self.dt=float(lines[1].split(':')[1])
        self.frames=[list(map(float,line.split())) for line in lines[2:] if line.strip()]
        self.poses=[]
        for frame in self.frames:
            global_m=[]
            for node in self.nodes:
                p=node['offset'].copy();rotation=Matrix.Identity(4)
                for key,value in zip(node['channels'],frame[node['start']:]):
                    if key.endswith('position'):p['XYZ'.index(key[0])]+=value
                    else:rotation=rotation@Matrix.Rotation(math.radians(value),4,key[0])
                local=Matrix.Translation(p)@rotation
                global_m.append(local if node['parent']<0 else global_m[node['parent']]@local)
            self.poses.append({n['name']:global_m[i].translation.copy() for i,n in enumerate(self.nodes)})
    def cycle(self,running):
        pairs=[('LeftUpLeg','LeftLeg'),('LeftLeg','LeftFoot'),('RightUpLeg','RightLeg'),('RightLeg','RightFoot'),
               ('LeftArm','LeftForeArm'),('RightArm','RightForeArm')]
        features=[]
        for p in self.poses:
            features.append([(p[b]-p[a]).normalized() for a,b in pairs])
        best=None
        for period in range(55 if running else 95,min(111 if running else 155,len(features)-20)):
            for start in range(12,len(features)-period-3):
                end=start+period
                error=sum((a-b).length_squared for a,b in zip(features[start],features[end]))
                error+=.5*sum(((features[start+2][j]-features[start][j])-(features[end+2][j]-features[end][j])).length_squared for j in range(len(pairs)))
                error+=.00001*(period-(80 if running else 125))**2
                if best is None or error<best[0]:best=(error,start,end)
        return best[1],best[2]

def frame(direction,side):
    y=direction.normalized();x=side-y*side.dot(y)
    if x.length<.001:x=eng((0,0,1))-y*eng((0,0,1)).dot(y)
    x.normalize();z=x.cross(y).normalized()
    return Matrix((x,y,z)).transposed().to_quaternion()

def pose_matrix(pb):
    if pb.parent is None:return pb.bone.matrix_local@pb.matrix_basis
    return pose_matrix(pb.parent)@pb.parent.bone.matrix_local.inverted()@pb.bone.matrix_local@pb.matrix_basis

def set_world(rig,name,q):
    pb=rig.pose.bones[name]
    base=pb.bone.matrix_local if not pb.parent else pose_matrix(pb.parent)@pb.parent.bone.matrix_local.inverted()@pb.bone.matrix_local
    desired=Matrix.LocRotScale(pose_matrix(pb).translation,q,Vector((1,1,1)))
    pb.matrix_basis=base.inverted()@desired

def limit_human_arms(rig,out):
    values=list(map(float,(Path(out)/'arm-limits.txt').read_text().splitlines()[0].split()))
    back,front,inward,outward,min_flex,max_flex,clearance,hand_up,forearm_side,hand_side=values
    chest=rig.pose.bones['Chest'];rotation=pose_matrix(chest).to_3x3()@chest.bone.matrix_local.to_3x3().inverted()
    up=(rotation@eng((0,1,0))).normalized();forward=(rotation@eng((0,0,1))).normalized();side=(rotation@eng((1,0,0))).normalized()
    def redirect(name,direction):
        pb=rig.pose.bones[name];m=pose_matrix(pb);current=(m.to_3x3()@Vector((0,1,0))).normalized()
        set_world(rig,name,current.rotation_difference(direction.normalized())@m.to_quaternion())
    def elevation(d,ceiling):
        y=d.dot(up)
        if y<=ceiling:return d
        tangent=d-up*y
        if tangent.length<1e-6:tangent=forward
        return tangent.normalized()*math.sqrt(max(0,1-ceiling*ceiling))+up*ceiling
    def outside(d,sign,minimum):
        outward=side*sign;component=d.dot(outward)
        if component>=minimum:return d
        tangent=d-outward*component
        return tangent.normalized()*math.sqrt(1-minimum*minimum)+outward*minimum
    for suffix,sign in [('.L',1),('.R',-1)]:
        upper='UpperArm'+suffix;forearm='Forearm'+suffix;hand='Hand'+suffix
        shoulder=pose_matrix(rig.pose.bones[upper]).translation
        elbow=pose_matrix(rig.pose.bones[forearm]).translation
        d=(elbow-shoulder).normalized()
        pitch=max(math.radians(back),min(math.radians(front),math.atan2(d.dot(forward),-d.dot(up))))
        lateral=max(math.radians(inward),min(math.radians(outward),math.asin(max(-1,min(1,sign*d.dot(side))))))
        limited=(-up*math.cos(pitch)+forward*math.sin(pitch))*math.cos(lateral)+side*(sign*math.sin(lateral))
        redirect(upper,limited)
        elbow=pose_matrix(rig.pose.bones[forearm]).translation;wrist=pose_matrix(rig.pose.bones[hand]).translation
        u=(elbow-shoulder).normalized();v=(wrist-elbow);d=v.normalized()
        captured=math.degrees(u.angle(d))
        # Compresion suave del gesto adulto: conserva variacion, en lugar de
        # dejar todo el clip de carrera pegado al tope del codo.
        if captured>55:captured=55+(captured-55)*.60
        angle=math.radians(max(min_flex,min(max_flex,captured)))
        bend=d-u*d.dot(u)
        if bend.length<1e-6:bend=forward-u*forward.dot(u)
        d=u*math.cos(angle)+bend.normalized()*math.sin(angle)
        ceiling=max(-.98,min(.98,((shoulder-elbow).dot(up)-(elbow-shoulder).length*clearance)/v.length))
        redirect(forearm,elevation(outside(d,sign,forearm_side),ceiling))
        palm=(pose_matrix(rig.pose.bones[hand]).to_3x3()@Vector((0,1,0))).normalized()
        redirect(hand,elevation(outside(palm,sign,hand_side),hand_up))

def audit_arm_vertices(rig,meshes,out):
    selected={}
    for obj in meshes:
        groups={g.index:g.name for g in obj.vertex_groups};points=[]
        for vertex in obj.data.vertices:
            weight={'L':0,'R':0}
            for influence in vertex.groups:
                name=groups[influence.group]
                if name.startswith(('Forearm.','Hand.')):weight[name[-1]]+=influence.weight
            side=max(weight,key=weight.get)
            if weight[side]>=.5:points.append((vertex.index,side))
        if points:selected[obj.name]=points
    result={}
    for clip in ['Idle','Walk','Run']:
        action=bpy.data.actions[clip];rig.animation_data.action=action;maximum=-1e9
        for f in range(int(action.frame_range[1])+1):
            bpy.context.scene.frame_set(f);bpy.context.view_layer.update()
            chest=rig.pose.bones['Chest'];up=(pose_matrix(chest).to_3x3()@chest.bone.matrix_local.to_3x3().inverted()@eng((0,1,0))).normalized()
            shoulders={s:pose_matrix(rig.pose.bones['UpperArm.'+s]).translation for s in ['L','R']}
            deps=bpy.context.evaluated_depsgraph_get()
            for obj in meshes:
                if obj.name not in selected:continue
                evaluated=obj.evaluated_get(deps)
                for index,side in selected[obj.name]:
                    maximum=max(maximum,(evaluated.matrix_world@evaluated.data.vertices[index].co-shoulders[side]).dot(up))
        result[clip]={'max_forearm_hand_vertex_above_shoulder':maximum,'frames':int(action.frame_range[1])+1}
        if maximum>.001:raise RuntimeError('Geometria del brazo por encima del hombro: '+clip+' '+str(maximum))
    (Path(out)/'arm-safety-audit.json').write_text(json.dumps(result,indent=2))

def audit_shoulder_clearance(rig,meshes,out):
    """Intersect the DEFORMED arm surface with the torso, beyond the joint seam."""
    from mathutils.bvhtree import BVHTree
    parts=[]
    for obj in meshes:
        groups={g.index:g.name for g in obj.vertex_groups}
        arm_weights=[sum(g.weight for g in v.groups if groups[g.group].startswith(('UpperArm.','Forearm.','Hand.'))) for v in obj.data.vertices]
        obj.data.calc_loop_triangles()
        # Short original sleeves surround the joint deliberately; exclude that
        # socket, but test every distal arm surface against the original torso.
        shoulders=[b.head_local for b in rig.data.bones if b.name.startswith('UpperArm.')]
        outside_socket=[min((v.co-p).length for p in shoulders)>.08 for v in obj.data.vertices]
        if obj.name=='Hand_L.003':
            triangles=[tuple(t.vertices) for t in obj.data.loop_triangles if all(arm_weights[i]>.95 and outside_socket[i] for i in t.vertices)]
            parts.append((obj,'arm',triangles))
        elif obj.name=='Hand_L.002':
            triangles=[tuple(t.vertices) for t in obj.data.loop_triangles if all(outside_socket[i] for i in t.vertices)]
            parts.append((obj,'torso',triangles))
    if {p[1] for p in parts}!={'arm','torso'}:raise RuntimeError('Faltan las superficies de brazos/torso para auditar hombros.')
    result={}
    for clip in ['Idle','Walk','Run']:
        action=bpy.data.actions[clip];rig.animation_data.action=action;collisions=0
        for f in range(int(action.frame_range[1])+1):
            bpy.context.scene.frame_set(f);bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get();trees={}
            for obj,kind,triangles in parts:
                evaluated=obj.evaluated_get(deps)
                trees[kind]=BVHTree.FromPolygons([evaluated.matrix_world@v.co for v in evaluated.data.vertices],triangles,all_triangles=True)
            collisions+=len(trees['arm'].overlap(trees['torso']))
        result[clip]={'frames':int(action.frame_range[1])+1,'arm_torso_triangle_intersections':collisions,
            'attachment_seam_excluded':'original short sleeves/socket radius .08; arm weights .05.. .95'}
    (Path(out)/'shoulder-clearance-audit.json').write_text(json.dumps(result,indent=2))
    if any(v['arm_torso_triangle_intersections'] for v in result.values()):
        raise RuntimeError('La malla de los brazos atraviesa el torso: '+str(result))

def bake_mocap(rig,meshes,out):
    root=Path(out);scene=bpy.context.scene;scene.render.fps=60;rig.animation_data_create()
    sources={n:Capture(root/'mocap'/file) for n,file in [('Walk','07_01.bvh'),('Run','09_01.bvh'),('Idle','77_02.bvh')]}
    mappings={'Hips':('Hips','Spine'),'Spine':('Spine','Spine1'),'Chest':('Spine1','Neck1'),
        'Neck':('Neck1','Head'),'Head':('Head','Head_End')}
    for side,src in [('L','Left'),('R','Right')]:
        # Keep shoulder sockets attached to the original chest instead of
        # stretching clavicles toward the adult capture's wider shoulders.
        for dest,pair in [('UpperArm',(src+'Arm',src+'ForeArm')),
            ('Forearm',(src+'ForeArm',src+'Hand')),('Hand',(src+'Hand',src+'HandIndex1')),
            ('Thigh',(src+'UpLeg',src+'Leg')),('Shin',(src+'Leg',src+'Foot')),
            ('Foot',(src+'Foot',src+'ToeBase')),('Toe',(src+'ToeBase',src+'ToeBase_End'))]:mappings[dest+'.'+side]=pair
    bind={b.name:b.matrix_local.to_quaternion() for b in rig.data.bones}
    axes={b.name:frame(b.tail_local-b.head_local,eng((1,0,0))) for b in rig.data.bones}
    sole=[o for o in meshes if o.name.startswith('Shoe')]
    level=min(v.co.z for o in sole for v in o.data.vertices)
    clips=[];provenance={}
    for name in ['Idle','Walk','Run']:
        capture=sources[name]
        if name=='Idle':start,end=80,320
        else:start,end=capture.cycle(name=='Run')
        count=max(30,round((end-start)*capture.dt*60))
        # Heading from captured pelvis travel, so the exported clips are in-place +Z.
        travel=capture.poses[end]['Hips']-capture.poses[start]['Hips'];travel.y=0
        facing=(capture.poses[start]['LeftArm']-capture.poses[start]['RightArm']).cross(
            capture.poses[start]['Neck1']-capture.poses[start]['Hips'])
        yaw=math.atan2(travel.x,travel.z) if name!='Idle' else math.atan2(facing.x,facing.z)
        alignment=Matrix.Rotation(-yaw,3,'Y')
        offset=0 if name=='Idle' else max(range(end-start),key=lambda f:(alignment@(
            capture.poses[start+f]['LeftFoot']-capture.poses[start+f]['Hips'])).z)/(end-start)
        action=bpy.data.actions.new(name);action.use_fake_user=True;rig.animation_data.action=action
        previous={};seam=[]
        for f in range(count+1):
            t=((f/count)+offset)%1.0;source_frame=min(end,round(start+(end-start)*t));p=capture.poses[source_frame]
            # Blend to preceding frames at the seam, preserving contact velocity.
            weight=max(0,min(1,(t-.88)/.12));weight=weight*weight*(3-2*weight)
            p={key:alignment@(value-p['Hips']) for key,value in p.items()}
            p0=capture.poses[max(1,start-(end-source_frame))];p0={key:alignment@(value-p0['Hips']) for key,value in p0.items()}
            if weight:p={key:p[key].lerp(p0[key],weight) for key in p}
            side=eng(p['LeftArm']-p['RightArm']);pelvis_side=eng(p['LeftUpLeg']-p['RightUpLeg'])
            scene.frame_set(f)
            for pb in rig.pose.bones:pb.rotation_mode='QUATERNION';pb.location=(0,0,0);pb.rotation_quaternion=(1,0,0,0);pb.scale=(1,1,1)
            for bone in rig.data.bones:
                if bone.name not in mappings:continue
                a,b=mappings[bone.name];direction=eng(p[b]-p[a])
                sideways=pelvis_side if bone.name=='Hips' or bone.name.startswith(('Thigh','Shin','Foot','Toe')) else side
                desired=frame(direction,sideways)@axes[bone.name].inverted()@bind[bone.name]
                if bone.name in ['Head','Neck']:desired=bind[bone.name].slerp(desired,.65)
                set_world(rig,bone.name,desired)
            limit_human_arms(rig,out)
            # Bake actual sole contact after retargeting the adult capture onto chibi proportions.
            bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get()
            minimum=min(v.co.z for o in sole for v in o.evaluated_get(deps).data.vertices)
            rig.pose.bones['Hips'].location.y+=level-minimum;bpy.context.view_layer.update()
            for pb in rig.pose.bones:
                q=pb.rotation_quaternion
                if pb.name in previous and previous[pb.name].dot(q)<0:q.negate()
                previous[pb.name]=q.copy()
                for prop in ['location','rotation_quaternion','scale']:pb.keyframe_insert(data_path=prop,frame=f,group=pb.name)
        clips.append({'name':name,'frames':count,'seconds':count/60})
        provenance[name]={'file':'mocap/'+('07_01.bvh' if name=='Walk' else '09_01.bvh' if name=='Run' else '77_02.bvh'),
            'capture_frames':[start,end],'phase_offset':offset,'source_fps':1/capture.dt,'root_travel_source':travel.length}
    audit_arm_vertices(rig,meshes,out)
    audit_shoulder_clearance(rig,meshes,out)
    (root/'mocap-retarget.json').write_text(json.dumps(provenance,indent=2),encoding='utf8')
    return clips
