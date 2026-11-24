import unreal,pathlib,json
root='/Game/TroubleSquad/CombatVisuals/GeneralWeapons';ed=unreal.MaterialEditingLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();mats={}
palette={'Armor':(.045,.065,.12),'Steel':(.28,.40,.51),'Silver':(.65,.77,.84),'Purple':(.27,.045,.42),'Gold':(.95,.49,.06),'Green':(.045,.36,.23),'Cyan':(.025,.8,1),'Pink':(1,.045,.45),'Black':(.012,.018,.032)}
for name,color in palette.items():
 p=root+'/Materials/M_GW_'+name;mat=unreal.load_asset(p)
 if not mat:mat=tools.create_asset('M_GW_'+name,root+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 ed.delete_all_material_expressions(mat);c=ed.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-400,0);c.set_editor_property('constant',unreal.LinearColor(*color,1));ed.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
 for prop,value,y in [(unreal.MaterialProperty.MP_ROUGHNESS,.32,150),(unreal.MaterialProperty.MP_METALLIC,.6,250)]:
  q=ed.create_material_expression(mat,unreal.MaterialExpressionConstant,-400,y);q.set_editor_property('r',value);ed.connect_material_property(q,'',prop)
 gain=ed.create_material_expression(mat,unreal.MaterialExpressionMultiply,-100,100);gain.set_editor_property('const_b',2.8 if name in ['Cyan','Pink'] else .09);ed.connect_material_expressions(c,'',gain,'A');ed.connect_material_property(gain,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 ed.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES);ed.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);mats['GW_'+name]=mat
report=[]
for src in pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/GeneralWeaponModels').glob('SM_GW_*.fbx'):
 t=unreal.AssetImportTask();t.filename=str(src);t.destination_path=root+'/Models';t.destination_name=src.stem;t.automated=True;t.replace_existing=True;t.save=True
 o=unreal.FbxImportUI();o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',False);t.options=o;tools.import_asset_tasks([t]);mesh=unreal.load_asset(t.destination_path+'/'+src.stem);assert mesh,src.stem
 for i,slot in enumerate(mesh.get_editor_property('static_materials')):
  n=str(slot.get_editor_property('material_slot_name'));assert n in mats,(src.stem,n);mesh.set_material(i,mats[n])
 mesh.set_editor_property('allow_cpu_access',True);unreal.EditorAssetLibrary.save_loaded_asset(mesh);b=mesh.get_bounds();report.append({'name':src.stem,'materials':mesh.get_num_sections(0),'extent':str(b.box_extent)})
open('D:/UnReal/TroubleSquad/Saved/Logs/GeneralWeaponModelsImport.json','w').write(json.dumps(report,indent=2))
assert len(report)==7
unreal.log('GENERAL_WEAPON_MODELS_IMPORTED 7/7')
