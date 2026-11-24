import bpy, math, pathlib, json
from mathutils import Vector
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/GeneralWeaponModels')
NAMES=['Train','Boomerang','Crescent','Blade','Tibber','Crossbow','Cyclone']
REPORT=[]
def setup():
 bpy.ops.wm.read_factory_settings(use_empty=True)
 global M
 M={}
 palette={'Armor':(.045,.065,.12,1),'Steel':(.28,.40,.51,1),'Silver':(.65,.77,.84,1),'Purple':(.27,.045,.42,1),'Gold':(.95,.49,.06,1),'Green':(.045,.36,.23,1),'Cyan':(.025,.8,1,1),'Pink':(1,.045,.45,1),'Black':(.012,.018,.032,1)}
 for n,c in palette.items():
  m=bpy.data.materials.new('GW_'+n);m.diffuse_color=c;m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=c;p.inputs['Metallic'].default_value=.68 if n not in ['Cyan','Pink'] else .2;p.inputs['Roughness'].default_value=.28
  if n in ['Cyan','Pink']:p.inputs['Emission Color'].default_value=c;p.inputs['Emission Strength'].default_value=3.2
  M[n]=m
 bpy.context.scene.unit_settings.system='METRIC'
def finish(o,n,mat,bevel=.025):
 o.name=n;o.data.materials.append(M[mat])
 if bevel:
  q=o.modifiers.new('Edge highlights','BEVEL');q.width=bevel;q.segments=2;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=q.name)
 return o
def box(n,p,d,mat='Armor',angle=0):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=d;o.rotation_euler.z=angle;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,n,mat,min(d)*.12)
def cyl(n,p,r,h,mat='Steel',axis='Z',vertices=24):
 bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=h,location=p);o=bpy.context.object
 if axis=='X':o.rotation_euler.y=math.pi/2
 if axis=='Y':o.rotation_euler.x=math.pi/2
 return finish(o,n,mat,.02)
def torus(n,p,r,t,mat='Cyan',axis='Z'):
 bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=t,major_segments=32,minor_segments=8,location=p);o=bpy.context.object
 if axis=='X':o.rotation_euler.y=math.pi/2
 if axis=='Y':o.rotation_euler.x=math.pi/2
 return finish(o,n,mat,0)
def prism(n,points,z,h,mat='Armor'):
 v=[(x,y,z+s*h/2) for s in [-1,1] for x,y in points];k=len(points);f=[tuple(reversed(range(k))),tuple(range(k,2*k))]+[(i,(i+1)%k,(i+1)%k+k,i+k) for i in range(k)]
 mesh=bpy.data.meshes.new(n);mesh.from_pydata(v,[],f);mesh.update();o=bpy.data.objects.new(n,mesh);bpy.context.collection.objects.link(o);return finish(o,n,mat,min(.02,h*.12))
def sphere(n,p,d,mat='Purple'):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=20,ring_count=12,location=p);o=bpy.context.object;o.scale=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,n,mat,0)
def cone(n,p,r,depth,mat='Silver',direction=(0,0,1)):
 bpy.ops.mesh.primitive_cone_add(vertices=6,radius1=r,radius2=.015,depth=depth,location=p);o=bpy.context.object;o.rotation_euler=Vector(direction).to_track_quat('Z','Y').to_euler();return finish(o,n,mat,.008)
def crescent(n,center,r,angle,mat='Gold'):
 cx,cy=center;pts=[]
 for i in range(21):
  a=math.radians(-75+i*9);pts.append((r*math.cos(a),r*math.sin(a)))
 for i in reversed(range(21)):
  a=math.radians(-75+i*9);t=i/20;width=.30*math.sin(math.pi*t);pts.append(((r-width)*math.cos(a)-.12*math.sin(math.pi*t),(r-width)*math.sin(a)))
 def trans(scale):return [(cx+scale*(x*math.cos(angle)-y*math.sin(angle)),cy+scale*(x*math.sin(angle)+y*math.cos(angle))) for x,y in pts]
 prism(n,trans(1),.03,.13,mat)
 # Dark spine is inset so the cutting silhouette stays gold.
 o=prism(n+' spine',trans(.86),.12,.045,'Armor');return o
for name in NAMES:
 setup()
 if name=='Train':
  box('armored chassis',(-.2,0,0),(2.7,1.32,.92),'Armor');box('lower purple keel',(-.05,0,-.45),(3.1,1.48,.25),'Purple')
  for side in [-1,1]:
   box('silver flank',(-.15,side*.67,.1),(1.9,.12,.76),'Steel');box('cyan flank stripe',(-.5,side*.745,.09),(1.1,.035,.07),'Cyan')
   for x in [-.95,-.45,.05]:box('side cooling fin',(x,side*.75,-.13),(.12,.05,.34),'Black')
   for x in [-.9,.65]:cyl('magnetic bogie',(x,side*.69,-.5),.25,.13,'Black','Y');torus('bogie trim',(x,side*.77,-.5),.18,.025,'Cyan','Y')
  cyl('forward reactor shroud',(.98,0,.12),.67,.7,'Steel','X');cyl('deep muzzle', (1.36,0,.12),.55,.13,'Black','X');cyl('plasma lens',(1.44,0,.12),.46,.04,'Cyan','X');torus('reactor rim',(1.49,0,.12),.57,.075,'Silver','X');torus('inner ion ring',(1.53,0,.12),.34,.038,'Cyan','X')
  box('reactor vertical bracket',(1.57,0,.12),(.07,.09,.97),'Silver')
  prism('snowplow prow',[(.3,-.78),(1.95,-.72),(1.48,0),(1.95,.72),(.3,.78)],-.63,.22,'Purple')
  for side in [-1,1]:prism('plow cyan edge',[(.65,side*.73),(1.94,side*.70),(1.50,side*.13)],-.49,.04,'Cyan')
  box('roof panel',(-.52,0,.53),(1.45,1.13,.13),'Silver');box('roof stripe',(-.52,0,.61),(1.05,.15,.025),'Purple');box('rear battery',(-1.63,0,.12),(.25,1.02,.75),'Purple')
 elif name=='Boomerang':
  for sign in [-1,1]:
   pts=[(.02,-.18),(.65,-.38),(1.35,-.3),(1.77,.16),(1.35,.02),(.9,.07),(.46,.39),(.20,.2)]
   p=[(sign*x,sign*y) for x,y in pts];prism('gold hooked cutting wing',p,0,.16,'Gold')
   q=[(sign*x,sign*y) for x,y in [(.16,-.1),(.67,-.25),(1.24,-.21),(1.48,-.03),(.87,-.04),(.42,.26)]];prism('emerald wing inlay',q,.11,.065,'Green')
   q=[(sign*x,sign*y) for x,y in [(.30,.14),(.86,-.05),(1.28,-.04),(.84,.03),(.37,.23)]];prism('ion cutting edge',q,.16,.025,'Cyan')
  cyl('axis housing',(0,0,.02),.37,.27,'Armor');cyl('axis cap',(0,0,.19),.25,.08,'Steel');torus('emerald core',(0,0,.25),.15,.025,'Cyan');box('core bridge',(0,0,.25),(.27,.09,.06),'Silver',.35)
 elif name=='Crescent':
  for i,(cx,cy,ang) in enumerate([(-.45,-.45,-.8),(.45,.65,2.34)]):
   crescent('lion crescent '+str(i),(cx,cy),.95,ang)
   cyl('blue blade jewel',(cx+.40*math.cos(ang),cy+.40*math.sin(ang),.18),.13,.07,'Cyan')
  for i in range(13):
   a=i/12*math.pi;torus('chain link',(-.17+.34*i/12-.35*math.sin(a),-.737+1.674*i/12,.11),.085,.025,'Gold',axis='Z' if i%2 else 'X')
 elif name=='Blade':
  for k,(dx,dy,a,sc) in enumerate([(0,0,0,1),(-.25,-.72,-.13,.78),(-.25,.72,.13,.78)]):
   def pp(points):return [(dx+sc*(x*math.cos(a)-y*math.sin(a)),dy+sc*(x*math.sin(a)+y*math.cos(a))) for x,y in points]
   prism('bat blade '+str(k),pp([(-1,-.17),(.3,-.18),(1.6,0),(.3,.18),(-1,.17),(-.7,0)]),.02,.14,'Armor')
   prism('silver sharpened edge',pp([(-.7,-.15),(.3,-.18),(1.6,0),(.28,-.045)]),.10,.035,'Silver')
   prism('cyan blade core',pp([(-.7,-.06),(.45,-.07),(1.25,0),(.3,.07),(-.7,.06)]),.14,.028,'Cyan')
   for side in [-1,1]:prism('bat crossguard',pp([(-.6,side*.03),(-.8,side*.48),(-.35,side*.30),(-.25,side*.11)]),.12,.10,'Steel')
   prism('pink exhaust fin',pp([(-1,0),(-1.7,-.14),(-1.4,0),(-1.7,.14)]),.04,.05,'Pink')
 elif name=='Tibber':
  sphere('armored bear chest',(0,0,1.1),(.57,.65,.70),'Armor');box('chest breastplate',(.48,0,1.25),(.18,.86,.75),'Purple');box('core crystal',(.59,0,1.4),(.08,.35,.32),'Cyan')
  sphere('bear helmet',(0,0,2.04),(.62,.61,.61),'Purple');box('mask forehead',(.50,0,2.23),(.17,.70,.50),'Silver');box('black face visor',(.61,0,2.02),(.10,.79,.35),'Black')
  for sign in [-1,1]:
   sphere('round bear ear',(-.08,sign*.48,2.58),(.25,.21,.26),'Steel');sphere('ear insert',(.10,sign*.48,2.60),(.07,.13,.15),'Pink')
   box('eye slit',(.675,sign*.25,2.10),(.035,.22,.07),'Cyan',sign*.18)
   sphere('shoulder armor',(0,sign*.83,1.52),(.42,.39,.44),'Purple');box('arm plating',(.03,sign*.83,1.13),(.45,.38,.72),'Steel');sphere('gauntlet',(.13,sign*.86,.77),(.34,.30,.31),'Armor')
   for j in range(3):cone('gauntlet claw',(.49,sign*.86+(j-1)*.16,.72),.09,.45,'Silver',(1,0,-.12))
   box('heavy shin',(.02,sign*.35,.40),(.46,.44,.62),'Purple');box('bear foot',(.20,sign*.35,.12),(.78,.50,.25),'Armor');box('boot strip',(.54,sign*.35,.20),(.06,.32,.06),'Cyan')
   cone('helmet antenna',(.1,sign*.50,2.60),.13,.70,'Cyan',(.1,sign*.25,1))
  sphere('bear snout',(.69,0,1.88),(.19,.30,.18),'Steel');box('nose',(.88,0,1.94),(.08,.18,.09),'Black');box('jaw glow',(.77,0,1.76),(.10,.42,.055),'Pink')
  for sign in [-1,1]:cyl('back thruster',(-.58,sign*.35,1.35),.20,.5,'Steel','X');cyl('thruster flame',(-.86,sign*.35,1.35),.13,.08,'Pink','X')
 elif name=='Crossbow':
  prism('raked central hull',[(-1.2,-.3),(.40,-.43),(1.5,0),(.40,.43),(-1.2,.3),(-.85,0)],.10,.38,'Armor')
  prism('silver upper armor',[(-.9,-.20),(.3,-.27),(1.20,0),(.3,.27),(-.9,.20)],.34,.1,'Steel')
  box('energy magazine',(-.1,0,.44),(.55,.26,.13),'Cyan')
  for sign in [-1,1]:
   prism('swept crossbow arm',[(-.55,sign*.22),(-.95,sign*1.25),(-.42,sign*1.60),(-.50,sign*.81),(.32,sign*.35)],.13,.20,'Purple')
   prism('arm silver edge',[(-.95,sign*1.25),(-.42,sign*1.60),(-.51,sign*1.32),(-.72,sign*1.09)],.27,.045,'Silver')
   box('arm luminous notch',(-.60,sign*1.15,.27),(.24,.17,.045),'Cyan',sign*.35)
  for i in range(4):
   y=(i-1.5)*.26;cyl('bolt rail',(.68,y,.07),.055,.85,'Steel','X',12);cone('quad arrowhead',(1.28,y,.07),.13,.55,'Cyan',(1,0,0))
 elif name=='Cyclone':
  cyl('turbine center',(0,0,0),.25,.17,'Steel');torus('core light',(0,0,.10),.16,.028,'Cyan')
  for k in range(3):
   a=k*math.tau/3;pts=[(.2,-.1),(.50,-.49),(1.03,-.51),(.82,-.15),(.60,.14),(.72,-.31),(.42,-.22)]
   p=[(x*math.cos(a)-y*math.sin(a),x*math.sin(a)+y*math.cos(a)) for x,y in pts];prism('swept rotary blade',p,.01,.10,'Silver')
   p=[(x*.87*math.cos(a)-y*.87*math.sin(a),x*.87*math.sin(a)+y*.87*math.cos(a)) for x,y in pts];prism('pink energy vane',p,.08,.035,'Pink')
   crescent('cyan hook',(0,0),.73,a+.2,'Cyan')
 # Merge exported mesh, keeping a hidden editable source collection.
 parts=[o for o in bpy.context.scene.objects if o.type=='MESH'];sources=bpy.data.collections.new('EDITABLE_PARTS');bpy.context.scene.collection.children.link(sources)
 for o in parts:
  for c in list(o.users_collection):c.objects.unlink(o)
  sources.objects.link(o)
 sources.hide_render=True;sources.hide_viewport=True
 copies=[]
 for o in parts:
  q=o.copy();q.data=o.data.copy();bpy.context.scene.collection.objects.link(q);copies.append(q)
 bpy.ops.object.select_all(action='DESELECT')
 for o in copies:o.select_set(True)
 bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();model=bpy.context.object;model.name='SM_GW_'+name
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False) if hasattr(bpy.ops.mesh,'normals_make_consistent') else None
 bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.02);bpy.ops.object.mode_set(mode='OBJECT')
 bpy.ops.export_scene.fbx(filepath=str(ROOT/(model.name+'.fbx')),use_selection=True,object_types={'MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z')
 bpy.ops.export_scene.gltf(filepath=str(ROOT/(model.name+'.glb')),export_format='GLB',use_selection=True)
 # Dark studio preview with three colored area lights.
 scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=20;scene.cycles.use_denoising=True;scene.render.resolution_x=896;scene.render.resolution_y=896;scene.render.resolution_percentage=100
 scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.055,.07,.11,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
 target=Vector((0,0,1.3 if name=='Tibber' else 0))
 bpy.ops.object.camera_add(location=(6,-7,5.5) if name in ['Train','Tibber'] else (1,-5,8));cam=bpy.context.object;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=4.8 if name!='Tibber' else 4.3;scene.camera=cam
 for pos,power,color,size in [((4,-4,7),1300,(.65,.85,1),5),((-4,-1,3),1000,(.55,.25,1),4),((1,5,5),1600,(.25,.8,1),3)]:
  bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.color=color;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
 scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ROOT/'Preview'/(name+'.png'))
 bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/(name+'.blend')));bpy.ops.render.render(write_still=True)
 REPORT.append({'name':name,'vertices':len(model.data.vertices),'triangles':sum(len(p.vertices)-2 for p in model.data.polygons),'materials':[x.name for x in model.data.materials],'dimensions_m':list(model.dimensions),'pivot':'feet' if name=='Tibber' else 'center','forward':'+X','up':'+Z'})
 (ROOT/'ModelManifest.json').write_text(json.dumps(REPORT,indent=2))
 print('WEAPON_MODEL_READY',name,flush=True)
