import bpy
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath='D:/UnReal/TroubleSquad/ArtSource/EnemyMotion/SM_Ghoul.fbx')
for o in bpy.context.scene.objects:
 if o.type=='MESH':print(o.name,[(u.name,len(u.data),[(min(v.uv[i] for v in u.data),max(v.uv[i] for v in u.data)) for i in range(2)]) for u in o.data.uv_layers])
