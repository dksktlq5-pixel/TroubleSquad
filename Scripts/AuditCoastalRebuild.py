import unreal,json,pathlib
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert ls and es
assert ls.load_level('/Game/TroubleSquad/Maps/L_CoastalResearch')
rows=[]
for a in es.get_all_level_actors():
 c=a.get_component_by_class(unreal.StaticMeshComponent)
 rows.append({'label':a.get_actor_label(),'class':a.get_class().get_name(),'mesh':c.static_mesh.get_path_name() if c and c.static_mesh else None,'location':list(a.get_actor_location().to_tuple()),'scale':list(a.get_actor_scale3d().to_tuple())})
assets=[]
for p in unreal.EditorAssetLibrary.list_assets('/Game/TroubleSquad/Maps/CoastalResearchMeshes',recursive=False):
 m=unreal.load_asset(p)
 if isinstance(m,unreal.StaticMesh):
  b=m.get_bounds();assets.append({'path':p,'origin':b.origin.to_tuple(),'extent':b.box_extent.to_tuple()})
pathlib.Path('D:/UnReal/TroubleSquad/Saved/Logs/CoastalRebuildAudit.json').write_text(json.dumps({'actors':rows,'meshes':assets},ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
