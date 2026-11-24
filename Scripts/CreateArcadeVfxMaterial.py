"""Soft-edged additive ribbons; authored geometry carries champion color and lifetime alpha."""
import unreal
root = '/Game/TroubleSquad/CombatVisuals'
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary
name = 'M_ArcadeRibbon'
mat = unreal.load_asset(root + '/' + name)
if not mat:
    mat = tools.create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
vertex = edit.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -650, 0)
uv = edit.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -650, 180)
soft = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, -350, 160)
inp = unreal.CustomInput()
inp.set_editor_property('input_name', 'UV')
clock_input = unreal.CustomInput()
clock_input.set_editor_property('input_name', 'Clock')
soft.set_editor_property('inputs', [inp, clock_input])
soft.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
soft.set_editor_property('code', '''
if(UV.x>3.5) { return .9; }
if(UV.x>1.5) {
    float2 p=float2(UV.x-2,UV.y)*2-1;
    float r=dot(p,p);
    float wisps=.82+.18*sin(p.x*15+p.y*11-Clock*7)*sin(p.y*17+Clock*5);
    return pow(saturate(1-r),2.5)*wisps;
}
float wave=.055*sin(UV.x*23-Clock*8)+.025*sin(UV.x*51+Clock*13);
float edge=saturate(1-abs((UV.y+wave)*2-1));
float streak=.84+.16*sin(UV.x*38-Clock*14);
return pow(edge,1.4)*streak;
''')
edit.connect_material_expressions(uv, '', soft, 'UV')
clock = edit.create_material_expression(mat, unreal.MaterialExpressionTime, -650, 300)
edit.connect_material_expressions(clock, '', soft, 'Clock')
alpha = edit.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 150)
assert edit.connect_material_expressions(vertex, 'A', alpha, 'A'), 'Vertex alpha not connected'
edit.connect_material_expressions(soft, '', alpha, 'B')
gain = edit.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, -100)
gain.set_editor_property('const_b', 1.1)
assert edit.connect_material_expressions(vertex, '', gain, 'A')
assert edit.connect_material_property(gain, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR), 'Vertex color not connected'
assert edit.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY), 'Opacity not connected'
edit.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('ARCADE_VFX_MATERIAL_READY')
