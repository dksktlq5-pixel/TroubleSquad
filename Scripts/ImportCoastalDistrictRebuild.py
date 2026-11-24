def main():
 import unreal,pathlib,json,math
 ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/CoastalRebuild');layout=json.loads((ROOT/'Layout.json').read_text(encoding='utf-8'));tools=unreal.AssetToolsHelpers.get_asset_tools();ed=unreal.MaterialEditingLibrary;dest='/Game/TroubleSquad/Maps/CoastalRebuild'
 # Ground materials share the new architectural palette.
 palette={'Concrete':(.67,.71,.70),'ConcreteShade':(.39,.46,.46),'Trim':(.88,.90,.83),'Metal':(.23,.31,.34),'Dark':(.045,.064,.079),'Roof':(.095,.13,.145),'Glass':(.055,.25,.32),'Orange':(.88,.32,.08),'Grass':(.19,.34,.06),'Leaf':(.09,.27,.085),'Soil':(.12,.075,.035),'AdBlue':(.018,.30,.60),'AdTeal':(.015,.40,.38),'Cyan':(.045,.72,.86),'Pink':(.83,.07,.44),'Yellow':(.93,.63,.14),'Road':(.14,.18,.20),'Brick':(.46,.19,.17),'Paving':(.59,.55,.45),'Water':(.015,.28,.36),'Earth':(.30,.21,.12)}
 mats={}
 for n,color in palette.items():
  p=dest+'/Materials/M_Coast_'+n;mat=unreal.load_asset(p)
  if not mat:mat=tools.create_asset('M_Coast_'+n,dest+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
  ed.delete_all_material_expressions(mat);c=ed.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-400,0);c.set_editor_property('constant',unreal.LinearColor(*color,1));ed.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
  q=ed.create_material_expression(mat,unreal.MaterialExpressionConstant,-400,130);q.set_editor_property('r',.30 if n=='Water' else .85);ed.connect_material_property(q,'',unreal.MaterialProperty.MP_ROUGHNESS)
  ed.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat,False);mats['CRB_'+n]=mat
 imp=unreal.AssetImportTask();imp.filename=str(ROOT/'SM_CoastalRebuildGround.fbx');imp.destination_path=dest+'/Meshes';imp.destination_name='SM_CoastalRebuildGround';imp.automated=True;imp.replace_existing=True;imp.save=True;o=unreal.FbxImportUI();o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',True);o.static_mesh_import_data.set_editor_property('auto_generate_collision',False);imp.options=o;tools.import_asset_tasks([imp]);ground=unreal.load_asset(dest+'/Meshes/SM_CoastalRebuildGround');assert ground
 for i,slot in enumerate(ground.get_editor_property('static_materials')):ground.set_material(i,mats[str(slot.get_editor_property('material_slot_name'))])
 body=ground.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(ground,False)
 ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);assert ls and es
 path='/Game/TroubleSquad/Maps/L_CoastalResearch_Rebuilt'
 if not unreal.EditorAssetLibrary.does_asset_exist(path):
  assert ls.load_level('/Game/TroubleSquad/Maps/L_CoastalResearch');assert unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(),path);unreal.log('BASE_COPY_CREATED_RERUN_IMPORT');unreal.SystemLibrary.quit_editor();return
 else:assert ls.load_level(path)
 for actor in es.get_all_level_actors():
  if actor.get_class().get_name() in ['StaticMeshActor','AnimaPreviewCharacter','PointLight','PostProcessVolume','CameraActor','Actor']:es.destroy_actor(actor)
 placed=[]
 def add(label,mesh,p,yaw=0,scale=1,collision=True):
  assert mesh,label
  a=es.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p));a.set_actor_label('Rebuilt | '+label);c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False);a.set_actor_scale3d(unreal.Vector(*([scale]*3 if isinstance(scale,(int,float)) else scale)));a.set_folder_path('Coastal rebuild');placed.append(a);return a
 add('district ground and road design',ground,(0,0,0)).static_mesh_component.set_editor_property('cast_shadow',False)
 for i,row in enumerate(layout['placements']):
  folder='ReferenceBuildings' if int(row['id'][:2])<=14 else 'ReferenceIndustrialModules';m=unreal.load_asset('/Game/TroubleSquad/Maps/'+folder+'/Models/SM_CRB_'+row['id']);add(str(i)+' '+row['id'],m,[v*100 for v in row['position_m']],row['yaw_deg'],row['scale'])
 old='/Game/TroubleSquad/Maps/CoastalResearchMeshes/'
 for i,row in enumerate(layout['props']):
  if 'mesh' in row:add('prop '+str(i)+' '+row['mesh'],unreal.load_asset(old+row['mesh']),[v*100 for v in row['position_m']],row.get('yaw_deg',0),row['scale'],collision='Healing' in row['mesh'] or 'Fountain' in row['mesh'])
  else:add('tree trunk '+str(i),unreal.load_asset('/Engine/BasicShapes/Cylinder'),[v*100 for v in row['position_m']],0,row['scale_m'],False).static_mesh_component.set_material(0,mats['CRB_Earth'])
 # Promenade details frame the paths without obstructing the broad combat routes.
 for i,(x,y) in enumerate([(-19,-27),(-19,-16),(-18,-4),(-26,21),(-12,29),(5,30),(12,-29),(26,-23),(30,-8),(37,12),(37,35),(-39,-16),(-33,4)]):
  add('pedestrian lamp '+str(i),unreal.load_asset(old+'SM_CR_DistrictDiscLamp'),(x*100,y*100,28),0,1.2,False)
 for i in range(14):
  x=-16+i*2.4;y=-33;add('north bollard '+str(i),unreal.load_asset(old+'SM_CR_SafetyBollard'),(x*100,y*100,18),0,1,False)
 for i,(x,y) in enumerate([(-39,-26),(-39,-24),(-26,-28),(-25,-27),(-24,-26),(-37,10),(-36,10),(-35,10),(-24,16),(-23,15),(-22,14),(-6,-27),(-7,-28),(22,-27)]):add('landscape shrub '+str(i),unreal.load_asset(old+'SM_CR_ParkLeafyShrub'),(x*100,y*100,55),0,1.5,False)
 wi=unreal.AssetImportTask();wi.filename=str(ROOT/'SM_CoastalRebuildWater.fbx');wi.destination_path=dest+'/Meshes';wi.destination_name='SM_CoastalRebuildWater';wi.automated=True;wi.replace_existing=True;wi.save=True;wi.options=o;tools.import_asset_tasks([wi]);wm=unreal.load_asset(dest+'/Meshes/SM_CoastalRebuildWater');wm.set_material(0,mats['CRB_Water']);unreal.EditorAssetLibrary.save_loaded_asset(wm,False);add('coastal ocean',wm,(0,0,0),0,1,False)
 # Keep playable spawn points and the original game mode, while providing a named overview camera.
 cam=es.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(7320,10980,13176));cam.set_actor_label('Coastal Rebuild Overview');rot=unreal.MathLibrary.find_look_at_rotation(cam.get_actor_location(),unreal.Vector(0,0,150));cam.set_actor_rotation(rot,False);cam.camera_component.set_editor_property('projection_mode',unreal.CameraProjectionMode.PERSPECTIVE);cam.camera_component.set_editor_property('aspect_ratio',1.25);cam.camera_component.set_field_of_view(50)
 for a in es.get_all_level_actors():
  if isinstance(a,unreal.DirectionalLight):a.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE);a.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(3.5 if 'Sun' in a.get_actor_label() else 1.2)
 post=es.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());post.set_actor_label('Rebuild daylight exposure');post.set_editor_property('unbound',True);settings=post.get_editor_property('settings');settings.set_editor_property('override_auto_exposure_bias',True);settings.set_editor_property('auto_exposure_bias',-.4);settings.set_editor_property('override_bloom_intensity',True);settings.set_editor_property('bloom_intensity',.20);post.set_editor_property('settings',settings)
 assert ls.save_current_level();unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(7320,10980,13176),rot)
 report={'map':path,'placed_actors':len(placed),'new_model_instances':len(layout['placements']),'source_layout':str(ROOT/'Layout.json'),'ground_complex_collision':True,'original_map_preserved':True};(ROOT/'UnrealRebuildVerification.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('COASTAL_DISTRICT_REBUILT '+str(len(placed)));unreal.SystemLibrary.quit_editor()


main()
