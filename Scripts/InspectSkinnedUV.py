import bpy,json
for n in ['Swarmer','Pix','Ghoul','Fuemigo','Red']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath='D:/UnReal/TroubleSquad/ArtSource/EnemyMotion/SK_'+n+'.fbx')
 for o in bpy.context.scene.objects:
  if o.type=='MESH':
   print(n,'UV',[(u.name,[(min(v.uv[i] for v in u.data),max(v.uv[i] for v in u.data)) for i in range(2)]) for u in o.data.uv_layers],'normal',o.data.polygons[0].normal)
