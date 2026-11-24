import unreal,pathlib,time,json
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def audit(path):
 assert ls.load_level(path)
 if path.endswith('_Rebuilt'):
  for actor in es.get_all_level_actors():
   if actor.get_actor_label() in ['ParkBroadleafCrown instances','ParkLeafyShrub instances']:es.destroy_actor(actor)
 actors=es.get_all_level_actors()
 return {'actors':len(actors),'rebuilt':sum(a.get_actor_label().startswith('Lab rebuilt | ') for a in actors),'starts':sum(isinstance(a,unreal.PlayerStart) for a in actors)}
old=audit('/Game/TroubleSquad/Maps/L_SubterraneanLab');assert old['rebuilt']==0 and old['actors']>=100,old
new=audit('/Game/TroubleSquad/Maps/L_SubterraneanLab_Rebuilt');assert new['rebuilt']==72 and new['starts']==4,new
pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/LabRebuild/MapReloadVerification.json').write_text(json.dumps({'original':old,'rebuilt':new},indent=2),encoding='utf-8')
world=unreal.EditorLevelLibrary.get_editor_world();cams=[a for a in es.get_all_level_actors() if a.get_actor_label()=='Lab Rebuild Overview'];assert len(cams)==1;cam=cams[0]
cam.set_actor_location(unreal.Vector(2400,7000,11500),False,False);cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(cam.get_actor_location(),unreal.Vector(0,0,150)),False)
for actor in es.get_all_level_actors():
 if actor.get_actor_label()=='Lab rebuilt | terrain and road':actor.static_mesh_component.set_editor_property('cast_shadow',False)
cam.camera_component.set_editor_property('projection_mode',unreal.CameraProjectionMode.PERSPECTIVE);cam.camera_component.set_editor_property('aspect_ratio',1.45);cam.camera_component.set_field_of_view(50)
for light in es.get_all_level_actors():
 if isinstance(light,unreal.DirectionalLight):light.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
assert ls.save_current_level()
unreal.EditorLevelLibrary.set_level_viewport_camera_info(cam.get_actor_location(),cam.get_actor_rotation());unreal.SystemLibrary.execute_console_command(world,'viewmode lit')
output='D:/UnReal/TroubleSquad/ArtSource/LabRebuild/UnrealLabOverview.png'
start=time.monotonic();requested=False;task=None
if hasattr(unreal,'EditorPythonScripting'):unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(dt):
 global requested,task,handle
 elapsed=time.monotonic()-start
 if elapsed>25 and not requested:
  requested=True;task=unreal.AutomationLibrary.take_high_res_screenshot(2030,1400,output,camera=cam);unreal.log('REBUILD_SCREENSHOT_REQUESTED')
 if (elapsed>38 and pathlib.Path(output).exists()) or elapsed>65:
  unreal.unregister_slate_post_tick_callback(handle);unreal.log('REBUILD_SCREENSHOT_EXISTS '+str(pathlib.Path(output).exists()));unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
