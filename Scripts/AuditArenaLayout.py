import unreal,json
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); ac=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out={}
for n in ['Outskirts','SubterraneanLab','Beachhead']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+n)
 arr=[]
 for a in ac.get_all_level_actors():
  if isinstance(a,unreal.StaticMeshActor):
   p=a.get_actor_location(); o,e=a.get_actor_bounds(False)
   arr.append({'label':a.get_actor_label(),'p':[p.x,p.y,p.z],'ext':[e.x,e.y,e.z]})
 out[n]=arr
with open('D:/UnReal/TroubleSquad/Saved/Logs/ArenaLayoutAudit.json','w',encoding='utf-8') as f:json.dump(out,f)
unreal.log('LAYOUT_AUDIT_DONE')
