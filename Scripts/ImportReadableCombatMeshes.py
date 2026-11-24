import unreal,pathlib
r='/Game/TroubleSquad/CombatVisuals/ReadableFX';tools=unreal.AssetToolsHelpers.get_asset_tools()
for p in pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/CombatVisuals/ReadableFX').glob('SM_*.fbx'):
 t=unreal.AssetImportTask();t.filename=str(p);t.destination_path=r;t.destination_name=p.stem;t.automated=True;t.replace_existing=True;t.save=True
 opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',False);t.options=opt;tools.import_asset_tasks([t])
 mesh=unreal.load_asset(r+'/'+p.stem);assert mesh,p.stem;mesh.set_editor_property('allow_cpu_access',True);unreal.EditorAssetLibrary.save_loaded_asset(mesh)
 unreal.log('READABLE_FX_IMPORTED '+p.stem)
