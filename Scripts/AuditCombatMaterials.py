import unreal
for name in ['M_TentacleBody','M_TentacleSuckers','M_FeatherVane','M_FeatherQuill','M_WindWall']:
 m=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/'+name)
 n=unreal.MaterialEditingLibrary.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
 unreal.log('VFX_AUDIT '+name+' base='+str(n.get_class().get_name() if n else 'none')+' params='+str(unreal.MaterialEditingLibrary.get_vector_parameter_names(m)))
 if n and n.get_class().get_name()=='MaterialExpressionConstant3Vector': unreal.log('VFX_COLOR '+name+' '+str(n.get_editor_property('constant')))
