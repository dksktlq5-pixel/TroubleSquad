import unreal,pathlib,json
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/EnemyMotion');tools=unreal.AssetToolsHelpers.get_asset_tools();report=[]
for src in sorted(root.glob('SK_*.fbx')):
 n=src.stem[3:];folder='/Game/TroubleSquad/Enemies/Locomotion/'+n
 opts=unreal.FbxImportUI();opts.import_mesh=True;opts.import_as_skeletal=True;opts.import_animations=True;opts.import_materials=False;opts.import_textures=False
 opts.set_editor_property('automated_import_should_detect_type',False);opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
 opts.skeletal_mesh_import_data.set_editor_property('import_morph_targets',False)
 task=unreal.AssetImportTask();task.filename=str(src);task.destination_path=folder;task.destination_name='SK_'+n;task.automated=True;task.replace_existing=True;task.save=True;task.options=opts;tools.import_asset_tasks([task])
 mesh=unreal.load_asset(folder+'/SK_'+n)
 if not mesh:raise RuntimeError('Missing skeleton mesh '+n)
 mat=unreal.load_asset('/Game/TroubleSquad/Enemies/M_'+n)
 mats=mesh.get_editor_property('materials')
 for slot in mats:slot.set_editor_property('material_interface',mat)
 mesh.set_editor_property('materials',mats);unreal.EditorAssetLibrary.save_loaded_asset(mesh)
 anim=None
 for a in unreal.EditorAssetLibrary.list_assets(folder):
  obj=unreal.load_asset(a)
  if isinstance(obj,unreal.AnimSequence):anim=obj;break
 if not anim:raise RuntimeError('Missing animation '+n)
 target=folder+'/AN_'+n+'_Walk'
 if anim.get_path_name().split('.')[0]!=target:unreal.EditorAssetLibrary.rename_asset(anim.get_path_name(),target)
 report.append({'name':n,'mesh':mesh.get_path_name(),'walk':target})
 unreal.log('LOCOMOTION_IMPORTED '+n)
with open('D:/UnReal/TroubleSquad/Saved/Logs/EnemyLocomotionImport.json','w') as f:json.dump(report,f)
