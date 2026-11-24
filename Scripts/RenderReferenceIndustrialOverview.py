import bpy,json,pathlib,math
from mathutils import Vector
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/ReferenceIndustrialModules');rows=sorted(json.loads((root/'BuildingManifest.json').read_text(encoding='utf-8'))['buildings'],key=lambda r:r['id']);assert len(rows)==11
bpy.ops.wm.read_factory_settings(use_empty=True)
for i,row in enumerate(rows):
 with bpy.data.libraries.load(str(root/(row['id']+'.blend')),link=False) as (a,b):b.objects=['SM_CRB_'+row['id']]
 obj=b.objects[0];bpy.context.collection.objects.link(obj);obj.location=((i%4-1.5)*14,(i//4-1)*15,0)
 bpy.ops.object.text_add(location=(obj.location.x,obj.location.y-6,.06));t=bpy.context.object;t.data.body=row['id'].replace('_','  ');t.data.size=.65;t.data.align_x='CENTER';t.data.extrude=.002
mat=bpy.data.materials.new('Studio slate');mat.diffuse_color=(.10,.14,.18,1);mat.use_nodes=True;mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.10,.14,.18,1)
bpy.ops.mesh.primitive_plane_add(size=230,location=(0,0,-.08));bpy.context.object.data.materials.append(mat)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=2400;scene.render.resolution_y=1600;scene.world=bpy.data.worlds.new('Studio');scene.world.color=(.25,.25,.25)
target=Vector((0,0,3));bpy.ops.object.camera_add(location=(28,-62,82));camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=70;scene.camera=camera
for loc,power,size in [((30,-45,90),130000,70),((-60,-15,50),85000,60),((0,60,80),150000,65)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.rotation_euler=(target-l.location).to_track_quat('-Z','Y').to_euler()
scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(root/'Preview/IndustrialModulesOverview.png');bpy.ops.wm.save_as_mainfile(filepath=str(root/'IndustrialModulesReview.blend'));bpy.ops.render.render(write_still=True)
