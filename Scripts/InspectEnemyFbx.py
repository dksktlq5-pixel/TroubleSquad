import bpy,json
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath='D:/UnReal/TroubleSquad/ArtSource/EnemyMotion/SM_Ghoul.fbx')
for o in bpy.context.scene.objects:
 if o.type=='MESH':
  co=[o.matrix_world@v.co for v in o.data.vertices]
  print('BOUNDS',[(min(v[i] for v in co),max(v[i] for v in co)) for i in range(3)],'matrix',o.matrix_world)
