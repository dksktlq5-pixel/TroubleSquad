import unreal,pathlib,json
root='/Game/TroubleSquad/Maps/ReferenceArchitecture';tools=unreal.AssetToolsHelpers.get_asset_tools();edit=unreal.MaterialEditingLibrary
colors={'Hull':(.035,.07,.085),'Steel':(.13,.22,.27),'Edge':(.55,.66,.67),'Cyan':(.03,.6,.7),'Gold':(.55,.32,.08),'Earth':(.28,.19,.10),'Sand':(.43,.31,.16)}
materials={}
for n,c in colors.items():
 name='M_AC_'+n;mat=unreal.load_asset(root+'/'+name)
 if not mat:mat=tools.create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
 edit.delete_all_material_expressions(mat)
 color=edit.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,0);color.set_editor_property('constant',unreal.LinearColor(*c,1));edit.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=edit.create_material_expression(mat,unreal.MaterialExpressionConstant,-300,180);rough.set_editor_property('r',.75);edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 if n=='Cyan':edit.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 edit.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);materials[n]=mat
meshes={}
for src in pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/Architecture').glob('SM_*.fbx'):
 task=unreal.AssetImportTask();task.filename=str(src);task.destination_path=root;task.destination_name=src.stem;task.automated=True;task.replace_existing=True;task.save=True
 options=unreal.FbxImportUI();options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False;options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',False);task.options=options;tools.import_asset_tasks([task]);mesh=unreal.load_asset(root+'/'+src.stem)
 if not mesh:raise RuntimeError(src.stem)
 for i,slot in enumerate(mesh.get_editor_property('static_materials')):
  n=str(slot.get_editor_property('material_slot_name'));mesh.set_material(i,materials.get(n,materials['Steel']))
 unreal.EditorAssetLibrary.save_loaded_asset(mesh);meshes[src.stem[3:]]=mesh
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report=[]
for arena in ['CoastalResearch','Outskirts','SubterraneanLab','Beachhead']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+arena);all_actors=list(es.get_all_level_actors());existing={a.get_actor_label():a for a in all_actors};count=0
 for source in all_actors:
  if not isinstance(source,unreal.StaticMeshActor):continue
  label=source.get_actor_label();kind=None
  if label.startswith('Architecture | '):continue
  if label=='Fortification' or label.startswith('Reference polish | city structure'):kind='FortressWall'
  elif label.startswith('Reference polish | specimen tank'):kind='Reactor'
  elif label.startswith('Reference polish | island console'):kind='Console'
  elif label.startswith('Reference polish | coolant trunk'):kind='PipeRack'
  elif label=='Service crossing':kind='Bridge'
  elif label.startswith(('Reference polish | impact basin','Reference polish | dune basin')):kind='Crater'
  elif label.startswith(('Reference polish | crater rim','Reference polish | dune rim')):source.set_actor_hidden_in_game(True)
  if not kind:continue
  center,extent=source.get_actor_bounds(False);p=source.get_actor_location();target=[extent.x*2,extent.y*2,extent.z*2]
  if kind=='Crater':target[2]=60
  if kind=='Bridge':target=[780,600,110]
  if kind=='Console':target=[90,90,100]
  art_label='Architecture | '+source.get_name();a=existing.get(art_label)
  if not a:a=es.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(p.x,p.y,0));a.set_actor_label(art_label)
  mesh=meshes[kind];bounds=mesh.get_bounds();e=bounds.box_extent;scale=unreal.Vector(target[0]/max(1,e.x*2),target[1]/max(1,e.y*2),target[2]/max(1,e.z*2))
  a.static_mesh_component.set_static_mesh(mesh);a.set_actor_scale3d(scale);a.set_actor_rotation(source.get_actor_rotation(),False)
  z=0 if kind in ['Bridge','Crater','FortressWall'] else p.z
  a.set_actor_location(unreal.Vector(p.x,p.y,z-(bounds.origin.z-e.z)*scale.z),False,False)
  a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);source.set_actor_hidden_in_game(True);count+=1
 # Tone down broad light pools so projectiles remain readable against the arena.
 for a in all_actors:
  light=a.get_component_by_class(unreal.LocalLightComponent)
  if light:light.set_intensity(min(light.get_editor_property('intensity'),1200.))
 vol=existing.get('Arcade clarity postprocess')
 if not vol:vol=es.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());vol.set_actor_label('Arcade clarity postprocess')
 vol.set_editor_property('unbound',True);vol.set_editor_property('priority',200)
 settings=vol.get_editor_property('settings');settings.set_editor_property('override_bloom_intensity',True);settings.set_editor_property('bloom_intensity',.08);vol.set_editor_property('settings',settings)
 ls.save_current_level();report.append({'arena':arena,'replaced':count})
with open('D:/UnReal/TroubleSquad/Saved/Logs/ArchitecturePlacement.json','w') as f:json.dump(report,f)
unreal.log('REFERENCE_ARCHITECTURE_APPLIED')
