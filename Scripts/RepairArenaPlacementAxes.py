import unreal,json
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);r=[]
for n in ['CoastalResearch','Outskirts','SubterraneanLab','Beachhead']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+n);count=0
 for a in es.get_all_level_actors():
  label=a.get_actor_label()
  if label.startswith(('Reference polish |','Architecture |')):
   rot=a.get_actor_rotation()
   if abs(rot.pitch)>.01 or abs(rot.roll)>.01:
    a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=rot.yaw+rot.pitch,roll=0),False);count+=1
  if label=='Shell crater markings':a.set_actor_hidden_in_game(True)
 ls.save_current_level();r.append({'map':n,'fixed_tilted_placements':count})
exec(open('D:/UnReal/TroubleSquad/Scripts/ImportReferenceArchitecture.py',encoding='utf-8-sig').read())
open('D:/UnReal/TroubleSquad/Saved/Logs/ArenaRotationRepair.json','w').write(json.dumps(r))
