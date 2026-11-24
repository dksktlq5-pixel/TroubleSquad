import unreal,os
os.makedirs('D:/UnReal/TroubleSquad/ArtSource/EnemyMotion',exist_ok=True)
for path in unreal.EditorAssetLibrary.list_assets('/Game/TroubleSquad/Enemies'):
 if '/SM_' not in path:continue
 mesh=unreal.load_asset(path)
 if not isinstance(mesh,unreal.StaticMesh):continue
 t=unreal.AssetExportTask();t.object=mesh;t.filename='D:/UnReal/TroubleSquad/ArtSource/EnemyMotion/'+mesh.get_name()+'.fbx';t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();t.options=unreal.FbxExportOption();unreal.Exporter.run_asset_export_task(t)
unreal.log('ENEMY_EXPORT_DONE')
