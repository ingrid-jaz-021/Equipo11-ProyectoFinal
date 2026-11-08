import bpy,json,traceback
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'obj'/'validation';OUT.mkdir(parents=True,exist_ok=True)
try:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.obj_import(filepath=str(ROOT/'Models'/'xoloitzcuintle.obj'),forward_axis='NEGATIVE_Z',up_axis='Y')
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    pts=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
    lo=Vector([min(v[i] for v in pts) for i in range(3)])
    hi=Vector([max(v[i] for v in pts) for i in range(3)])
    data={'blender_min':list(lo),'blender_max':list(hi),'engine_min':[lo.x,lo.z,-hi.y],
        'engine_max':[hi.x,hi.z,-lo.y],'objects':[{'name':o.name,'vertices':len(o.data.vertices),
        'materials':[m.name for m in o.data.materials]} for o in meshes]}
    (OUT/'xolo-inspection.json').write_text(json.dumps(data,indent=2))
    for m in bpy.data.materials:m.diffuse_color=(.32,.24,.20,1) if m.name=='Material.001' else (.03,.03,.03,1)
    scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH'
    scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
    scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
    scene.world=bpy.data.worlds.new('InspectionWorld');scene.world.color=(.12,.14,.18)
    scene.display.shading.background_type='WORLD'
    scene.render.resolution_x=640;scene.render.resolution_y=640;scene.render.resolution_percentage=100
    center=(lo+hi)*.5;radius=max(hi-lo)*2
    for name,direction in [('front',Vector((0,-1,.08))),('side',Vector((1,0,.08))),('top',Vector((0,0,1)))]:
        camera_data=bpy.data.cameras.new(name);camera=bpy.data.objects.new(name,camera_data)
        scene.collection.objects.link(camera);camera.location=center+direction.normalized()*radius
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        camera_data.type='ORTHO';camera_data.ortho_scale=max(hi-lo)*1.12
        scene.camera=camera;scene.render.filepath=str(OUT/('xolo-'+name+'.png'));bpy.ops.render.render(write_still=True)
    (OUT/'xolo-inspection-done.txt').write_text('OK')
except Exception:
    (OUT/'xolo-inspection-error.txt').write_text(traceback.format_exc());raise
