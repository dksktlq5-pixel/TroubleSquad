"""World-space ground detail: independent of stretched blockout mesh UVs."""
import unreal
root='/Game/TroubleSquad/Maps/ArenaMaterials'
assets=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary

def material(name, code):
 mat=unreal.load_asset(root+'/'+name)
 if mat and edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR): return mat
 if not mat: mat=assets.create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
 pos=edit.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-600,0)
 shader=edit.create_material_expression(mat,unreal.MaterialExpressionCustom,-300,0)
 custom_input=unreal.CustomInput()
 custom_input.set_editor_property('input_name','WorldPos')
 shader.set_editor_property('inputs',[custom_input])
 shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 shader.set_editor_property('code',code)
 assert edit.connect_material_expressions(pos,'',shader,'WorldPos')
 assert edit.connect_material_property(shader,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=edit.create_material_expression(mat,unreal.MaterialExpressionConstant,-300,200)
 rough.set_editor_property('r',.93)
 assert edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 edit.recompile_material(mat)
 assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
 return mat

sand=material('M_ReferenceSandGrain',r'''
float2 p=WorldPos.xy;
float grain=frac(sin(dot(floor(p/6.0),float2(12.9898,78.233)))*43758.5453);
float broad=0.5+0.5*sin(p.x/530.0+sin(p.y/370.0));
return float3(0.35,0.285,0.19)*(0.91+0.10*broad+0.07*grain);
''')
road_code=r'''
float2 p=WorldPos.xy;
ROTATE
float grain=frac(sin(dot(floor(p/5.0),float2(12.9898,78.233)))*43758.5453);
float2 edge=min(frac(p/float2(620,440)),1-frac(p/float2(620,440)))*float2(620,440);
float seam=1-smoothstep(0.8,2.8,min(edge.x,edge.y));
float wear=0.5+0.5*sin(p.x/900+sin(p.y/420));
return float3(0.115,0.121,0.128)*(0.90+0.08*grain+0.12*wear)*(1-0.42*seam);
'''
road=material('M_ReferenceRoadPanels',road_code.replace('ROTATE',''))
outskirts_road=material('M_ReferenceOutpostPanels',road_code.replace('ROTATE','p=float2(p.x*0.978148+p.y*0.207912,-p.x*0.207912+p.y*0.978148);'))
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for arena in ['Outskirts','Beachhead']:
 assert levels.load_level('/Game/TroubleSquad/Maps/L_'+arena)
 count=0
 for a in actors.get_all_level_actors():
  label=a.get_actor_label()
  target=sand if (arena=='Outskirts' and label=='Arena floor') or label=='Beach strip' else outskirts_road if label=='Access road' else road if label=='Urban landing pad' else None
  if target:
   a.static_mesh_component.set_material(0,target)
   count+=1
 assert levels.save_current_level()
 unreal.log('GROUND_REFERENCE_SAVED '+arena+' surfaces='+str(count))
