import bpy,math,pathlib,json,random,sys
from mathutils import Vector,Matrix
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/OutskirtsRebuild');ROOT.mkdir(exist_ok=True)
SPECS=[
('01_TerraceLow','공원 테라스 저층동','Residential',(6.8,5.2,5.4)),
('02_StepResidence','계단형 녹화 주거동','Residential',(7.8,6.0,7.2)),
('03_RoundCornerResidence','곡면 코너 주거동','Residential',(5.6,5.0,6.2)),
('04_CourtyardAnnex','중정형 공원 별관','Residential',(5.4,4.8,4.2)),
('05_PrismMediaTower','스트라이프 미디어 타워','Commercial',(5.6,5.2,18.0)),
('06_PlazaBillboardTower','중앙 대형 광고 빌딩','Commercial',(9.0,6.6,15.0)),
('07_CornerHoloTower','코너 홀로 광고 빌딩','Commercial',(6.8,5.4,12.5)),
('08_ArcadeShopRow','모듈형 상가동','Commercial',(8.0,4.6,5.4)),
('09_UtilityOffice','설비형 중층 오피스','Commercial',(5.0,4.8,9.5)),
('10_BoilerWorkshop','보일러 공장동','Industrial',(7.6,5.4,6.2)),
('11_PumpHouse','펌프 설비동','Industrial',(6.2,5.2,4.8)),
('12_VentilationPlant','대형 환기 공장동','Industrial',(8.5,6.6,5.8)),
('13_ServiceStack','산업 서비스 타워','Utility',(4.0,4.4,9.8)),
('14_TwinFlueTower','쌍연통 설비 타워','Utility',(4.5,4.5,11.2))]
PALETTE={'Concrete':(.67,.71,.70,1),'ConcreteShade':(.39,.46,.46,1),'Trim':(.88,.90,.83,1),'Metal':(.23,.31,.34,1),'Dark':(.045,.064,.079,1),'Roof':(.095,.13,.145,1),'Glass':(.055,.25,.32,1),'Orange':(.88,.32,.08,1),'Grass':(.25,.42,.075,1),'Leaf':(.09,.27,.085,1),'Soil':(.12,.075,.035,1),'AdBlue':(.018,.30,.60,1),'AdTeal':(.015,.40,.38,1),'Cyan':(.045,.72,.86,1),'Pink':(.83,.07,.44,1),'Yellow':(.93,.63,.14,1)}
REPORT=[]
MANIFEST=ROOT/'BuildingManifest.json'
if MANIFEST.exists():REPORT=json.loads(MANIFEST.read_text(encoding='utf-8'))['buildings']
FILTER=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []

def begin():
 bpy.ops.wm.read_factory_settings(use_empty=True)
 global MAT,PARTS
 MAT={};PARTS=[]
 for n,c in PALETTE.items():
  m=bpy.data.materials.new('CRB_'+n);m.diffuse_color=c;m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=c;p.inputs['Roughness'].default_value=.65 if n not in ['Glass','Metal'] else .28;p.inputs['Metallic'].default_value=.5 if n=='Metal' else .22 if n=='Glass' else 0
  if n in ['Cyan','Pink','AdBlue','AdTeal','Yellow']:
   p.inputs['Emission Color'].default_value=c;p.inputs['Emission Strength'].default_value=1.5 if n in ['Cyan','Pink'] else .22
  MAT[n]=m

def finish(o,n,mat,cat='Structure',bevel=.025):
 o.name=n;o['part_group']=cat;o.data.materials.append(MAT[mat]);PARTS.append(o)
 if bevel>0:
  b=o.modifiers.new('Manufactured softened edges','BEVEL');b.width=bevel;b.segments=2 if bevel>.035 else 1;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=b.name)
 return o

def box(n,p,d,mat='Concrete',cat='Structure',yaw=0):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=d;o.rotation_euler.z=yaw;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,n,mat,cat,min(.045,min(d)*.12) if min(d)>.055 else 0)

def cyl(n,p,r,h,mat='Metal',axis='Z',cat='Rooftop',verts=20):
 bpy.ops.mesh.primitive_cylinder_add(vertices=verts,radius=r,depth=h,location=p);o=bpy.context.object
 if axis=='X':o.rotation_euler.y=math.pi/2
 elif axis=='Y':o.rotation_euler.x=math.pi/2
 return finish(o,n,mat,cat,.018)

def ring(n,p,r,t=.035,mat='Metal',axis='Z',cat='Rooftop'):
 bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=t,major_segments=24,minor_segments=6,location=p);o=bpy.context.object
 if axis=='X':o.rotation_euler.y=math.pi/2
 elif axis=='Y':o.rotation_euler.x=math.pi/2
 return finish(o,n,mat,cat,0)

def poly(n,pts,z,h,mat='Concrete',cat='Structure'):
 k=len(pts);v=[(x,y,z+q*h/2) for q in [-1,1] for x,y in pts];f=[tuple(reversed(range(k))),tuple(range(k,2*k))]+[(i,(i+1)%k,(i+1)%k+k,i+k) for i in range(k)];m=bpy.data.meshes.new(n);m.from_pydata(v,[],f);m.update();o=bpy.data.objects.new(n,m);bpy.context.collection.objects.link(o);return finish(o,n,mat,cat,.02 if h>.08 else 0)

def tube(n,points,r=.045,mat='Metal',cat='Rooftop',smooth=False):
 c=bpy.data.curves.new(n,'CURVE');c.dimensions='3D';c.resolution_u=4;c.bevel_depth=r;c.bevel_resolution=2;c.use_fill_caps=True
 s=c.splines.new('BEZIER' if smooth else 'POLY');(s.bezier_points if smooth else s.points).add(len(points)-1)
 if smooth:
  for p,v in zip(s.bezier_points,points):p.co=v;p.handle_left_type=p.handle_right_type='AUTO'
 else:
  for p,v in zip(s.points,points):p.co=(*v,1)
 o=bpy.data.objects.new(n,c);bpy.context.collection.objects.link(o);bpy.ops.object.select_all(action='DESELECT');bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.convert(target='MESH');return finish(bpy.context.object,n,mat,cat,0)

def text(n,body,p,size=.28,mat='Trim',side=False):
 bpy.ops.object.text_add(location=p);o=bpy.context.object;o.name=n;o.data.body=body;o.data.align_x='CENTER';o.data.align_y='CENTER';o.data.size=size;o.data.extrude=.003;o.data.resolution_u=2;o.rotation_euler=(math.pi/2,0,math.pi/2 if side else 0);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');return finish(bpy.context.object,n,mat,'Signs',0)

def front_panel(n,x,y,z,w,h,mat='Glass'):
 return box(n,(x,y,z),(w,.045,h),mat,'Facade')

def facade(w,d,base,floors,fh,orange=False,offset=(0,0)):
 start=len(PARTS)
 for j in range(floors):
  z=base+(j+.5)*fh
  box('floor fascia',(0,-d/2-.035,base+j*fh+.14),(w+.12,.11,.16),'Orange' if orange else 'Trim','Facade')
  for x in range(max(2,int(w/1.1))):
   cx=-w/2+.55+x*(w-1.1)/max(1,max(2,int(w/1.1))-1)
   front_panel('recessed glazing',cx,-d/2-.032,z,.79,fh*.62)
   box('window divider',(cx,-d/2-.071,z),(.035,.025,fh*.64),'Trim','Facade')
  # Return glazing on the right wall.
  for i in range(max(2,int(d/1.1))):
   cy=-d/2+.55+i*(d-1.1)/max(1,max(2,int(d/1.1))-1)
   box('side window',(w/2+.028,cy,z),(.045,.77,fh*.60),'Glass','Facade')
  box('side floor band',(w/2+.04,0,base+j*fh+.14),(.12,d+.10,.14),'Trim','Facade')
 for x in [-w/2+.1,w/2-.1]:box('front corner pier',(x,-d/2-.08,base+floors*fh/2),(.20,.19,floors*fh),'Trim','Facade')

 for o in PARTS[start:]:o.location.x+=offset[0];o.location.y+=offset[1]

def roofrim(w,d,z,offset=(0,0)):
 start=len(PARTS)
 for sy in [-1,1]:box('roof parapet',(0,sy*d/2,z+.16),(w+.12,.15,.32),'Trim','Rooftop')
 for sx in [-1,1]:box('roof parapet return',(sx*w/2,0,z+.16),(.15,d,.32),'Trim','Rooftop')

 for o in PARTS[start:]:o.location.x+=offset[0];o.location.y+=offset[1]

def rail(n,a,b,z,mat='Trim'):
 for h in [.12,.62]:tube(n,[(*a,z+h),(*b,z+h)],.026,mat,'Facade')
 distance=math.dist(a,b)
 for j in range(max(2,math.ceil(distance/.65))+1):
  t=j/max(2,math.ceil(distance/.65));x=a[0]*(1-t)+b[0]*t;y=a[1]*(1-t)+b[1]*t;box('railing stanchion',(x,y,z+.35),(.045,.045,.70),mat,'Facade')

def planter(x,y,z,w=1.1,d=.45):
 box('planter housing',(x,y,z+.16),(w,d,.32),'ConcreteShade','Vegetation');box('planter soil',(x,y,z+.33),(w-.10,d-.08,.035),'Soil','Vegetation')
 for j in range(3):
  bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=(x+(j-1)*w*.25,y,z+.48));o=bpy.context.object;o.scale=(w*.23,d*.48,.27);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);finish(o,'clipped balcony shrub','Grass' if j%2 else 'Leaf','Vegetation',0)

def garden(w,d,z,offset=(0,0)):
 x,y=offset;box('green roof inset',(x,y,z+.025),(w-.35,d-.35,.05),'Grass','Vegetation')
 for sx in [-1,1]:planter(x+sx*(w/2-.45),y+d/2-.45,z,.65,.5)

def fan(x,y,z,r=.38):
 box('fan equipment curb',(x,y,z+.17),(r*2.4,r*2.4,.34),'Metal','Rooftop');cyl('fan dark recess',(x,y,z+.355),r,.035,'Dark');ring('fan guard',(x,y,z+.39),r,.028,'Trim');cyl('fan motor',(x,y,z+.405),r*.24,.07,'Metal')
 for k in range(6):
  a=k*math.tau/6;box('fan blade',(x+math.cos(a)*r*.52,y+math.sin(a)*r*.52,z+.39),(r*.80,r*.16,.025),'ConcreteShade','Rooftop',a+.40)
 for sy in [-1,1]:tube('fan grate',[(x-r,y+sy*r*.40,z+.43),(x+r,y+sy*r*.40,z+.43)],.014,'Trim')

def hvac(x,y,z,w=1.25,d=.85):
 box('HVAC case',(x,y,z+.36),(w,d,.72),'Metal','Rooftop')
 for j in range(6):box('heat exchanger slat',(x-w/2+.10+j*(w-.2)/5,y-d/2-.025,z+.36),(.045,.035,.48),'Dark','Rooftop')
 box('HVAC service cover',(x,y,z+.74),(w+.06,d+.06,.05),'Trim','Rooftop');tube('refrigerant pipe',[(x+w/2,y,z+.45),(x+w/2+.15,y,z+.45),(x+w/2+.15,y,z+.03)],.035,'Cyan')

def door(x,y,z,w=.92,h=1.55):
 front_panel('service door frame',x,y,z+h/2,w+.15,h+.12,'Metal');front_panel('service door',x,y-.03,z+h/2,w,h,'Dark');box('door handle',(x+w*.28,y-.065,z+h*.48),(.05,.035,.18),'Trim','Facade');box('door step',(x,y-.15,z+.055),(w+.3,.42,.11),'ConcreteShade')

def solar(x,y,z,w=1.5,d=.9):
 box('solar frame',(x,y,z+.08),(w+.08,d+.08,.13),'Trim','Rooftop');box('photovoltaic panel',(x,y,z+.16),(w,d,.035),'Glass','Rooftop')
 for j in range(1,5):box('panel silver busbar',(x-w/2+j*w/5,y,z+.185),(.015,d,.01),'Trim','Rooftop')
 for j in [-1,1]:box('panel transverse busbar',(x,y+j*d/6,z+.185),(w,.012,.01),'Trim','Rooftop')

def bunny(x,y,z,size):
 def path(name,p,mat='Trim',r=.035):tube(name,[(x+u*size,y,z+v*size) for u,v in p],r*size,mat,'Signs',True)
 path('rabbit left ear',[(-.30,.12),(-.43,.55),(-.30,.8),(-.14,.60),(-.14,.17)])
 path('rabbit right ear',[(.10,.15),(.13,.64),(.35,.78),(.43,.55),(.30,.10)])
 path('rabbit face',[(-.32,.12),(-.52,-.08),(-.30,-.34),(.20,-.36),(.49,-.10),(.30,.12),(-.32,.12)])
 for sx in [-1,1]:path('mascot eye',[(sx*.24,-.04),(sx*.11,-.04)],'Cyan',.025)
 path('mascot smile',[(-.13,-.21),(0,-.26),(.13,-.20)],'Trim',.018)

def bolts(x,y,z,r=.36):
 for j in range(6):
  a=j*math.tau/6;cyl('flange bolt',(x+r*math.cos(a),y+r*math.sin(a),z),.032,.035,'Trim',verts=6)

def flue(x,y,z,h=1.6,r=.40):
 cyl('chimney shell',(x,y,z+h/2),r,h,'Orange');cyl('chimney inner bore',(x,y,z+h+.005),r*.79,.02,'Dark');ring('chimney silver lip',(x,y,z+h),r,.055,'Trim');ring('chimney base flange',(x,y,z+.10),r*1.13,.045,'Metal');bolts(x,y,z+.13,r*1.02)

def plinth(w,d):
 pts=[(-w/2+.30,-d/2),(w/2-.30,-d/2),(w/2,-d/2+.3),(w/2,d/2-.3),(w/2-.3,d/2),(-w/2+.3,d/2),(-w/2,d/2-.3),(-w/2,-d/2+.3)]
 poly('chamfered building foundation',pts,.10,.20,'ConcreteShade')


PALETTE.update({'Sand':(.48,.33,.18,1),'SandLight':(.58,.43,.26,1),'SandShade':(.37,.25,.13,1),'Asphalt':(.20,.20,.17,1),'Rock':(.26,.27,.23,1),'Water':(.018,.32,.39,1),'Dust':(.45,.38,.25,1)})
begin();random.seed(1402);PATH_INDEX=0
def slab(n,pts,z,h,mat):return poly(n,[(x,-y) for x,y in reversed(pts)],z,h,mat)
def line(n,a,b,width=.12,mat='Trim',z=.13):
 global PATH_INDEX
 PATH_INDEX+=1;dx=b[0]-a[0];dy=b[1]-a[1];return box(n,((a[0]+b[0])/2,-(a[1]+b[1])/2,z+PATH_INDEX*.000005),(math.hypot(dx,dy),width,.015),mat,yaw=-math.atan2(dy,dx))
def loop(n,pts,width=.12,mat='Trim',z=.13):
 for i,a in enumerate(pts):line(n+str(i),a,pts[(i+1)%len(pts)],width,mat,z)
boundary=[(-49,23),(-47,0),(-41,-21),(-31,-37),(-16,-49),(30,-49),(47,-40),(49,-14),(48,43),(34,49),(-40,49),(-49,36)]
slab('coastal sediment foundation',boundary,-.40,.80,'SandShade');slab('weathered sandy ground',boundary,.025,.05,'Sand')
# Dust strata and exposed sediment vary the ground without covering combat routes.
for i in range(100):
 x=random.uniform(-40,44);y=random.uniform(-43,44);r=random.uniform(.7,3.3);pts=[(x+r*math.cos(j*math.tau/7)*random.uniform(.8,1.1),y+r*.5*math.sin(j*math.tau/7)) for j in range(7)]
 slab('sand erosion patch',pts,.055+i*.00003,.006,'SandLight' if i%3 else 'SandShade')
road=[(-37,40),(-37,22),(-33,7),(-25,-9),(-14,-22),(2,-31),(20,-34),(38,-32)]
left=[];right=[]
for i,p in enumerate(road):
 a=road[max(0,i-1)];b=road[min(len(road)-1,i+1)];dx=b[0]-a[0];dy=b[1]-a[1];L=max(.01,math.hypot(dx,dy));nx=-dy/L;ny=dx/L;left.append((p[0]+nx*4.4,p[1]+ny*4.4));right.append((p[0]-nx*4.4,p[1]-ny*4.4))
slab('curving abandoned service road',left+list(reversed(right)),.075,.04,'Asphalt')
for side in [left,right]:
 for i in range(len(side)-1):line('faded road edge',side[i],side[i+1],.10,'Dust',.106)
for a,b in zip(road,road[1:]):
 length=math.dist(a,b);dx=b[0]-a[0];dy=b[1]-a[1]
 for j in range(int(length/3)):
  t=(j+.15)*3/length;u=min(1,t+1.0/length);line('road center worn dash',(a[0]+dx*t,a[1]+dy*t),(a[0]+dx*u,a[1]+dy*u),.085,'Paving' if 'Paving' in MAT else 'ConcreteShade',.11)
# Southern base pad with clipped corners, landing panel and branching maintenance paths.
basepad=[(-27,17),(-3,17),(3,23),(3,31),(-3,36),(-27,36)]
slab('abandoned southern military apron',basepad,.08,.06,'Dust');loop('base apron perimeter',basepad,.18,'ConcreteShade',.12)
landing=[(-21,21),(-6,21),(-3,24),(-3,29),(-6,32),(-21,32)]
slab('landing bay painted inset',landing,.115,.012,'SandShade');loop('landing bay octagonal line',landing,.18,'Concrete',.133)
line('base center parking stripe',(-25,24),(-4,24),.09,'ConcreteShade',.14);line('base service lane',(-22,35),(-7,44),1.0,'Dust',.065)
for x in [-24,-18,-12,-6]:line('base bay markings',(x,17.3),(x,20),.12,'ConcreteShade',.14)
# Visible erosion fractures and wheel tracks around the sandy center.
for i in range(40):
 x=random.uniform(-22,43);y=random.uniform(-37,39)
 if math.dist((x,y),(10.5,-7))<12:continue
 a=(x,y);b=(x+random.uniform(.4,1.6),y+random.uniform(.6,2));line('fine sand fissure',a,b,.025,'SandShade',.068)
for x,y in [(-8,10),(10,28),(25,-30)]:
 for j in range(14):
  for side in [-1,1]:line('paired vehicle tire print',(x+side*.55,y+j*.21),(x+side*.55+.17,y+j*.21+.05),.09,'SandShade',.075)
# Merge only the terrain/roads; all buildings and cliffs remain separately selectable in Unreal.
bpy.ops.object.select_all(action='DESELECT')
for o in PARTS:o.select_set(True)
bpy.context.view_layer.objects.active=PARTS[0];bpy.ops.object.join();ground=bpy.context.object;ground.name='SM_OutskirtsGround';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.2,island_margin=.003);bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_OutskirtsGround.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
water=slab('northwest ocean',[(-110,-110),(110,-110),(30,-49),(-16,-49),(-31,-37),(-41,-21),(-49,23),(-110,50)],-.75,.03,'Water');bpy.ops.object.select_all(action='DESELECT');water.select_set(True);bpy.context.view_layer.objects.active=water
bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_OutskirtsWater.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
layout=json.loads((ROOT/'Layout.json').read_text(encoding='utf-8'));cache={}
for i,row in enumerate(layout['placements']):
 name='SM_CRB_'+row['id']
 if name not in cache:
  with bpy.data.libraries.load(str(ROOT/'Models'/(row['id']+'.blend')),link=False) as (a,b):b.objects=[name]
  cache[name]=b.objects[0]
 obj=cache[name].copy();obj.data=cache[name].data;bpy.context.collection.objects.link(obj);x,y,z=row['position_m'];obj.location=(x,-y,z);obj.rotation_euler.z=-math.radians(row['yaw_deg']);obj.scale=(row['scale'],)*3;obj.name='Outskirts '+str(i)+' '+row['id']
# A few dry brush clumps add scale around the shore; baked into a separate decorative mesh.
brush=[]
for j in range(65):
 x=random.uniform(-42,44);y=random.uniform(-42,43)
 if math.dist((x,y),(10.5,-7))<17.5 or any(math.dist((x,y),(r['position_m'][0],r['position_m'][1]))<3 for r in layout['placements']):continue
 for i in range(5):
  a=i*math.tau/5;tube('dry coastal grass',[(x,-y,.06),(x+math.cos(a)*.17,-y+math.sin(a)*.17,.32+random.random()*.18)],.015,'Dust','Vegetation')
 brush.extend(PARTS[-5:])
if brush:
 bpy.ops.object.select_all(action='DESELECT')
 for o in brush:o.select_set(True)
 bpy.context.view_layer.objects.active=brush[0];bpy.ops.object.join();deco=bpy.context.object;deco.name='SM_OutskirtsDryBrush';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_OutskirtsDryBrush.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=2100;scene.render.resolution_y=1800;scene.world=bpy.data.worlds.new('Outskirts daylight');scene.world.color=(.025,.025,.04)
target=Vector((0,0,0));bpy.ops.object.camera_add(location=(18,-82,112));cam=bpy.context.object;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=133;scene.camera=cam
for pos,power,size in [((25,-40,90),110000,65),((-65,-10,60),65000,60),((0,70,80),100000,60)]:
 bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.rotation_euler=(target-l.location).to_track_quat('-Z','Y').to_euler()
scene.view_settings.view_transform='AgX';scene.view_settings.exposure=-.4;scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ROOT/'OutskirtsOverview.png');bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'OutskirtsDistrict.blend'));bpy.ops.render.render(write_still=True)
print('OUTSKIRTS_SOURCE_COMPLETE',len(layout['placements']),flush=True)
