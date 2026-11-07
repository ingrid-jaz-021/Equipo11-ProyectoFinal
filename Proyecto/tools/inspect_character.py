import bpy, json, pathlib
result={"objects":[{"name":o.name,"type":o.type,"vertices":len(o.data.vertices) if o.type=="MESH" else 0,"groups":list(o.vertex_groups.keys()) if o.type=="MESH" else [],"bones":list(o.data.bones.keys()) if o.type=="ARMATURE" else []} for o in bpy.data.objects],"actions":[{"name":a.name,"range":list(a.frame_range)} for a in bpy.data.actions]}
pathlib.Path("C:/Users/hugo1/Desktop/Proyecto_Equipo11/Equipo11-ProyectoFinal/Proyecto/obj/validation/blend-inspection.json").write_text(json.dumps(result,indent=2),encoding="utf8")
