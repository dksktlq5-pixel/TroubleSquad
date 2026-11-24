import unreal,json,math
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert ls.load_level('/Game/TroubleSquad/Maps/L_SubterraneanLab_Rebuilt')
for a in es.get_all_level_actors():
 if isinstance(a,unreal.StaticMeshActor) and '06_ConsoleIsland' in a.get_actor_label():
  p=a.get_actor_location()
  if -2100<p.y<-1800:a.set_actor_location(unreal.Vector(p.x,-2400,p.z),False,False)
cube=unreal.load_asset('/Engine/BasicShapes/Cube');assert cube
for a in es.get_all_level_actors():
 if a.get_actor_label().startswith('Lab traversal collision |'):es.destroy_actor(a)
 if isinstance(a,unreal.StaticMeshActor) and '08_ServiceBridge' in a.get_actor_label():
  a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
  a.static_mesh_component.set_collision_profile_name('NoCollision')
rows=[]
def block(name,loc,size,roll=0):
 a=es.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc));a.set_actor_label('Lab traversal collision | '+name);a.set_folder_path('Lab traversal collision');c=a.static_mesh_component;c.set_static_mesh(cube);c.set_collision_profile_name('BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS);c.set_editor_property('cast_shadow',False);a.set_actor_hidden_in_game(True);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)));a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=roll),False);rows.append({'name':name,'position':loc,'size':size,'roll':roll})
bridges=(-2600,-400,1800)
for y,right in ((-1200,2200),(1200,3000)):
 x=-4600
 for bx in bridges:
  end=bx-250
  if end>x:block('water barrier',((x+end)/2,y,150),(end-x,600,700))
  x=bx+250
 if x<right:block('water barrier',((x+right)/2,y,150),(right-x,600,700))
 for bx in bridges:
  block('smooth bridge deck',(bx,y,36),(480,620,16))
  angle=math.degrees(math.atan2(39,200))
  for sign in (-1,1):block('bridge approach ramp',(bx,y+sign*410,19.5),(480,204,10),sign*angle)
  for side in (-1,1):block('bridge side rail',(bx+side*245,y,130),(16,620,260))
assert ls.save_current_level()
from pathlib import Path
Path('D:/UnReal/TroubleSquad/ArtSource/LabRebuild/BridgeWaterCollision.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
unreal.log('LAB_BRIDGE_WATER_COLLISION_FIXED proxies='+str(len(rows)))
unreal.SystemLibrary.quit_editor()
