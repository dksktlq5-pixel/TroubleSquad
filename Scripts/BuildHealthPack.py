import bpy,math
from pathlib import Path
out=Path('D:/UnReal/TroubleSquad/ArtSource/HealthPack')
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
mats={}
for n,c in {'Stone':(.07,.18,.22,1),'Steel':(.24,.43,.48,1),'Cyan':(.02,.8,1,1),'Green':(.08,1,.24,1)}.items():
 m=bpy.data.materials.new(n);m.diffuse_color=c;m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=c;p.inputs['Metallic'].default_value=.35;p.inputs['Roughness'].default_value=.3
 if n in ('Cyan','Green'):p.inputs['Emission Color'].default_value=c;p.inputs['Emission Strength'].default_value=3
 mats[n]=m
parts=[]
def cube(n,loc,scale,mat,bevel=.025):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=n;o.scale=scale;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mats[mat]);b=o.modifiers.new('Rounded edges','BEVEL');b.width=bevel;b.segments=3;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=b.name);parts.append(o);return o
bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=.4,depth=.08,location=(0,0,.04));o=bpy.context.object;o.data.materials.append(mats['Stone']);parts.append(o)
for i in range(8):
 a=i*math.pi/4;o=cube('Pedestal tooth',(.36*math.cos(a),.36*math.sin(a),.05),(.23,.13,.09),'Steel',.015);o.rotation_euler.z=a
for radius,z,thick in ((.28,.095,.025),(.18,.11,.018)):
 bpy.ops.mesh.primitive_torus_add(major_radius=radius,minor_radius=thick,major_segments=48,minor_segments=8,location=(0,0,z));o=bpy.context.object;o.data.materials.append(mats['Cyan']);parts.append(o)
cube('Cross upright',(0,0,.43),(.14,.08,.44),'Green')
cube('Cross arms',(0,0,.43),(.36,.08,.14),'Green')
cube('Cross core backing',(0,.045,.43),(.17,.04,.46),'Cyan',.015)
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_HealthPack';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.wm.save_as_mainfile(filepath=str(out/'HealthPack.blend'))
bpy.ops.export_scene.fbx(filepath=str(out/'SM_HealthPack.fbx'),use_selection=True,apply_unit_scale=True,add_leaf_bones=False,object_types={'MESH'})
