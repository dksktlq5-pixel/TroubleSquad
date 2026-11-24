import bpy,math,pathlib
from mathutils import Vector
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/Architecture');root.mkdir(exist_ok=True)
for model in ['Barricade','FortressWall','Console','Reactor','PipeRack','Crater','Bridge']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 colors={'Hull':(.035,.07,.085,1),'Steel':(.13,.22,.27,1),'Edge':(.55,.66,.67,1),'Cyan':(.03,.8,.9,1),'Gold':(.9,.53,.12,1),'Earth':(.28,.19,.10,1),'Sand':(.43,.31,.16,1)}
 mats={}
 for n,c in colors.items():
  m=bpy.data.materials.new(n);m.diffuse_color=c;mats[n]=m
 def finish(o,n,mat,bevel=.03):
  o.name=n;o.data.materials.append(mats[mat])
  if bevel:
   b=o.modifiers.new('Soft manufactured edges','BEVEL');b.width=bevel;b.segments=2;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=b.name)
  return o
 def box(n,p,d,mat='Hull',angle=0):
  bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=d;o.rotation_euler.z=angle;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,n,mat,min(d)*.06)
 def cyl(n,p,r,depth,mat='Steel',axis=None):
  bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=r,depth=depth,location=p);o=bpy.context.object
  if axis=='X':o.rotation_euler.y=math.pi/2
  return finish(o,n,mat,.02)
 if model in ['Barricade','FortressWall']:
  L=3 if model=='Barricade' else 7;H=1.1 if model=='Barricade' else 2.5
  box('wide footing',(0,0,.12),(L,.8,.24),'Steel')
  verts=[(x,y,z) for z,half in [(0,.4),(H,.22)] for x,y in [(-L/2,-half),(-L/2,half),(L/2,half),(L/2,-half)]]
  mesh=bpy.data.meshes.new('tapered hull');mesh.from_pydata(verts,[],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]);o=bpy.data.objects.new('armored barrier',mesh);bpy.context.collection.objects.link(o);finish(o,'armored barrier','Hull')
  box('silver cap',(0,0,H),(L,.49,.1),'Edge')
  box('light strip',(0,-.256,H-.18),(L*.88,.03,.05),'Cyan')
  for x in [-L*.38,L*.38]:box('reinforced end',(x,0,H*.46),(.22,.82,H*.9),'Steel')
  for i in range(5):box('hazard marking',(-L*.25+i*L*.125,-.34,.2),(L*.08,.04,.1),'Gold')
 elif model=='Console':
  box('base',(0,0,.12),(1,.85,.24),'Steel');box('column',(0,.1,.48),(.65,.6,.7),'Hull')
  o=box('sloped panel',(0,-.04,.9),(1.1,.8,.16),'Edge');o.rotation_euler.x=.35
  o=box('glass',(0,-.08,1.0),(.92,.62,.03),'Cyan');o.rotation_euler.x=.35
  for i in range(4):box('switch',(-.32+i*.21,-.4,.91),(.09,.06,.04),'Gold')
 elif model=='Reactor':
  cyl('base',(0,0,.12),1.15,.24,'Steel');cyl('shell',(0,0,1.0),.8,1.8,'Hull');cyl('fluid',(0,0,1.12),.69,1.55,'Cyan')
  for k in range(4):
   a=k*math.pi/2;box('support',(math.cos(a)*.76,math.sin(a)*.76,1.1),(.17,.17,2),'Steel')
  cyl('top',(0,0,2.02),.94,.22,'Edge')
  for k in range(3):cyl('band',(0,0,.42+k*.56),.83,.12,'Steel')
  for sign in [-1,1]:cyl('feed pipe',(sign*1.15,0,.5),.2,.9,'Steel','X');cyl('pipe cuff',(sign*1.5,0,.5),.25,.15,'Gold','X')
 elif model=='PipeRack':
  box('rack base',(0,0,.1),(3.5,1.1,.2),'Steel')
  for side in [-1,1]:box('rack upright',(side*1.55,0,.7),(.16,1.1,1.4),'Hull')
  for y in [-.33,0,.33]:
   cyl('pipeline',(0,y,.72),.12,3.7,'Steel','X')
   for x in [-1.45,0,1.45]:cyl('cuff',(x,y,.72),.15,.1,'Cyan','X')
 elif model=='Crater':
  verts=[];rings=[(0,0),(.8,.08),(1.15,.38),(1.4,.16),(1.65,0)]
  for ri,(r,z) in enumerate(rings):
   for i in range(32):
    a=math.tau*i/32;rr=r*(1+.06*math.sin(i*4.3));verts.append((math.cos(a)*rr,math.sin(a)*rr,z*(.85+.15*math.sin(i*1.7))))
  faces=[]
  for ri in range(4):
   for i in range(32):a=ri*32+i;b=ri*32+(i+1)%32;faces.append((a,a+32,b+32,b))
  mesh=bpy.data.meshes.new('eroded crater');mesh.from_pydata(verts,[],faces);o=bpy.data.objects.new('crater',mesh);bpy.context.collection.objects.link(o);finish(o,'crater','Earth',0)
  for p in o.data.polygons:p.use_smooth=True
 elif model=='Bridge':
  box('deck',(0,0,.12),(4,5,.24),'Steel')
  for x in [-1.95,1.95]:
   box('side curb',(x,0,.3),(.18,5,.25),'Edge');box('floor light',(x*.96,0,.445),(.05,4.8,.02),'Cyan')
   for y in [-1.8,0,1.8]:box('post',(x,y,.7),(.12,.12,1.1),'Hull')
   box('rail',(x,0,1.2),(.12,4.5,.12),'Edge')
  for y in [-1.9,-.9,.1,1.1,2.1]:box('deck seam',(0,y,.248),(3.6,.025,.005),'Hull')
 bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=next(o for o in bpy.context.selected_objects if o.type=='MESH');bpy.ops.object.join();o=bpy.context.object;o.name='SM_'+model;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 bpy.ops.export_scene.fbx(filepath=str(root/('SM_'+model+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',bake_anim=False)
 bpy.ops.wm.save_as_mainfile(filepath=str(root/(model+'.blend')))
 print('ARCHITECTURE_READY',model,len(o.data.vertices))
