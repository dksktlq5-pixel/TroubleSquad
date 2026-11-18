import bpy, math
bpy.ops.wm.read_factory_settings(use_empty=True)
verts=[]
faces=[]
# A raised central quill and two swept vanes, with the tip facing local +X.
for i in range(13):
 t=i/12
 x=-30+65*t
 width=13*math.sin(math.pi*t)**.7
 sweep=7*math.sin(math.pi*t)
 verts.extend([(x-sweep,-width,2),(x,0,5),(x-sweep,width,2)])
for i in range(12):
 a=i*3
 faces.extend([(a,a+3,a+4,a+1),(a+1,a+4,a+5,a+2)])
mesh=bpy.data.meshes.new("FeatherVanes")
mesh.from_pydata(verts,[],faces)
mesh.update()
obj=bpy.data.objects.new("SM_XayahFeather",mesh)
bpy.context.collection.objects.link(obj)
mat=bpy.data.materials.new("FeatherVane")
mat.diffuse_color=(.38,.055,.55,1)
obj.data.materials.append(mat)
bpy.context.view_layer.objects.active=obj
obj.select_set(True)
solid=obj.modifiers.new("Vane thickness","SOLIDIFY")
solid.thickness=1.5
bpy.ops.object.modifier_apply(modifier=solid.name)
bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=1.7,depth=63,location=(1,0,5),rotation=(0,math.pi/2,0))
quill=bpy.context.object
gold=bpy.data.materials.new("FeatherQuill")
gold.diffuse_color=(1,.65,.16,1)
quill.data.materials.append(gold)
obj.select_set(True)
bpy.context.view_layer.objects.active=obj
bpy.ops.object.join()
bpy.ops.export_scene.fbx(filepath=r"D:/UnReal/TroubleSquad/ArtSource/CombatVisuals/SM_XayahFeather.fbx",use_selection=True,object_types={"MESH"},axis_forward="-Y",axis_up="Z",apply_unit_scale=False,global_scale=.01,add_leaf_bones=False,bake_anim=False)
print("FEATHER_EXPORTED")
