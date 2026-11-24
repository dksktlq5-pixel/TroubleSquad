import unreal
mesh=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/SM_Supercar')
for i,slot in enumerate(mesh.get_editor_property('static_materials')):
    mat=slot.get_editor_property('material_interface')
    unreal.log('SUPERCAR_SLOT '+str(i)+' '+str(slot.get_editor_property('material_slot_name'))+' '+str(mat.get_path_name() if mat else None))
