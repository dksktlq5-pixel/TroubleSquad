import unreal,json
edit=unreal.MaterialEditingLibrary
for p in unreal.EditorAssetLibrary.list_assets('/Game/TroubleSquad/Enemies',recursive=False):
 mat=unreal.load_asset(p)
 if not isinstance(mat,unreal.Material) or not mat.get_name().startswith('M_'):continue
 n=mat.get_name()[2:];tex=unreal.load_asset('/Game/TroubleSquad/Enemies/T_'+n)
 if not tex:continue
 edit.delete_all_material_expressions(mat)
 node=edit.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-500,0);node.set_editor_property('texture',tex)
 assert edit.connect_material_property(node,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
 gain=edit.create_material_expression(mat,unreal.MaterialExpressionMultiply,-200,150);gain.set_editor_property('const_b',.15)
 assert edit.connect_material_expressions(node,'RGB',gain,'A');assert edit.connect_material_property(gain,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 rough=edit.create_material_expression(mat,unreal.MaterialExpressionConstant,-200,300);rough.set_editor_property('r',.8);edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 edit.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH);edit.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
 rig=unreal.load_asset('/Game/TroubleSquad/Enemies/Locomotion/'+n+'/SK_'+n)
 if rig:
  mats=[]
  for old in rig.get_editor_property('materials'):
   slot=unreal.SkeletalMaterial();slot.set_editor_property('material_interface',mat);slot.set_editor_property('material_slot_name',old.get_editor_property('material_slot_name'));mats.append(slot)
  rig.set_editor_property('materials',mats);unreal.EditorAssetLibrary.save_loaded_asset(rig)
 unreal.log('ENEMY_TEXTURE_BOUND '+n)
