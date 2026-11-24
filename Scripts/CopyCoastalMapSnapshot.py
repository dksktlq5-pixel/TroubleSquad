import unreal,os,json,pathlib
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);src=os.environ['TS_MAP_COPY_SOURCE'];dst=os.environ['TS_MAP_COPY_DEST'];assert ls.load_level(src)
actors=es.get_all_level_actors();count=sum(a.get_actor_label().startswith('Rebuilt | ') for a in actors);unreal.log('MAP_COPY '+src+' -> '+dst+' rebuild actors='+str(count));assert unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(),dst);unreal.SystemLibrary.quit_editor()
