import unreal,json
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);out={}
for arena in ['Outskirts','SubterraneanLab','Beachhead']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+arena);rows=[]
 for a in es.get_all_level_actors():
  light=a.get_component_by_class(unreal.LocalLightComponent)
  if light:rows.append((a.get_actor_label(),light.get_editor_property('intensity'),light.get_editor_property('attenuation_radius')))
 out[arena]=rows
with open('D:/UnReal/TroubleSquad/Saved/Logs/ArenaLightAudit.json','w') as f:json.dump(out,f)
