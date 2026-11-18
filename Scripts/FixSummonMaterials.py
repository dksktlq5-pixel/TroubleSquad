import unreal
for name in ['SM_RobotTibber','SM_YuumiDrone']:
 mesh=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/'+name)
 for i,slot in enumerate(mesh.get_editor_property('static_materials')):
  key=str(slot.get_editor_property('imported_material_slot_name')).split('.')[0]
  mat=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/M_'+key)
  assert mat,key
  mesh.set_material(i,mat)
  assert mesh.get_material(i)==mat
 assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
 unreal.log('SUMMON_MATERIALS_FIXED '+name)
