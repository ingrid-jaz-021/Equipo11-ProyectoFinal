"""Export the supplied Blender axolotl with a weighted, looping greeting."""
import bpy,bmesh,math,json,traceback,sys,shutil
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Models/AxoloteEntrada'
sys.path.insert(0,str(ROOT/'tools'))
from mocap_retarget import eng,set_world,pose_matrix

def smooth(t):
 t=max(0,min(1,t));return t*t*t*(10+t*(-15+6*t))
def main():
 bpy.ops.wm.open_mainfile(filepath=str(OUT/'AjoloteEntrada.blend'))
 bpy.context.preferences.filepaths.save_version=0
 meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
 for o in meshes:
  bm=bmesh.new();bm.from_mesh(o.data)
  loose=[v for v in bm.verts if not v.link_faces]
  if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
  bm.to_mesh(o.data);bm.free();o.data.update()
 # The supplied body has UVs but no assigned skin/eye material. Restore the
 # small eye surfaces in the exported copy; their weights follow the head.
 body=next(o for o in meshes if o.name=='AjoloteHum')
 tree=BVHTree.FromPolygons([body.matrix_world@v.co for v in body.data.vertices],[list(poly.vertices) for poly in body.data.polygons])
 def front(x,z):
  hit,normal,_,_=tree.ray_cast(Vector((x,-10,z)),Vector((0,1,0)))
  if hit is None:raise RuntimeError('Missing facial surface')
  return hit
 def sphere(name,center,radii):
  bpy.ops.mesh.primitive_uv_sphere_add(segments=20,ring_count=12,location=center)
  o=bpy.context.object;o.name=name
  for v in o.data.vertices:v.co=Vector((v.co.x*radii[0],v.co.y*radii[1],v.co.z*radii[2]))
  return o
 for sign in [-1,1]:
  hit=front(sign*.64,5.43)
  meshes.append(sphere('AxoEye'+str(sign),hit+Vector((0,-.035,0)),(.17,.075,.235)))
  meshes.append(sphere('AxoHighlight'+str(sign),hit+Vector((-.045,-.105,.075)),(.047,.028,.06)))
 curve=bpy.data.curves.new('AxoSmile','CURVE');curve.dimensions='3D';curve.bevel_depth=.025;curve.bevel_resolution=2
 spline=curve.splines.new('POLY');spline.points.add(20)
 for i,cp in enumerate(spline.points):
  x=-.40+.8*i/20;z=4.85+.10*(x/.4)**2;hit=front(x,z);cp.co=(*tuple(hit+Vector((0,-.025,0))),1)
 smile=bpy.data.objects.new('AxoSmile',curve);bpy.context.collection.objects.link(smile)
 bpy.ops.object.select_all(action='DESELECT');smile.select_set(True);bpy.context.view_layer.objects.active=smile;bpy.ops.object.convert(target='MESH');meshes.append(bpy.context.object)
 points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
 floor=min(v.z for v in points);height=max(v.z for v in points)-floor
 def p(raw):return Vector((raw[0],raw[1],raw[2]-floor))/height
 materials={}
 for name,color in [('AjolotePiel',(.92,.48,.63,1)),('AjoloteBranquias',(.80,.25,.43,1)),('AjoloteRopa',(1,1,1,1)),('AjolotePlumas',(1,1,1,1)),('AxoEyes',(.012,.008,.024,1)),('AxoShine',(.98,.99,1,1))]:
  m=bpy.data.materials.new(name);m.diffuse_color=color;m.use_nodes=True
  bsdf=next((n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None) or m.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
  output=next((n for n in m.node_tree.nodes if n.type=='OUTPUT_MATERIAL'),None) or m.node_tree.nodes.new('ShaderNodeOutputMaterial')
  m.node_tree.links.new(bsdf.outputs['BSDF'],output.inputs['Surface'])
  bsdf.inputs['Base Color'].default_value=color;bsdf.inputs['Roughness'].default_value=.72
  materials[name]=m
 textures=OUT/'textures';textures.mkdir(exist_ok=True)
 def local_image(source,name):
  target=textures/name;shutil.copy2(source,target)
  image=bpy.data.images.load(str(target),check_existing=True);image.pack();return image
 feathers=local_image(ROOT/'Models/EntornoCompleto/Laguna/Xolotl/PlumasColorXolotl.png','PlumasColor.png')
 clothes=local_image(ROOT/'Models/EntornoCompleto/Laguna/Xolotl/RopaXolotlColor.png','RopaColor.png')
 def image_material(material,image):
  material.use_nodes=True
  bsdf=next((n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None)
  if bsdf is None:raise RuntimeError('Missing surface shader')
  node=next((n for n in material.node_tree.nodes if n.type=='TEX_IMAGE'),None) or material.node_tree.nodes.new('ShaderNodeTexImage')
  node.image=image;material.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color'])
  material.diffuse_color=(1,1,1,1)
 image_material(materials['AjoloteRopa'],clothes);image_material(materials['AjolotePlumas'],feathers)
 rawCoordinates={}
 for o in meshes:
  rawCoordinates[o.name]=[o.matrix_world@v.co for v in o.data.vertices]
  for v,raw in zip(o.data.vertices,rawCoordinates[o.name]):v.co=p(raw)
  o.matrix_world=Matrix.Identity(4)
  if o.name=='Plumas':
   # Keep original slots/UVs. Repair the missing relative image from the
   # matching Xolotl assets supplied with this same environment.
   for index,m in enumerate(o.data.materials):
    if m:image_material(m,feathers)
    else:o.data.materials[index]=materials['AjolotePlumas']
  elif o.name=='AjoloteHum':
   o.data.materials.append(materials['AjolotePiel']);o.data.materials.append(materials['AjoloteBranquias'])
   for poly in o.data.polygons:
    c=sum((rawCoordinates[o.name][i] for i in poly.vertices),Vector())/len(poly.vertices)
    poly.material_index=1 if abs(c.x)>1.40 and c.z>4.3 else 0
  else:
   o.data.materials.append(materials['AxoShine' if o.name.startswith('AxoHighlight') else 'AxoEyes' if o.name.startswith(('AxoEye','AxoSmile')) else 'AjoloteRopa'])
  for poly in o.data.polygons:poly.use_smooth=True
  o.data.update()
 arm=bpy.data.armatures.new('AxoloteSkeleton');rig=bpy.data.objects.new('AxoloteRig',arm);bpy.context.collection.objects.link(rig)
 bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
 def bone(name,a,b,parent=None,deform=True):
  eb=arm.edit_bones.new(name);eb.head=p(a);eb.tail=p(b);eb.use_deform=deform
  if parent:eb.parent=arm.edit_bones[parent]
  eb.align_roll(Vector((0,-1,0)))
 bone('AxoRoot',(0,0,floor),(0,0,1),'',True)
 bone('AxoChest',(0,0,2.7),(0,0,3.7),'AxoRoot')
 bone('AxoHead',(0,0,4.15),(0,0,6.3),'AxoChest')
 bone('AxoUpperArm',(1.02,-.25,3.73),(1.43,-.25,3.22),'AxoChest')
 bone('AxoForearm',(1.43,-.25,3.22),(1.93,-.32,3.70),'AxoUpperArm')
 bone('AxoHand',(1.93,-.32,3.70),(2.29,-.34,3.96),'AxoForearm')
 bone('AxoFingertip',(2.29,-.34,3.96),(2.38,-.34,4.03),'AxoHand',False)
 bpy.ops.object.mode_set(mode='OBJECT')
 def ease(a,b,x):return smooth((x-a)/(b-a))
 for o in meshes:
  for b in arm.bones:
   if b.use_deform:o.vertex_groups.new(name=b.name)
  for v,raw in zip(o.data.vertices,rawCoordinates[o.name]):
   x,y,z=raw;weights={}
   if o.name=='Plumas' or o.name.startswith(('AxoEye','AxoHighlight','AxoSmile')):weights={'AxoHead':1}
   elif o.name=='RopaAcc':
    weights={'AxoForearm':1} if x>1.38 and z>3.1 else {'AxoRoot':1}
   else:
    head=0 if x>1.40 and z<4.20 else ease(3.96,4.18,z);chest=(1-head)*ease(2.65,3.15,z)
    armWeight=ease(.88,1.28,x)*(1-head)*ease(2.85,3.07,z)
    fore=ease(1.40,1.70,x);hand=ease(1.88,2.08,x)
    weights={'AxoRoot':(1-head-chest)*(1-armWeight),'AxoChest':chest*(1-armWeight),'AxoHead':head*(1-armWeight),
     'AxoUpperArm':armWeight*(1-fore),'AxoForearm':armWeight*fore*(1-hand),'AxoHand':armWeight*hand}
   for name,w in weights.items():
    if w>.00001:o.vertex_groups[name].add([v.index],w,'REPLACE')
  m=o.modifiers.new('AxoloteSkin','ARMATURE');m.object=rig;o.parent=rig
 def point(raw):return p(raw)
 shoulder=point((1.02,-.25,3.73));a=arm.bones['AxoUpperArm'].length;b=arm.bones['AxoForearm'].length
 def ik(target):
  d=target-shoulder;length=max(abs(a-b)+.001,min(a+b-.002,d.length));d.normalize()
  pole=Vector((1,0,-.15));pole-=d*pole.dot(d);pole.normalize()
  along=(a*a-b*b+length*length)/(2*length)
  elbow=shoulder+d*along+pole*math.sqrt(max(0,a*a-along*along));wrist=shoulder+d*length
  plane=d.cross(pole).normalized()
  for name,direction in [('AxoUpperArm',elbow-shoulder),('AxoForearm',wrist-elbow)]:
   y=direction.normalized();z=(plane-y*plane.dot(y)).normalized();x=y.cross(z).normalized()
   set_world(rig,name,Matrix((x,y,z)).transposed().to_quaternion())
  return wrist
 def pose(t):
  for pb in rig.pose.bones:pb.rotation_mode='QUATERNION';pb.matrix_basis=Matrix.Identity(4)
  # Only greeting: rest -> raise -> side-to-side hand wave -> lower -> rest.
  idle=point((1.27,-.36,2.45));raised=point((1.80,-.40,4.32))
  lift=smooth((t-1.4)/1.2)*(1-smooth((t-5.2)/1.2))
  target=idle.lerp(raised,lift)
  wrist=ik(target)
  fore=pose_matrix(rig.pose.bones['AxoForearm']);direction=(fore.to_3x3()@Vector((0,1,0))).normalized()
  # Spread fingers/palm along the forearm; hand waves +-16 degrees, never a full twist.
  reference=fore.to_3x3()@arm.bones['AxoForearm'].matrix_local.to_3x3().inverted()@arm.bones['AxoHand'].matrix_local.to_3x3()
  q=(reference@Vector((0,1,0))).normalized().rotation_difference(direction)@reference.to_quaternion()
  envelope=smooth((t-2.6)/.35)*(1-smooth((t-4.85)/.35))
  wave=math.radians(16)*math.sin(2*math.pi*1.6*(t-2.6))*envelope
  set_world(rig,'AxoHand',Quaternion(Vector((0,-1,0)),wave)@q)
  return lift
 s=bpy.context.scene;s.render.fps=30;s.frame_start=1;s.frame_end=241
 rig.animation_data_create();action=bpy.data.actions.new('AxoGreeting');action.use_fake_user=True;rig.animation_data.action=action
 for f in range(241):
  s.frame_set(f+1);pose(f/30)
  for pb in rig.pose.bones:
   pb.keyframe_insert('rotation_quaternion',frame=f+1,group=pb.name)
 for layer in action.layers:
  for strip in layer.strips:
   for bag in strip.channelbags:
    for fc in bag.fcurves:
     for k in fc.keyframe_points:k.interpolation='LINEAR'
 s.frame_set(1);bpy.context.view_layer.update()
 bpy.ops.object.select_all(action='DESELECT')
 for o in meshes+[rig]:o.select_set(True)
 bpy.context.view_layer.objects.active=rig
 bpy.ops.export_scene.fbx(filepath=str(OUT/'AxoloteEntrada.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,
  bake_anim=True,bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Z',axis_up='Y',use_armature_deform_only=False,
  apply_unit_scale=False,apply_scale_options='FBX_SCALE_ALL',use_triangles=True,path_mode='COPY',embed_textures=True)
 bpy.ops.export_scene.gltf(filepath=str(OUT/'AxoloteEntrada.glb'),export_format='GLB',use_selection=True,export_animations=True,export_yup=True)
 bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AxoloteSaludo.blend'))
 (OUT/'rig-manifest.json').write_text(json.dumps({'source':'AjoloteEntrada.blend','runtime':'AxoloteEntrada.fbx','clip':'AxoGreeting','duration':8,'deformingBones':6,'wristWaveDegrees':16,'phases':['idle','raise','wave','lower','idle'],'materials':{'eyes':'head-weighted black eyes and highlights','skin':'pink skin and darker gills; source has no assigned skin material','feathers':'original UVs with recovered PlumasColorXolotl','clothes':'original UVs with RopaXolotlColor','embedded':True}},indent=2))
 (ROOT/'obj/axolote-build-ok.txt').write_text('OK')
 if (ROOT/'obj/axolote-build-error.txt').exists():(ROOT/'obj/axolote-build-error.txt').unlink()
try:main()
except Exception:
 (ROOT/'obj/axolote-build-error.txt').write_text(traceback.format_exc());raise
