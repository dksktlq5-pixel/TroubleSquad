import unreal
root='/Game/TroubleSquad/CombatVisuals/Pickups';ed=unreal.MaterialEditingLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();mats={}
for name,color in {'Stone':(.07,.18,.22),'Steel':(.24,.43,.48),'Cyan':(.02,.8,1),'Green':(.08,1,.24)}.items():
 path=root+'/M_Heal_'+name;mat=unreal.load_asset(path) or tools.create_asset('M_Heal_'+name,root,unreal.Material,unreal.MaterialFactoryNew());ed.delete_all_material_expressions(mat)
 c=ed.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(*color));ed.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
 if name in ('Cyan','Green'):
  e=ed.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);e.set_editor_property('constant',unreal.LinearColor(*(x*3 for x in color)));ed.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 ed.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);mats[name]=mat
x=unreal.AssetImportTask();x.filename='D:/UnReal/TroubleSquad/ArtSource/HealthPack/SM_HealthPack.fbx';x.destination_path=root;x.destination_name='SM_HealthPack';x.automated=True;x.replace_existing=True;x.save=True
o=unreal.FbxImportUI();o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',False);x.options=o;tools.import_asset_tasks([x]);mesh=unreal.load_asset(root+'/SM_HealthPack');assert mesh
for i,slot in enumerate(mesh.get_editor_property('static_materials')):mesh.set_material(i,mats[str(slot.get_editor_property('material_slot_name'))])
unreal.EditorAssetLibrary.save_loaded_asset(mesh);unreal.log('HEALTH_PACK_IMPORTED '+str(mesh.get_bounds().box_extent))
