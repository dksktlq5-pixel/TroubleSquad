import unreal
for name in ['SM_RobotTibber','SM_YuumiDrone']:
 mesh=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/'+name)
 for slot in mesh.get_editor_property('static_materials'):
  mat=slot.get_editor_property('material_interface')
  unreal.log('ROBOT_MATERIAL '+name+' '+str(slot.get_editor_property('material_slot_name'))+' '+str(mat.get_path_name() if mat else None))
