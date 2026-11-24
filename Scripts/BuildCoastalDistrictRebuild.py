import bpy,math,pathlib,json,random,sys
from mathutils import Vector,Matrix
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/CoastalRebuild');ROOT.mkdir(exist_ok=True)
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


PALETTE.update({'Road':(.14,.18,.20,1),'Brick':(.46,.19,.17,1),'Paving':(.59,.55,.45,1),'Water':(.015,.28,.36,1),'Grass':(.19,.34,.06,1),'Earth':(.30,.21,.12,1)})
begin();random.seed(48)
def slab(n,points,z,h,mat):return poly(n,[(x,-y) for x,y in reversed(points)],z,h,mat)
def ub(n,x,y,z,w,d,h,mat='Road',yaw=0):return box(n,(x,-y,z),(w,d,h),mat,yaw=-math.radians(yaw))
PATH_INDEX=0
def path(n,a,b,width=.12,mat='Paving',z=.22):
 global PATH_INDEX
 PATH_INDEX+=1;z+=PATH_INDEX*.00001
 dx=b[0]-a[0];dy=b[1]-a[1];return ub(n,(a[0]+b[0])/2,(a[1]+b[1])/2,z,math.hypot(dx,dy),width,.025,mat,math.degrees(math.atan2(dy,dx)))
def loop(n,points,width=.16,mat='Paving',z=.23):
 for i,a in enumerate(points):path(n+str(i),a,points[(i+1)%len(points)],width,mat,z)
def uring(n,x,y,r,width=.14,mat='Paving',z=.23,steps=64):
 pts=[(x+r*math.cos(i*math.tau/steps),y+r*math.sin(i*math.tau/steps)) for i in range(steps)];loop(n,pts,width,mat,z)
boundary=[(-48,-44),(29,-44),(42,-29),(49,11),(49,40),(40,49),(-8,49),(-32,25),(-48,25)]
slab('thick coastal district foundation',boundary,-.55,1.1,'ConcreteShade');slab('continuous asphalt road deck',boundary,.03,.12,'Road');loop('foundation perimeter',boundary,.40,'Concrete',.12)
# Large park lawns with deliberate sidewalks and a broad eastern boulevard.
park=[(-45,-35),(-21,-35),(-20,-19),(-19,-4),(-20,13),(-31,22),(-45,22)]
slab('park raised concrete curb',park,.10,.20,'Paving');slab('park lawn',[(x*.994-.08,y*.994) for x,y in park],.23,.05,'Grass');loop('park curb bright edge',park,.18,'Trim',.27)
for i,(a,b) in enumerate([((-33,-31),(-33,17)),((-44,-16),(-21,-16)),((-43,3),(-21,3)),((-33,17),(-22,11)),((-32,-7),(-23,-4)),((-43,16),(-33,8))]):path('park paved walk'+str(i),a,b,1.0,'Paving',.29)
for i in range(7):path('park stepping stone'+str(i),(-38+i*.7,13+i*.55),(-37.65+i*.7,13.25+i*.55),.50,'Concrete',.30)
north=[(-12,-32),(26,-32),(33,-24),(18,-10),(7,-7),(-5,-9),(-12,-17)]
slab('north red plaza raised rim',north,.10,.16,'Paving');slab('north red plaza brick surface',north,.20,.05,'Brick');loop('north plaza curb',north,.18,'Trim',.25)
for x,y,r in [(9,-22,4.5),(17,-24,1.15),(1,-27,.9)]:
 cyl('north circular lawn',(x,-y,.24),r,.08,'Grass',cat='Structure',verts=48);uring('circular pale brick ring',x,y,r+.25,.28);uring('outer plaza circular inlay',x,y,r+1,.12)
def inside(p,polygon):
 x,y=p;result=False
 for i,a in enumerate(polygon):
  b=polygon[(i+1)%len(polygon)]
  if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:result=not result
 return result
for j in range(20):
 t=math.pi*.05+j*math.pi*.90/19;a=(-1+16*math.cos(t),-7-16*math.sin(t));b=(-1+22*math.cos(t),-7-22*math.sin(t))
 if inside(a,north) and inside(b,north):path('radial plaza paving',a,b,.09,'Paving',.245)
for y in [-30,-27,-24,-21,-18,-15,-12]:path('north horizontal brick joint',(-10,y),(20,y),.035,'ConcreteShade',.235)
# Angular planted panels echo the fan shaped landscaped plaza.
for points in [[(-9,-30),(-3,-30),(-5,-26),(-9,-26)],[(-9,-24),(-6,-24),(-6,-21),(-9,-19)],[(0,-30),(5,-30),(4,-27)],[(21,-29),(26,-28),(23,-25)]]:
 slab('north triangular lawn',points,.27,.06,'Grass');loop('lawn edging',points,.12,'Paving',.31)
south=[(-18,15),(-3,10),(10,24),(6,32),(-21,32),(-27,26)]
slab('south triangular plaza raised bed',south,.10,.18,'Paving');slab('south plaza brick',south,.23,.05,'Brick');loop('south plaza pale curb',south,.22,'Trim',.29)
for points in [[(-21,26),(-18,18),(-12,16),(-15,26)],[(-12,28),(-9,16),(-6,16),(-6,28)],[(-3,15),(3,22),(1,27),(-3,27)]]:
 slab('south planted fan bed',points,.29,.06,'Grass');loop('south planting edging',points,.17,'Paving',.34)
for a,b in [((-23,29),(5,29)),((-19,15),(-12,30)),((-9,12),(-4,30)),((-4,12),(5,29))]:path('south radial pathway',a,b,.42,'Paving',.33)
# Painted road perimeter, junction approach lanes, zebra crossings and industrial service aprons.
for a,b in [((-17,-36),(-17,-7)),((-18,-3),(-27,21)),((-15,6),(-33,24)),((7,7),(35,-18)),((10,13),(38,39)),((-5,40),(35,40))]:
 path('road narrow edge',a,b,.12,'Paving',.17)
 dx=b[0]-a[0];dy=b[1]-a[1];length=math.hypot(dx,dy)
 for j in range(int(length/3)):
  t=(j+.2)*3/length;u=min(1,t+1.15/length);path('road broken white center',(a[0]+dx*t,a[1]+dy*t),(a[0]+dx*u,a[1]+dy*u),.10,'Trim',.18)
for x,y,yaw in [(-17,-9,0),(9,10,-45),(24,-17,45)]:
 for j in range(7):ub('pedestrian crossing',x+(j-3)*.45,y,.185,.22,2.3,.025,'Trim',yaw)
for x,y,w,d in [(15,17,10.8,7),(31,28,11,10),(21,4,10,8),(33,0,9,8)]:
 pts=[(x-w/2,y-d/2+1),(x-w/2+1,y-d/2),(x+w/2-1,y-d/2),(x+w/2,y-d/2+1),(x+w/2,y+d/2-1),(x+w/2-1,y+d/2),(x-w/2+1,y+d/2),(x-w/2,y+d/2-1)]
 slab('industrial island service apron',pts,.15,.10,'Earth');loop('industrial apron painted border',pts,.10,'Yellow',.23)
for j in range(14):path('hazard loading stripe',(9+j*.45,9),(10+j*.45,7.7),.14,'Yellow',.18)
# Central road circle leaves the detailed healing structure unobstructed.
uring('central junction road rim',0,0,8.0,.20,'Paving',.23);uring('central junction inner ring',0,0,7.4,.08,'Cyan',.24)
for j in range(24):
 a=j*math.tau/24;path('roundabout broken line',(9.5*math.cos(a),9.5*math.sin(a)),(9.5*math.cos(a+.10),9.5*math.sin(a+.10)),.09,'Trim',.18)
# Join the authored ground with its origin at the district center.
bpy.ops.object.select_all(action='DESELECT')
for o in PARTS:o.select_set(True)
bpy.context.view_layer.objects.active=PARTS[0];bpy.ops.object.join();ground=bpy.context.object;ground.name='SM_CoastalRebuildGround';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.2,island_margin=.003);bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_CoastalRebuildGround.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
# Source composition shares a single placement manifest with Unreal.
layout=json.loads((ROOT/'Layout.json').read_text(encoding='utf-8'));props=[]
def source_obj(file,name):
 with bpy.data.libraries.load(str(file),link=False) as (a,b):b.objects=[name] if name in a.objects else []
 if not b.objects:raise RuntimeError((file,name))
 obj=b.objects[0];bpy.context.collection.objects.link(obj);return obj
for i,row in enumerate(layout['placements']):
 folder='ReferenceBuildings' if int(row['id'][:2])<=14 else 'ReferenceIndustrialModules';obj=source_obj(ROOT.parent/folder/(row['id']+'.blend'),'SM_CRB_'+row['id']);obj.name='Layout_'+str(i)+'_'+row['id'];x,y,z=row['position_m'];obj.location=(x,-y,z);obj.rotation_euler.z=-math.radians(row['yaw_deg']);obj.scale=(row['scale'],)*3
for file,name,x,y,scale in [('HealingStationDeepBase','SM_CR_HealingStationDeepBase',0,0,.90),('CoastalFountains','SM_CR_ParkFountain',-32,-25,.70)]:
 obj=source_obj(ROOT.parent/'CoastalResearch'/(file+'.blend'),name);obj.location=(x,-y,.18);obj.scale=(scale,)*3;props.append({'mesh':name,'position_m':[x,y,.18],'scale':scale})
# Trees, shrubs, benches, walkway lighting and bollards are separate reusable props in the map.
for i,(x,y) in enumerate([(-40,-29),(-38,-20),(-27,-30),(-24,-20),(-40,-3),(-39,13),(-27,17),(-25,5),(-35,9),(-35,-9),(-22,-8),(-29,-33)]):
 obj=source_obj(ROOT.parent/'CoastalResearch/ParkFoliage.blend','SM_CR_ParkBroadleafCrown');obj.location=(x,-y,3.0);obj.scale=(3.5,)*3;props.append({'mesh':'SM_CR_ParkBroadleafCrown','position_m':[x,y,3.0],'scale':3.5})
 # A rounded trunk is excluded from the exported ground and becomes a separate prop recipe.
 bpy.ops.mesh.primitive_cylinder_add(vertices=10,radius=.17,depth=2.7,location=(x,-y,1.4));trunk=bpy.context.object;trunk.data.materials.append(MAT['Earth']);props.append({'primitive':'trunk','position_m':[x,y,1.4],'scale_m':[.34,.34,2.7]})
for x,y,yaw in [(-35,-16,90),(-31,3,0),(-25,12,0),(-40,19,0),(-10,-18,0),(20,-15,-30),(-16,25,-30),(2,30,0)]:
 obj=source_obj(ROOT.parent/'CoastalResearch/ParkFurniture.blend','SM_CR_DistrictTimberBench');obj.location=(x,-y,.30);obj.rotation_euler.z=-math.radians(yaw);props.append({'mesh':'SM_CR_DistrictTimberBench','position_m':[x,y,.30],'yaw_deg':yaw,'scale':1})
layout['props']=props;(ROOT/'Layout.json').write_text(json.dumps(layout,indent=2),encoding='utf-8')
# Water and soft studio illumination provide a readable whole-map preview.
water=slab('coastal ocean',[(-100,24),(-48,24),(-32,24),(-7,48),(39,48),(49,39),(100,100),(-100,100)],-1.1,.04,'Water');bpy.ops.object.select_all(action='DESELECT');water.select_set(True);bpy.context.view_layer.objects.active=water;bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_CoastalRebuildWater.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=2200;scene.render.resolution_y=1800;scene.world=bpy.data.worlds.new('Coastal daylight');scene.world.color=(.18,.18,.22)
target=Vector((0,0,0));bpy.ops.object.camera_add(location=(48,-86,100));cam=bpy.context.object;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=145;scene.camera=cam
for pos,power,size in [((25,-40,90),130000,70),((-70,0,60),95000,65),((0,65,80),140000,60)]:
 bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.rotation_euler=(target-l.location).to_track_quat('-Z','Y').to_euler()
scene.view_settings.view_transform='AgX';scene.view_settings.exposure=-.45;scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ROOT/'CoastalRebuildOverview.png');bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'CoastalDistrictRebuilt.blend'));bpy.ops.render.render(write_still=True)
print('COASTAL_REBUILD_SOURCE_READY',len(layout['placements']),len(props),flush=True)
