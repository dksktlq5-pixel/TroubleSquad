import bpy,pathlib,math
r=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/GeneralWeaponModels');bpy.ops.wm.open_mainfile(filepath=str(r/'Crescent.blend'))
parts=list(bpy.data.collections['EDITABLE_PARTS'].objects)
for o in parts:
 if o.name.startswith('chain link'):
  i=0 if o.name=='chain link' else int(o.name.rsplit('.',1)[1]);a=i/12*math.pi;o.location=(-.17+.34*i/12-.35*math.sin(a),-.737+1.674*i/12,.11)
old=bpy.data.objects.get('SM_GW_Crescent');bpy.data.objects.remove(old,do_unlink=True)
bs=[]
for o in parts:
 q=o.copy();q.data=o.data.copy();bpy.context.scene.collection.objects.link(q);bs.append(q)
bpy.ops.object.select_all(action='DESELECT')
for o in bs:o.select_set(True)
bpy.context.view_layer.objects.active=bs[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_GW_Crescent';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.02);bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.export_scene.fbx(filepath=str(r/'SM_GW_Crescent.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z');bpy.ops.export_scene.gltf(filepath=str(r/'SM_GW_Crescent.glb'),export_format='GLB',use_selection=True)
bpy.ops.wm.save_as_mainfile(filepath=str(r/'Crescent.blend'));bpy.ops.render.render(write_still=True)
