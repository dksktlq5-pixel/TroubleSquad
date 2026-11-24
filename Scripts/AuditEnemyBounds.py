import unreal,json
out={}
for path in unreal.EditorAssetLibrary.list_assets('/Game/TroubleSquad/Enemies'):
 if '/SM_' not in path:continue
 mesh=unreal.load_asset(path)
 if not isinstance(mesh,unreal.StaticMesh):continue
 b=mesh.get_bounds();out[mesh.get_name()]={'origin':[b.origin.x,b.origin.y,b.origin.z],'extent':[b.box_extent.x,b.box_extent.y,b.box_extent.z]}
with open('D:/UnReal/TroubleSquad/Saved/Logs/EnemyBounds.json','w') as f:json.dump(out,f)
refs=[('T_Arena_Outskirts','C:/Users/cksdn/AppData/Local/Temp/codex-clipboard-2139cb39-1b3e-48c3-82f0-f743dd79e179.png')]
unreal.log('ENEMY_BOUNDS_DONE')
