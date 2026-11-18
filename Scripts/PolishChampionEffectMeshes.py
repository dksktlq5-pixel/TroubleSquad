"""Anima reference accents: warm coral tentacles, blue-violet feather vanes and gold quills."""
import unreal
edit=unreal.MaterialEditingLibrary
root='/Game/TroubleSquad/CombatVisuals/'
for name,color in [('M_TentacleSuckers',(0.75,0.13,0.28)),('M_FeatherVane',(0.08,0.22,0.65)),('M_FeatherQuill',(1.0,0.65,0.16))]:
    mat=unreal.load_asset(root+name)
    node=edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
    assert node and node.get_class().get_name()=='MaterialExpressionConstant3Vector',name
    node.set_editor_property('constant',unreal.LinearColor(*color,1.0))
    edit.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
body=unreal.load_asset(root+'M_TentacleBody')
# Rebuild only this simple body shader; keep all mesh assignments and geometry.
edit.delete_all_material_expressions(body)
body.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
pos=edit.create_material_expression(body,unreal.MaterialExpressionWorldPosition,-700,-100)
shade=edit.create_material_expression(body,unreal.MaterialExpressionCustom,-420,-100)
inp=unreal.CustomInput()
inp.set_editor_property('input_name','P')
shade.set_editor_property('inputs',[inp])
shade.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
shade.set_editor_property('code','float tip=saturate(P.z/330.0); float bands=0.92+0.08*sin(P.z*0.12+sin(P.x*0.05)); return lerp(float3(0.48,0.12,0.025),float3(1,0.52,0.09),tip)*bands;')
assert edit.connect_material_expressions(pos,'',shade,'P')
assert edit.connect_material_property(shade,'',unreal.MaterialProperty.MP_BASE_COLOR)
gain=edit.create_material_expression(body,unreal.MaterialExpressionMultiply,-150,50)
gain.set_editor_property('const_b',0.8)
assert edit.connect_material_expressions(shade,'',gain,'A')
assert edit.connect_material_property(gain,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(body)
assert unreal.EditorAssetLibrary.save_loaded_asset(body)
unreal.log('CHAMPION_EFFECT_MESH_POLISH_READY')
