import bpy,pathlib,json
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/ReferenceBuildings')
for row in json.loads((root/'BuildingManifest.json').read_text(encoding='utf-8'))['buildings']:
 bpy.ops.wm.open_mainfile(filepath=str(root/(row['id']+'.blend')));bpy.ops.object.select_all(action='DESELECT');model=bpy.data.objects['SM_CRB_'+row['id']];model.select_set(True);bpy.context.view_layer.objects.active=model;bpy.data.objects['UCX_'+model.name+'_00'].select_set(True)
 bpy.ops.export_scene.fbx(filepath=str(root/row['fbx']),use_selection=True,object_types={'MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
