import unreal,pathlib,json
src=pathlib.Path('D:/UnReal/TroubleSquad/Scripts/PolishArenaSurfaces.py').read_text(encoding='utf-8-sig');exec(src.split("lab=make(")[0])
sand=make('M_ReferenceSandGrain',r'''
float2 p=P.xy;
float broad=.5+.25*sin(p.x/620.+sin(p.y/470.))+.25*sin(p.y/340.+p.x/1100.);
float grain=frac(sin(dot(floor(p/4),float2(12.9898,78.233)))*43758.5453);
float ripple=pow(.5+.5*sin(p.x/22.+5*sin(p.y/240.)),10)*.06;
float wash=pow(saturate(.5+.5*sin(p.x/920.+p.y/670.)),4);
return lerp(float3(.19,.125,.061),float3(.38,.29,.16),saturate(.25+.65*broad))*(.9+.12*grain-ripple)*(1-.16*wash);
''')
water=make('M_LabCoolantDepth',r'''
float2 p=P.xy;float streak=.5+.5*sin(p.x/74.+sin(p.y/140.));
float grit=frac(sin(dot(floor(p/13.),float2(12.9898,78.233)))*43758.5453);
return float3(.008,.095,.12)*(.65+.35*streak+.12*grit);
''',.28)
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);report=[]
for arena in ['Outskirts','SubterraneanLab','Beachhead']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+arena);count=0
 for a in es.get_all_level_actors():
  label=a.get_actor_label()
  if isinstance(a,unreal.StaticMeshActor):
   target=water if arena=='SubterraneanLab' and label=='Coolant channel' else sand if (arena=='Outskirts' and label=='Arena floor') or label=='Beach strip' else None
   if target:a.static_mesh_component.set_material(0,target);count+=1
  light=a.get_component_by_class(unreal.PointLightComponent)
  if light:light.set_intensity(min(light.get_editor_property('intensity'),180.))
  if label=='Arcade clarity postprocess':
   s=a.get_editor_property('settings');s.set_editor_property('override_auto_exposure_bias',True);s.set_editor_property('auto_exposure_bias',-.35 if arena=='SubterraneanLab' else -.55);a.set_editor_property('settings',s)
 ls.save_current_level();report.append({'arena':arena,'surfaces':count})
pathlib.Path('D:/UnReal/TroubleSquad/Saved/Logs/ArenaSurfaceReadability.json').write_text(json.dumps(report))
