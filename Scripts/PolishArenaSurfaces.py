import unreal
root='/Game/TroubleSquad/Maps/ArenaMaterials';ed=unreal.MaterialEditingLibrary

def make(name,code,emission=0):
 path=root+'/'+name;mat=unreal.load_asset(path)
 if not mat:mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
 ed.delete_all_material_expressions(mat)
 pos=ed.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-600,0)
 node=ed.create_material_expression(mat,unreal.MaterialExpressionCustom,-300,0)
 inp=unreal.CustomInput();inp.set_editor_property('input_name','P');node.set_editor_property('inputs',[inp]);node.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);node.set_editor_property('code',code)
 ed.connect_material_expressions(pos,'',node,'P');assert ed.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=ed.create_material_expression(mat,unreal.MaterialExpressionConstant,-250,250);rough.set_editor_property('r',.88);ed.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 if emission:
  g=ed.create_material_expression(mat,unreal.MaterialExpressionMultiply,-80,150);g.set_editor_property('const_b',emission);ed.connect_material_expressions(node,'',g,'A');ed.connect_material_property(g,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 ed.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);return mat
lab=make('M_LabReferencePanels',r'''
float2 q=P.xy/420.;float2 t=frac(q);float2 e=min(t,1-t);
float seam=1-smoothstep(.003,.012,min(e.x,e.y));
float fine=frac(sin(dot(floor(P.xy/3),float2(12.9898,78.233)))*43758.5453);
float alternate=fmod(abs(floor(q.x)+floor(q.y)),2);
float3 c=float3(.053,.083,.102)*(1+.08*alternate+.06*fine);
c*=1-.68*seam;
float vent=step(.88,t.x)*step(.3,t.y)*step(t.y,.7)*step(.45,frac(t.y*34));
c*=1-.3*vent;return c;
''',.12)
road=make('M_OutpostWeatheredRoad',r'''
float2 q=P.xy/600.;float2 t=frac(q);float2 e=min(t,1-t);
float seam=1-smoothstep(.003,.009,min(e.x,e.y));
float fine=frac(sin(dot(floor(P.xy/4),float2(12.9898,78.233)))*43758.5453);
float wear=.5+.5*sin(P.x/510+sin(P.y/280));
return float3(.11,.112,.095)*(.85+.2*wear+.1*fine)*(1-.34*seam);
''')
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for arena in ['SubterraneanLab','Outskirts']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+arena)
 for a in es.get_all_level_actors():
  if not isinstance(a,unreal.StaticMeshActor):continue
  label=a.get_actor_label()
  if arena=='SubterraneanLab' and (label=='Arena floor' or label.startswith('Lab Work Deck ')):
   for slot in range(a.static_mesh_component.get_num_materials()):a.static_mesh_component.set_material(slot,lab)
  if arena=='Outskirts' and label=='Access road':a.static_mesh_component.set_material(0,road)
 ls.save_current_level()
unreal.log('REFERENCE_SURFACES_DONE')
