import unreal
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
path='/Game/TroubleSquad/Maps/ArenaMaterials/M_LabDarkDeck'
mat=unreal.load_asset(path)
if not mat:
 mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_LabDarkDeck','/Game/TroubleSquad/Maps/ArenaMaterials',unreal.Material,unreal.MaterialFactoryNew())
 color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,0)
 color.set_editor_property('constant',unreal.LinearColor(.022,.032,.045,1))
 assert unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
 for prop,value,y in [(unreal.MaterialProperty.MP_ROUGHNESS,.72,140),(unreal.MaterialProperty.MP_METALLIC,.35,240)]:
  node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant,-300,y)
  node.set_editor_property('r',value)
  assert unreal.MaterialEditingLibrary.connect_material_property(node,'',prop)
 unreal.MaterialEditingLibrary.recompile_material(mat)
 assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
assert levels.load_level('/Game/TroubleSquad/Maps/L_SubterraneanLab')
count=0
for actor in actors.get_all_level_actors():
 if actor.get_actor_label().startswith('Lab Work Deck '):
  component=actor.static_mesh_component
  for slot in range(component.get_num_materials()): component.set_material(slot,mat)
  count+=1
assert count==21,count
assert levels.save_current_level()
unreal.log('LAB_REFERENCE_TONE_SAVED panels='+str(count))
