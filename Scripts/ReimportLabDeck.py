import unreal,pathlib
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/LabRebuild');dest='/Game/TroubleSquad/Maps/LabRebuild';tools=unreal.AssetToolsHelpers.get_asset_tools()
t=unreal.AssetImportTask();t.filename=str(root/'SM_LabRebuildDeck.fbx');t.destination_path=dest+'/Meshes';t.destination_name='SM_LabRebuildDeck';t.automated=True;t.replace_existing=True;t.save=True;o=unreal.FbxImportUI();o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.set_editor_property('auto_generate_collision',False);t.options=o;tools.import_asset_tasks([t]);mesh=unreal.load_asset(dest+'/Meshes/SM_LabRebuildDeck');assert mesh
for i,slot in enumerate(mesh.get_editor_property('static_materials')):
 name=str(slot.get_editor_property('material_slot_name')).removeprefix('CRB_');mat=unreal.load_asset(dest+'/Materials/M_OSG_'+name);assert mat,name;mesh.set_material(i,mat)
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
t=unreal.AssetImportTask();t.filename=str(root/'LabOverview.png');t.destination_path='/Game/TroubleSquad/UI';t.destination_name='T_Arena_SubterraneanLab_Rebuilt';t.automated=True;t.replace_existing=True;t.save=True;tools.import_asset_tasks([t]);unreal.log('LAB_DECK_REFINED');unreal.SystemLibrary.quit_editor()
