import unreal
p='D:/UnReal/TroubleSquad/ArtSource/Branding/TroubleSquad_Logo.png'
t=unreal.AssetImportTask();t.filename=p;t.destination_path='/Game/TroubleSquad/UI';t.destination_name='T_TroubleSquadLogo';t.automated=True;t.save=True;t.replace_existing=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
a=unreal.load_asset('/Game/TroubleSquad/UI/T_TroubleSquadLogo');assert a
a.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
a.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
a.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
unreal.EditorAssetLibrary.save_loaded_asset(a)
unreal.log('TROUBLESQUAD_LOGO_IMPORTED')
