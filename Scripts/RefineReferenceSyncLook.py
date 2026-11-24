import unreal,pathlib
exec(pathlib.Path('D:/UnReal/TroubleSquad/Scripts/CreateArcadeVfxMaterial.py').read_text(encoding='utf-8-sig'))
ed=unreal.MaterialEditingLibrary
base='/Game/TroubleSquad/Maps/LabRebuild/Materials/M_OSG_'
for name,color in [('Deck',(.028,.044,.052)),('DeckPanel',(.038,.061,.067)),('Metal',(.035,.058,.068))]:
 m=unreal.load_asset(base+name);assert m;ed.delete_all_material_expressions(m)
 c=ed.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-600,0);c.set_editor_property('constant',unreal.LinearColor(*color,1))
 noise=ed.create_material_expression(m,unreal.MaterialExpressionNoise,-600,180);noise.set_editor_property('scale',.018);noise.set_editor_property('levels',2);noise.set_editor_property('output_min',.96);noise.set_editor_property('output_max',1.04)
 mul=ed.create_material_expression(m,unreal.MaterialExpressionMultiply,-200,0);ed.connect_material_expressions(c,'',mul,'A');ed.connect_material_expressions(noise,'',mul,'B');ed.connect_material_property(mul,'',unreal.MaterialProperty.MP_BASE_COLOR)
 for prop,val,y in [(unreal.MaterialProperty.MP_METALLIC,.28,200),(unreal.MaterialProperty.MP_ROUGHNESS,.62,300)]:
  q=ed.create_material_expression(m,unreal.MaterialExpressionConstant,-200,y);q.set_editor_property('r',val);ed.connect_material_property(q,'',prop)
 ed.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m,False)
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);assert ls.load_level('/Game/TroubleSquad/Maps/L_SubterraneanLab_Rebuilt')
for a in es.get_all_level_actors():
 if isinstance(a,unreal.DirectionalLight):
  c=a.get_component_by_class(unreal.DirectionalLightComponent);c.set_intensity(2.2 if 'Sun' in a.get_actor_label() else 1.1)
 if isinstance(a,unreal.PostProcessVolume):
  s=a.get_editor_property('settings');s.set_editor_property('override_auto_exposure_method',True);s.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL);s.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);s.set_editor_property('auto_exposure_apply_physical_camera_exposure',False);s.set_editor_property('override_auto_exposure_bias',True);s.set_editor_property('auto_exposure_bias',.65);s.set_editor_property('override_bloom_intensity',True);s.set_editor_property('bloom_intensity',.24);a.set_editor_property('settings',s)
assert ls.save_current_level();unreal.log('REFERENCE_SYNC_TONE_READY');unreal.SystemLibrary.quit_editor()
