import unreal,pathlib,json
srcroot=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/OutskirtsRebuild/Models')
manifest=json.loads((srcroot/'BuildingManifest.json').read_text(encoding='utf-8'));assert len(manifest['buildings'])==16
root='/Game/TroubleSquad/Maps/OutskirtsRebuild';ed=unreal.MaterialEditingLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();mats={}
palette={'Concrete':(.67,.71,.70),'ConcreteShade':(.39,.46,.46),'Trim':(.88,.90,.83),'Metal':(.23,.31,.34),'Dark':(.045,.064,.079),'Roof':(.095,.13,.145),'Glass':(.055,.25,.32),'Orange':(.88,.32,.08),'Grass':(.25,.42,.075),'Leaf':(.09,.27,.085),'Soil':(.12,.075,.035),'AdBlue':(.018,.30,.60),'AdTeal':(.015,.40,.38),'Cyan':(.045,.72,.86),'Pink':(.83,.07,.44),'Yellow':(.93,.63,.14)}
palette.update({'Sand':(.46,.31,.16),'Earth':(.23,.14,.075),'Rock':(.24,.245,.215),'RockLight':(.38,.36,.29),'Military':(.18,.22,.19),'Rust':(.34,.15,.055),'Healing':(.03,.65,.28)})
for name,color in palette.items():
 p=root+'/Materials/M_CRB_'+name;mat=unreal.load_asset(p)
 if not mat:mat=tools.create_asset('M_CRB_'+name,root+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 ed.delete_all_material_expressions(mat);c=ed.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-400,0);c.set_editor_property('constant',unreal.LinearColor(*color,1));ed.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
 for prop,value,y in [(unreal.MaterialProperty.MP_ROUGHNESS,.28 if name in ['Glass','Metal'] else .65,150),(unreal.MaterialProperty.MP_METALLIC,.5 if name=='Metal' else .22 if name=='Glass' else 0,250)]:
  q=ed.create_material_expression(mat,unreal.MaterialExpressionConstant,-400,y);q.set_editor_property('r',value);ed.connect_material_property(q,'',prop)
 if name in ['Cyan','Pink','AdBlue','AdTeal','Yellow']:
  gain=ed.create_material_expression(mat,unreal.MaterialExpressionMultiply,-100,100);gain.set_editor_property('const_b',1.5 if name in ['Cyan','Pink'] else .22);ed.connect_material_expressions(c,'',gain,'A');ed.connect_material_property(gain,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 ed.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES);ed.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);mats['CRB_'+name]=mat
report=[]
for row in manifest['buildings']:
 src=srcroot/row['fbx'];t=unreal.AssetImportTask();t.filename=str(src);t.destination_path=root+'/Models';t.destination_name=src.stem;t.automated=True;t.replace_existing=True;t.save=True
 o=unreal.FbxImportUI();o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',True);o.static_mesh_import_data.set_editor_property('auto_generate_collision',False);t.options=o;tools.import_asset_tasks([t]);mesh=unreal.load_asset(t.destination_path+'/'+src.stem);assert mesh,src.stem
 for i,slot in enumerate(mesh.get_editor_property('static_materials')):
  n=str(slot.get_editor_property('material_slot_name'));assert n in mats,(src.stem,n);mesh.set_material(i,mats[n])

 if row['id'].startswith(('05_','14_')):
  body=mesh.get_editor_property('body_setup');agg=body.get_editor_property('agg_geom');box=unreal.KBoxElem();side=480.0 if row['id'].startswith('05_') else 140.0;box.set_editor_property('x',side);box.set_editor_property('y',side);box.set_editor_property('z',row['dimensions_ue_cm'][2]);box.set_editor_property('center',unreal.Vector(0,0,row['dimensions_ue_cm'][2]/2));agg.set_editor_property('box_elems',[box]);agg.set_editor_property('convex_elems',[]);body.set_editor_property('agg_geom',agg)
 if row['id'].startswith('09_'):mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 unreal.EditorAssetLibrary.save_loaded_asset(mesh,False);b=mesh.get_bounds();dims=[b.box_extent.x*2,b.box_extent.y*2,b.box_extent.z*2];expected=row['dimensions_ue_cm']
 assert all(abs(a-b)<1 for a,b in zip(dims,expected)),(src.stem,dims,expected)
 assert abs(b.origin.z-b.box_extent.z)<1,(src.stem,'ground pivot')
 body=mesh.get_editor_property('body_setup');agg=body.get_editor_property('agg_geom');collision_count=len(agg.get_editor_property('convex_elems'))+len(agg.get_editor_property('box_elems'))
 if collision_count==0:
  box=unreal.KBoxElem();box.set_editor_property('x',dims[0]);box.set_editor_property('y',dims[1]);box.set_editor_property('z',dims[2]);box.set_editor_property('center',b.origin);agg.set_editor_property('box_elems',[box]);body.set_editor_property('agg_geom',agg);unreal.EditorAssetLibrary.save_loaded_asset(mesh,False);collision_count=len(mesh.get_editor_property('body_setup').get_editor_property('agg_geom').get_editor_property('box_elems'))
 if row['id'].startswith('09_'):
  body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
 assert collision_count>0,src.stem
 report.append({'id':row['id'],'asset':t.destination_path+'/'+src.stem,'dimensions_cm':dims,'ground_min_z':b.origin.z-b.box_extent.z,'material_slots':len(mesh.get_editor_property('static_materials')),'convex_collision_count':collision_count})
(srcroot/'UnrealImportVerification.json').write_text(json.dumps({'imported':len(report),'buildings':report},ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('OUTSKIRTS_MODELS_IMPORTED_AND_VERIFIED 16/16')
