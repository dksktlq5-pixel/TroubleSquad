import unreal,json
out=[]
for folder in unreal.EditorAssetLibrary.list_assets('/Game/TroubleSquad/Enemies/Locomotion',recursive=True):
 a=unreal.load_asset(folder)
 if isinstance(a,unreal.SkeletalMesh):
  out.append({'name':a.get_name(),'materials':[str(x.get_editor_property('material_interface')) for x in a.get_editor_property('materials')]})
with open('D:/UnReal/TroubleSquad/Saved/Logs/EnemySkinBinding.json','w') as f:json.dump(out,f)
