import unreal,json
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);r=[]
for n in ['CoastalResearch','Outskirts','SubterraneanLab','Beachhead']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+n);tilted=[];count=0;craters=[]
 for a in es.get_all_level_actors():
  if not a.get_actor_label().startswith(('Reference polish |','Architecture |')):continue
  count+=1;rot=a.get_actor_rotation()
  if abs(rot.pitch)>.01 or abs(rot.roll)>.01:tilted.append(a.get_actor_label())
  if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_Crater':
   o,e=a.get_actor_bounds(False);craters.append({'min_z':o.z-e.z,'max_z':o.z+e.z})
 r.append({'map':n,'placements':count,'tilted':tilted,'craters':craters})
open('D:/UnReal/TroubleSquad/Saved/Logs/ArenaPlacementAcceptance.json','w').write(json.dumps(r,indent=2))
assert all(not x['tilted'] for x in r)
assert all(len(x['craters'])==3 for x in r if x['map'] in ['Outskirts','Beachhead'])
assert all(abs(c['min_z'])<.1 and c['max_z']>50 for x in r for c in x['craters'])
unreal.log('ARENA_PLACEMENT_ACCEPTANCE_PASS')
