import unreal
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert ls.load_level('/Game/TroubleSquad/Maps/L_SubterraneanLab_Rebuilt')
for a in es.get_all_level_actors():
 if '07_CryoPylon' in a.get_actor_label():
  a.static_mesh_component.set_collision_profile_name('NoCollision');a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);unreal.log('CRYO_NO_COLLISION '+a.get_actor_label())
assert ls.save_current_level()
unreal.SystemLibrary.quit_editor()
