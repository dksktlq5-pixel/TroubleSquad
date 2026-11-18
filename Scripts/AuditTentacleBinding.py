import unreal
m=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/SM_IllaoiTentacle')
for slot in m.get_editor_property('static_materials'):
 mat=slot.get_editor_property('material_interface')
 unreal.log('TENTACLE_SLOT '+str(mat.get_path_name() if mat else None))
b=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/M_TentacleBody')
unreal.log('TENTACLE_SHADER '+str(b.get_editor_property('shading_model'))+' '+str(unreal.MaterialEditingLibrary.get_material_property_input_node(b,unreal.MaterialProperty.MP_EMISSIVE_COLOR)))
