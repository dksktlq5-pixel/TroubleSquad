import unreal,json
r=[]
for p in unreal.EditorAssetLibrary.list_assets('/Game/TroubleSquad/Enemies',recursive=False):
 a=unreal.load_asset(p)
 if isinstance(a,unreal.Texture2D):r.append({'name':a.get_name(),'compression':str(a.get_editor_property('compression_settings')),'srgb':a.get_editor_property('srgb'),'source':a.get_editor_property('asset_import_data').get_first_filename()})
open('D:/UnReal/TroubleSquad/Saved/Logs/EnemyTextureAudit.json','w').write(json.dumps(r,indent=2))
