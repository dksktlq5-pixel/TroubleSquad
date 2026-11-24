import bpy,json,pathlib
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/EnemyMotion');out={}
for src in root.glob('SM_*.fbx'):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(src))
 rows=[]
 for o in bpy.context.scene.objects:
  if o.type!='MESH':continue
  uvs=[x.uv for x in o.data.uv_layers.active.data] if o.data.uv_layers.active else []
  rows.append({'name':o.name,'vertices':len(o.data.vertices),'uvs':len(uvs),'range':[[min(u[i] for u in uvs),max(u[i] for u in uvs)] for i in range(2)] if uvs else []})
 out[src.stem]=rows
with open(str(root/'UvAudit.json'),'w') as f:json.dump(out,f)
