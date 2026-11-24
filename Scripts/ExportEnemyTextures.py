import unreal,os,json
os.makedirs('D:/UnReal/TroubleSquad/Saved/EnemyTextureReview',exist_ok=True)
for n in ['Swarmer','Pix','Ghoul','Fuemigo','Red','Lancer','Crusher']:
 tex=unreal.load_asset('/Game/TroubleSquad/Enemies/T_'+n)
 task=unreal.AssetExportTask();task.object=tex;task.filename='D:/UnReal/TroubleSquad/Saved/EnemyTextureReview/'+n+'.png';task.exporter=unreal.TextureExporterPNG();task.automated=True;task.prompt=False;task.replace_identical=True;unreal.Exporter.run_asset_export_task(task)
 unreal.log('TEXTURE_SETTINGS '+n+' '+str(tex.get_editor_property('srgb'))+' '+str(tex.get_editor_property('compression_settings')))
