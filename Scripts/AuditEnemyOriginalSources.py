import unreal,json
out={}
for path in unreal.EditorAssetLibrary.list_assets('/Game/TroubleSquad/Enemies',recursive=False):
 a=unreal.load_asset(path)
 if isinstance(a,unreal.StaticMesh):
  data=a.get_editor_property('asset_import_data');out[a.get_name()]=data.get_first_filename() if data else ''
with open('D:/UnReal/TroubleSquad/Saved/Logs/EnemyOriginalSources.json','w') as f:json.dump(out,f)
