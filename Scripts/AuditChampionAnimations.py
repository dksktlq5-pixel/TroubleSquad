import unreal,json
r=[]
for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/TroubleSquad/AnimaSquad',recursive=True):
 if str(a.asset_class_path.asset_name)=='AnimSequence':
  o=a.get_asset(); imp=o.get_editor_property('asset_import_data');r.append({'path':str(a.package_name),'source':imp.get_first_filename() if imp else '', 'duration':o.get_play_length()})
open('D:/UnReal/TroubleSquad/Saved/Logs/ChampionAnimationAudit.json','w').write(json.dumps(r,indent=2))
