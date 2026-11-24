import bpy,math,pathlib
r=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/CombatVisuals/ReadableFX');r.mkdir(exist_ok=True)
for name in ['Dart','Feather','Crescent','ImpactStar','Mine','Flame']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 if name in ['Dart','Feather']:
  v=[(1.1,0,0),(-.5,-.15,0),(-.5,.15,0),(-.45,0,.15),(-.45,0,-.15)];f=[(0,1,3),(0,3,2),(0,2,4),(0,4,1),(1,4,2),(1,2,3)]
  if name=='Feather':
   for sign in [-1,1]:
    for i in range(4):
     x=.55-i*.25;n=len(v);v.extend([(x,0,0),(x-.4,sign*(.24+i*.04),0),(x-.5,0,0)]);f.append((n,n+1,n+2))
 elif name=='Crescent':
  v=[];f=[]
  for i in range(17):
   a=math.radians(-80+i*10)
   for radius,z in [(1,0),(.65,.04),(.7,-.04)]:v.append((math.cos(a)*radius,math.sin(a)*radius,z))
  for i in range(16):
   n=i*3;f.extend([(n,n+3,n+4,n+1),(n+1,n+4,n+5,n+2),(n+2,n+5,n+3,n)])
 elif name=='ImpactStar':
  v=[(0,0,.12)];f=[]
  for i in range(16):a=math.tau*i/16;rad=1 if i%2==0 else .24;v.append((math.cos(a)*rad,math.sin(a)*rad,0))
  for i in range(16):f.append((0,i+1,(i+1)%16+1))
 elif name=='Mine':
  bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=.55)
  for i in range(6):
   a=math.tau*i/6;bpy.ops.mesh.primitive_cone_add(vertices=4,radius1=.17,radius2=.04,depth=.5,location=(math.cos(a)*.6,math.sin(a)*.6,0));bpy.context.object.rotation_euler=(0,math.pi/2,a)
  bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=bpy.context.selected_objects[0];bpy.ops.object.join();v=[];f=[]
 else:
  v=[];f=[]
  for i in range(6):
   a=math.tau*i/6;n=len(v);v.extend([(0,0,0),(math.cos(a)*.35,math.sin(a)*.35,.2),(math.cos(a+.5)*.16,math.sin(a+.5)*.16,.8),(0,0,1.4)]);f.extend([(n,n+1,n+2),(n+1,n+3,n+2)])
 if v:
  m=bpy.data.meshes.new(name);m.from_pydata(v,[],f);m.update();o=bpy.data.objects.new('SM_FX_'+name,m);bpy.context.collection.objects.link(o);o.select_set(True);bpy.context.view_layer.objects.active=o
 else:o=bpy.context.object;o.name='SM_FX_'+name
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 bpy.ops.export_scene.fbx(filepath=str(r/('SM_FX_'+name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',bake_anim=False)
 print('READABLE_FX_READY',name,len(o.data.vertices))
