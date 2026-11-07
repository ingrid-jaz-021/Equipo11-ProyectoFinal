import bpy, json, math
from pathlib import Path
rig=next(o for o in bpy.data.objects if o.type=="ARMATURE")
result={}
for name in ["Idle","Walk","Run"]:
 action=bpy.data.actions.get(name)
 rig.animation_data.action=action
 data={}
 for side in ["L","R"]:
  for joint,bones in [("elbow",["UpperArm.","Forearm.","Hand."]),("knee",["Thigh.","Shin.","Foot."])]:
   values=[]
   for frame in range(int(action.frame_range[0]),int(action.frame_range[1])+1):
    bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()
    a,b,c=[rig.pose.bones[n+side].head.copy() for n in bones]
    values.append(math.degrees((b-a).angle(c-b)))
   data[joint+side]={"min":min(values),"max":max(values)}
 result[name]=data
result["weights"]={}
for obj in bpy.data.objects:
 if obj.type!="MESH":continue
 for g in obj.vertex_groups:
  if "Shin" in g.name or "Forearm" in g.name:
   weights=[v.groups[i].weight for v in obj.data.vertices for i in range(len(v.groups)) if v.groups[i].group==g.index]
   if weights:result["weights"][obj.name+"/"+g.name]={"count":len(weights),"max":max(weights)}
Path("C:/Users/hugo1/Desktop/Proyecto_Equipo11/Equipo11-ProyectoFinal/Proyecto/obj/validation/joint-audit-before.json").write_text(json.dumps(result,indent=2),encoding="utf8")
