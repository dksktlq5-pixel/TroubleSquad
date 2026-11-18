"""Run with Blender --background --python. Original stylized robotic summon meshes."""
import bpy,math
from pathlib import Path
from mathutils import Vector
out=Path(__file__).resolve().parents[1]/'ArtSource'/'CombatSummons'
out.mkdir(parents=True,exist_ok=True)

def material(name,color,metal=0.0):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(*color,1)
    bs.inputs['Metallic'].default_value=metal;bs.inputs['Roughness'].default_value=.34
    return m

def surface(obj,mat):
    obj.data.materials.append(mat);return obj
def box(pos,size,mat,bevel=.04):
    bpy.ops.mesh.primitive_cube_add(size=1,location=pos);o=bpy.context.object;o.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=o.modifiers.new('machined edges','BEVEL');mod.width=bevel;mod.segments=2
    return surface(o,mat)
def ball(pos,size,mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=1,location=pos)
    o=bpy.context.object;o.scale=size
    for p in o.data.polygons:p.use_smooth=True
    return surface(o,mat)
def rod(a,b,r,mat,vertices=10):
    d=Vector(b)-Vector(a);mid=(Vector(a)+Vector(b))/2
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=d.length,location=mid)
    o=bpy.context.object;o.rotation_euler=d.to_track_quat('Z','Y').to_euler();return surface(o,mat)
def ring(pos,r,mat):
    bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=.025,major_segments=20,minor_segments=6,location=pos)
    return surface(bpy.context.object,mat)
def ear(pos,mat):
    bpy.ops.mesh.primitive_cone_add(vertices=4,radius1=.18,radius2=0,depth=.33,location=pos,rotation=(0,0,math.pi/4))
    return surface(bpy.context.object,mat)
def reset():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
def export(name):
    bpy.ops.object.select_all(action='SELECT')
    for o in list(bpy.context.selected_objects):
        bpy.context.view_layer.objects.active=o
        for mod in list(o.modifiers):bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.context.view_layer.objects.active=bpy.context.selected_objects[0]
    bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
    bpy.ops.export_scene.fbx(filepath=str(out/(name+'.fbx')),use_selection=True,object_types={'MESH'},
        axis_forward='-Y',axis_up='Z',apply_unit_scale=True,add_leaf_bones=False,bake_anim=False)
    print('SUMMON_EXPORTED',name,len(o.data.polygons))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC'
armor=material('SummonArmor',(.16,.035,.25),.55)
trim=material('SummonTrim',(.04,.10,.19),.65)
metal=material('SummonMetal',(.32,.38,.48),.7)
cyan=material('SummonCyan',(.03,.65,1))
pink=material('SummonPink',(.9,.05,.35))
visor=material('SummonVisor',(.008,.012,.035),.3)
gold=material('SummonGold',(1,.48,.06),.5)
for sign in (-1,1):
    box((sign*.31,-.03,.16),(.38,.55,.28),trim)
    rod((sign*.3,0,.28),(sign*.3,0,.68),.14,metal)
    box((sign*.31,-.25,.2),(.22,.08,.10),cyan,.02)
box((0,0,.91),(1.03,.65,.78),armor,.10)
box((0,-.36,.98),(.53,.12,.38),metal)
box((0,-.44,.98),(.26,.06,.24),pink,.03)
ball((0,-.02,1.48),(.48,.39,.38),armor)
box((0,-.38,1.49),(.65,.10,.27),visor,.05)
for sign in (-1,1):
    ball((sign*.15,-.455,1.52),(.055,.025,.045),cyan)
    rod((sign*.37,-.08,1.77),(sign*.37,.09,1.77),.16,metal)
    ball((sign*.37,-.09,1.77),(.105,.03,.105),pink)
    ball((sign*.64,0,1.15),(.25,.27,.25),metal)
    box((sign*.7,-.02,.86),(.33,.42,.47),armor,.07)
    box((sign*.7,-.21,.64),(.38,.36,.25),trim,.05)
    for digit in (-1,1):rod((sign*.7+digit*.10,-.39,.64),(sign*.7+digit*.10,-.48,.55),.045,gold,8)
    rod((sign*.26,.36,.72),(sign*.26,.47,1.10),.13,trim)
    ball((sign*.26,.47,1.10),(.09,.09,.055),cyan)
export('SM_RobotTibber')
reset()
box((0,0,.18),(.92,.63,.11),trim,.04)
for sign in (-1,1):
    page=box((sign*.24,-.02,.24),(.43,.56,.06),metal,.01);page.rotation_euler.y=sign*.13
    ring((sign*.59,.08,.68),.24,cyan)
    rod((sign*.34,.08,.57),(sign*.61,.08,.64),.055,armor)
ball((0,.03,.61),(.36,.30,.28),armor)
ball((0,-.10,.98),(.38,.32,.31),metal)
box((0,-.39,.99),(.57,.075,.23),visor,.05)
for sign in (-1,1):
    ear((sign*.27,-.10,1.30),cyan)
    ball((sign*.14,-.445,1.03),(.048,.025,.055),cyan)
    for dz in (-.045,.04):rod((sign*.23,-.44,.94+dz),(sign*.43,-.43,.96+dz*1.5),.012,cyan,6)
ball((0,-.452,.94),(.035,.025,.026),pink)
ball((0,-.265,.58),(.15,.07,.12),cyan)
tail=[(0,.27,.56),(.07,.47,.63),(.11,.58,.86),(.04,.56,1.05),(-.08,.48,1.13)]
for a,b in zip(tail,tail[1:]):rod(a,b,.06,cyan)
export('SM_YuumiDrone')
