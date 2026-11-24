import bpy,pathlib
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/LabRebuild');bpy.ops.wm.open_mainfile(filepath=str(root/'SubterraneanLabRebuilt.blend'));ground=bpy.data.objects['SM_LabRebuildDeck'];changed=0
for v in ground.data.vertices:
 if 30.9<v.co.x<31.1 and 6.69<v.co.y<6.91 and .07<v.co.z<.12:
  v.co.x=19.;changed+=1
assert changed>0,changed
ground.data.update();bpy.ops.object.select_all(action='DESELECT');ground.select_set(True);bpy.context.view_layer.objects.active=ground
bpy.ops.export_scene.fbx(filepath=str(root/'SM_LabRebuildDeck.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE');bpy.ops.wm.save_as_mainfile(filepath=str(root/'SubterraneanLabRebuilt.blend'));bpy.context.scene.render.filepath=str(root/'LabOverview.png');bpy.ops.render.render(write_still=True);print('LAB_GUIDE_TRIMMED',changed)
