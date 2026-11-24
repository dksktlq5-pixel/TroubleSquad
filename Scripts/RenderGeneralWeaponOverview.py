import bpy,pathlib,math,json
from mathutils import Vector
r=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/GeneralWeaponModels');bpy.ops.wm.read_factory_settings(use_empty=True)
items=[('Train','최후의 도시 대중교통'),('Boomerang','칼날 부메랑'),('Crescent','사자의 비가'),('Blade','메아리치는 박쥐칼날'),('Tibber','티버'),('Crossbow','전투 토끼 석궁'),('Cyclone','회오리 칼날')]
manifest=json.loads((r/'ModelManifest.json').read_text())
font=bpy.data.fonts.load('C:/Windows/Fonts/malgun.ttf')
for i,(n,label) in enumerate(items):
 before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(r/('SM_GW_'+n+'.glb')));objs=[o for o in bpy.data.objects if o not in before and o.type=='MESH'];bpy.ops.object.select_all(action='DESELECT')
 for o in objs:o.select_set(True)
 bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join();o=bpy.context.object;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)

 for rec in manifest:
  if rec['name']==n:rec.update(vertices=len(o.data.vertices),triangles=sum(len(f.vertices)-2 for f in o.data.polygons),dimensions_m=list(o.dimensions))
 x=(i%4-1.5)*4.8;y=2.6-(i//4)*5.4;low=min(v.co.z for v in o.data.vertices);o.location+=Vector((x,y,-low+.18))
 o.rotation_euler.z=math.radians(-45 if n=='Tibber' else -12)
 bpy.ops.object.text_add(location=(x,y-2.02,.025));text=bpy.context.object;text.name='Label_'+n;text.data.body=label;text.data.font=font;text.data.align_x='CENTER';text.data.size=.31;text.data.extrude=.001
 m=bpy.data.materials.get('Label')
 if not m:m=bpy.data.materials.new('Label');m.diffuse_color=(.72,.84,.94,1);m.use_nodes=True;m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.72,.84,.94,1)
 text.data.materials.append(m)
(r/'ModelManifest.json').write_text(json.dumps(manifest,indent=2))
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.03));m=bpy.data.materials.new('Midnight stage');m.diffuse_color=(.015,.024,.045,1);m.use_nodes=True;p=m.node_tree.nodes['Principled BSDF'];p.inputs['Base Color'].default_value=m.diffuse_color;p.inputs['Roughness'].default_value=.8;bpy.context.object.data.materials.append(m)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=1920;scene.render.resolution_y=1152;scene.render.resolution_percentage=100;scene.world=bpy.data.worlds.new('StudioWorld');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.08,.12,.2,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.4
look=Vector((0,-.15,.5));bpy.ops.object.camera_add(location=(3,-12,22));c=bpy.context.object;c.rotation_euler=(look-c.location).to_track_quat('-Z','Y').to_euler();c.data.type='ORTHO';c.data.ortho_scale=22;scene.camera=c
for x in [-7,0,7]:
 bpy.ops.object.light_add(type='AREA',location=(x,-1,10));o=bpy.context.object;o.data.energy=2200;o.data.size=8;o.rotation_euler=(Vector((x,0,0))-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(r/'Preview/WeaponsOverview.png');scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='AgX';bpy.ops.wm.save_as_mainfile(filepath=str(r/'WeaponsOverview.blend'));bpy.ops.render.render(write_still=True)
