import unreal,json
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);r={}
for n in ['Beachhead','Outskirts']:
 ls.load_level('/Game/TroubleSquad/Maps/L_'+n);r[n]=[]
 for a in es.get_all_level_actors():
  if a.get_actor_label().startswith('Architecture |'):
   p=a.get_actor_location();o,e=a.get_actor_bounds(False);c=a.static_mesh_component;m=c.static_mesh;b=m.get_bounds();r[n].append({'label':a.get_actor_label(),'mesh':m.get_name(),'location':[p.x,p.y,p.z],'extent':[e.x,e.y,e.z],'hidden':a.get_editor_property('hidden'),'rotation':str(a.get_actor_rotation()),'scale':str(a.get_actor_scale3d()),'mesh_extent':str(b.box_extent)})
open('D:/UnReal/TroubleSquad/Saved/Logs/ArchitectureBounds.json','w').write(json.dumps(r,indent=2))
