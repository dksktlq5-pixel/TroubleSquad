import unreal
root='/Game/TroubleSquad/CombatVisuals/GeneralWeapons';ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);unreal.EditorAssetLibrary.make_directory(root+'/Preview')
path=root+'/Preview/L_WeaponModels';assert ls.new_level(path)
world=unreal.EditorLevelLibrary.get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
for i,n in enumerate(['Train','Boomerang','Crescent','Blade','Tibber','Crossbow','Cyclone']):
 m=unreal.load_asset(root+'/Models/SM_GW_'+n);a=es.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector((i%4-1.5)*580,300-(i//4)*650,70 if n!='Tibber' else 0));a.set_actor_label('Model review | '+n);a.static_mesh_component.set_static_mesh(m);a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-25 if n!='Tibber' else -90,roll=0),False)
start=es.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(0,-1600,1600));start.set_actor_rotation(unreal.Rotator(pitch=-45,yaw=90,roll=0),False)
light=es.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1500));light.set_actor_rotation(unreal.Rotator(pitch=-55,yaw=-30,roll=0),False);light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(4)
for x in [-750,0,750]:
 l=es.spawn_actor_from_class(unreal.PointLight,unreal.Vector(x,-100,700));c=l.get_component_by_class(unreal.PointLightComponent);c.set_intensity(24000);c.set_editor_property('attenuation_radius',1800)
a=es.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-35));a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.set_actor_scale3d(unreal.Vector(30,20,.5));a.set_actor_label('Model review stage')
v=es.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());v.set_editor_property('unbound',True);s=v.get_editor_property('settings');s.set_editor_property('override_auto_exposure_bias',True);s.set_editor_property('auto_exposure_bias',0);s.set_editor_property('override_bloom_intensity',True);s.set_editor_property('bloom_intensity',.15);v.set_editor_property('settings',s)
assert ls.save_current_level();unreal.log('WEAPON_MODEL_PREVIEW_LEVEL_SAVED')
