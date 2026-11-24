import unreal,json
out={}
for name in ['Ghoul','Pix','Swarmer']:
 mat=unreal.load_asset('/Game/TroubleSquad/Enemies/M_'+name)
 tex=unreal.MaterialEditingLibrary.get_used_textures(mat)
 mesh=unreal.load_asset('/Game/TroubleSquad/Enemies/SM_'+name)
 rig=unreal.load_asset('/Game/TroubleSquad/Enemies/Locomotion/'+name+'/SK_'+name)
 unreal.log('ENEMY_MAT '+name+' textures='+str([t.get_path_name() for t in tex])+' static='+str(mesh.get_material(0))+' rig='+str(rig.get_editor_property('materials')))
