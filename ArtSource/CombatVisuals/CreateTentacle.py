import bpy, math
from mathutils import Vector
bpy.ops.wm.read_factory_settings(use_empty=True)
body=bpy.data.materials.new("TentacleBody")
body.diffuse_color=(.025,.32,.25,1)
cup=bpy.data.materials.new("TentacleSuckers")
cup.diffuse_color=(.22,.8,.58,1)
vertices=[]
faces=[]
rings=25
sides=14
def center(t):
 return Vector((85*t*t-35*t,0,220*t-35*t*t*t))
for ring in range(rings):
 t=ring/(rings-1)
 pos=center(t)
 tangent=Vector((170*t-35,0,220-105*t*t)).normalized()
 across=Vector((0,1,0))
 normal=tangent.cross(across).normalized()
 radius=29*(1-t)**.75+2.5
 for side in range(sides):
  angle=2*math.pi*side/sides
  p=pos+radius*(math.cos(angle)*across+math.sin(angle)*normal)
  vertices.append(tuple(p))
for ring in range(rings-1):
 for side in range(sides):
  a=ring*sides+side
  b=ring*sides+(side+1)%sides
  faces.append((a,b,b+sides,a+sides))
faces.append(tuple(reversed(range(sides))))
faces.append(tuple((rings-1)*sides+s for s in range(sides)))
mesh=bpy.data.meshes.new("TentacleTube")
mesh.from_pydata(vertices,[],faces)
mesh.update()
obj=bpy.data.objects.new("SM_IllaoiTentacle",mesh)
bpy.context.collection.objects.link(obj)
obj.data.materials.append(body)
for p in obj.data.polygons: p.use_smooth=True
parts=[obj]
for i in range(9):
 t=.14+i*.087
 pos=center(t)
 radius=29*(1-t)**.75+2.5
 pos.y=-radius
 bpy.ops.mesh.primitive_torus_add(major_segments=12,minor_segments=6,location=pos,rotation=(math.pi/2,0,0),major_radius=9*(1-t)+2,minor_radius=2.3*(1-t)+.6)
 piece=bpy.context.object
 piece.data.materials.append(cup)
 for p in piece.data.polygons: p.use_smooth=True
 parts.append(piece)
bpy.ops.object.select_all(action="DESELECT")
for part in parts: part.select_set(True)
bpy.context.view_layer.objects.active=obj
bpy.ops.object.join()
bpy.ops.export_scene.fbx(filepath=r"D:/UnReal/TroubleSquad/ArtSource/CombatVisuals/SM_IllaoiTentacle.fbx",use_selection=True,object_types={"MESH"},axis_forward="-Y",axis_up="Z",apply_unit_scale=False,global_scale=.01,add_leaf_bones=False,bake_anim=False)
print("TENTACLE_EXPORTED",len(obj.data.vertices))
