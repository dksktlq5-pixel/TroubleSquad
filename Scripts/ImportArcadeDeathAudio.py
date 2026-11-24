import unreal,pathlib
for p in pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/AudioArcade/Enemies').glob('Minion_Death_*.wav'):
 t=unreal.AssetImportTask();t.filename=str(p);t.destination_path='/Game/TroubleSquad/Audio/Enemies';t.destination_name=p.stem;t.automated=True;t.replace_existing=True;t.save=True;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
