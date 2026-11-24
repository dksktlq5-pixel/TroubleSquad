import bpy,math,pathlib,json,random,sys
import numpy as np
from mathutils import Vector,Matrix
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/ReferenceIndustrialModules');ROOT.mkdir(exist_ok=True)
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


SPECS=[
 ('15_SeawallStraight','방파제 직선 모듈','Seawall',(6.0,1.15,1.65)),
 ('16_SeawallCorner45','방파제 45도 코너','Seawall',(6.0,3.3,1.65)),
 ('17_SeawallCorner90','방파제 90도 코너','Seawall',(5.5,5.5,1.65)),
 ('18_SeawallEndCap','방파제 끝단','Seawall',(1.7,1.4,1.9)),
 ('19_SeawallButtress','방파제 외부 지지대','Seawall',(2.5,3.2,2.1)),
 ('20_TwinGasPlant','쌍탱크 가스 처리 설비','Industrial',(9.0,5.4,4.6)),
 ('21_GasStorageVessel','독립 가스 저장 탱크','Industrial',(2.6,2.6,3.8)),
 ('22_CompressorStation','대형 압축 환기 설비','Industrial',(8.5,7.0,4.8)),
 ('23_PipeRack','다중 배관 연결 랙','Industrial',(5.5,1.8,2.6)),
 ('24_HazardBarricade','황흑 경계 바리케이드','Utility',(4.0,.8,1.05)),
 ('25_ControlKiosk','산업 제어 함체','Utility',(2.2,1.5,2.5))]
REPORT=[]
if MANIFEST.exists():REPORT=json.loads(MANIFEST.read_text(encoding='utf-8'))['buildings']

def wheel(x,y,z,r=.18):
 ring('red orange valve handwheel',(x,y,z),r,.027,'Orange',axis='Y');cyl('valve spindle',(x,y+.10,z),.045,.30,'Metal',axis='Y')
 for a in [0,math.pi/2]:
  tube('handwheel spoke',[(x-r*math.cos(a),y,z-r*math.sin(a)),(x+r*math.cos(a),y,z+r*math.sin(a))],.022,'Orange')

def tank(x,y,z,r=1.0,h=2.7):
 cyl('pressure vessel shell',(x,y,z+h/2),r,h,'Orange',verts=32)
 for zz in [.13,h*.25,h*.76,h-.10]:ring('pressure vessel reinforcement',(x,y,z+zz),r+.035,.075,'Metal')
 cyl('pressure vessel top cap',(x,y,z+h),r*.94,.15,'Metal',verts=32);cyl('glowing status inset',(x,y,z+h+.09),r*.60,.035,'AdTeal',verts=32);ring('top white service hatch',(x,y,z+h+.12),r*.63,.055,'Trim')
 for k in range(8):
  a=k*math.tau/8;cyl('cap flange hex bolt',(x+math.cos(a)*r*.82,y+math.sin(a)*r*.82,z+h+.1),.035,.045,'Trim',verts=6)
 for sx in [-1,1]:box('vessel mount foot',(x+sx*r*.57,y,z+.15),(.24,r*1.1,.30),'Metal','Structure')
 box('tank inspection label',(x,y-r-.035,z+h*.54),(.46,.035,.40),'Trim','Signs');box('tank warning inset',(x,y-r-.057,z+h*.55),(.30,.018,.23),'Yellow','Signs')
 tube('tank feed elbow',[(x,y-r,z+.5),(x,y-r-.42,z+.5),(x,y-r-.42,z+.25)],.09,'Trim',smooth=True);wheel(x,y-r-.48,z+.52)
 for j in range(7):box('maintenance ladder step',(x+r*.75,y-r*.75,z+.25+j*h/8),(.40,.06,.045),'Metal','Facade')
 for xx in [x+r*.75-.21,x+r*.75+.21]:tube('ladder side rail',[(xx,y-r*.75,z+.12),(xx,y-r*.75,z+h-.1)],.025,'Metal')

def wall_segment(a,b):
 dx=b[0]-a[0];dy=b[1]-a[1];L=math.hypot(dx,dy);angle=math.atan2(dy,dx);cx=(a[0]+b[0])/2;cy=(a[1]+b[1])/2
 profile=[(-.43,0),(.43,0),(.35,.30),(.23,1.45),(-.23,1.45),(-.35,.30)]
 verts=[]
 for sx in [-1,1]:
  for yy,zz in profile:verts.append((cx+math.cos(angle)*sx*L/2-math.sin(angle)*yy,cy+math.sin(angle)*sx*L/2+math.cos(angle)*yy,zz))
 n=len(profile);faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
 mesh=bpy.data.meshes.new('sloped seawall');mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new('sloped seawall',mesh);bpy.context.collection.objects.link(o);finish(o,'sloped concrete seawall','ConcreteShade','Structure',.035)
 # Transform local detail positions into each segment's frame.
 start=len(PARTS)
 box('continuous dark recessed wall panel',(0,-.333,.79),(L-.15,.045,.72),'Dark','Facade')
 box('armored panel insert',(0,-.367,.79),(L-.27,.040,.59),'Metal','Facade')
 box('top coping rail',(0,0,1.45),(L+.06,.61,.16),'Metal','Structure')
 for x in [-L/2,L/2]:
  box('wall segment clamp',(x,-.02,.79),(.24,.85,1.58),'Concrete','Structure');box('clamp steel crown',(x,-.02,1.59),(.32,.83,.12),'Metal','Structure')
 for x in [-L*.3,0,L*.3]:
  box('gold reflector',(x,-.401,.80),(.13,.025,.21),'Yellow','Signs',-.5)
  for zz in [.56,1.03]:cyl('wall panel rivet',(x,-.397,zz),.025,.025,'Trim',axis='Y',cat='Facade',verts=8)
 box('seawall foot stripe',(0,-.45,.10),(L-.2,.07,.075),'Yellow','Signs')
 for o in PARTS[start:]:
  px,py=o.location.x,o.location.y;o.location.x=cx+math.cos(angle)*px-math.sin(angle)*py;o.location.y=cy+math.sin(angle)*px+math.cos(angle)*py;o.rotation_euler.z+=angle

for identifier,korean,family,dims in SPECS:
 if FILTER and identifier not in FILTER:continue
 begin();w,d,H=dims
 if identifier.startswith('15_'):wall_segment((-3,0),(3,0))
 elif identifier.startswith('16_'):
  wall_segment((-3,-1),(0,-1));wall_segment((0,-1),(2.4,1.4));box('corner joint armored collar',(0,-1,.80),(.98,.98,1.60),'ConcreteShade');box('corner coping cover',(0,-1,1.61),(1.05,1.05,.14),'Metal')
 elif identifier.startswith('17_'):
  wall_segment((-2.5,-2.5),(2.5,-2.5));wall_segment((2.5,-2.5),(2.5,2.5));box('corner joint armored collar',(2.5,-2.5,.80),(.98,.98,1.60),'ConcreteShade');box('corner coping cover',(2.5,-2.5,1.61),(1.05,1.05,.14),'Metal')
 elif identifier.startswith('18_'):
  box('endcap foundation',(0,0,.15),(1.7,1.4,.3),'ConcreteShade');box('endcap armored pillar',(0,0,.98),(1.20,.95,1.66),'Metal');box('pillar crown',(0,0,1.86),(1.45,1.15,.12),'Concrete')
  for sy in [-1,1]:box('endcap inset',(0,sy*.50,.95),(.86,.06,1.1),'Dark','Facade');box('endcap reflector',(0,sy*.54,1.45),(.50,.035,.13),'Yellow','Signs')
  for x in [-.52,.52]:box('pillar vertical brace',(x,-.53,.95),(.12,.15,1.65),'Concrete','Facade')
 elif identifier.startswith('19_'):
  box('buttress footing',(0,0,.16),(2.5,3.2,.32),'ConcreteShade');box('buttress wall saddle',(0,1.05,.92),(2.2,.65,1.5),'Metal')
  for x in [-.70,.70]:
   tube('angled external load brace',[(x,-1.20,.32),(x,.95,1.95)],.16,'Concrete');box('brace base shoe',(x,-1.20,.42),(.58,.50,.24),'Metal')
   for xx in [-.14,.14]:cyl('anchor bolt',(x+xx,-1.20,.56),.04,.06,'Trim',verts=6)
  box('buttress center service deck',(0,-.10,.42),(1.9,1.6,.18),'Dark');box('support warning edge',(0,-1.45,.25),(2.3,.10,.10),'Yellow','Signs')
 elif identifier.startswith('20_'):
  plinth(w,d);box('gas skid deck',(0,0,.26),(w-.25,d-.25,.17),'Metal')
  for x in [-w*.35,w*.35]:box('skid longitudinal reinforcement',(x,0,.40),(.12,d-.40,.10),'Trim')
  tank(1.15,-.15,.38,.98,3.35);tank(3.10,-.15,.38,.98,3.35)
  for j,x in enumerate([-3.25,-2.1,-.95]):
   flue(x,.20,.38,2.45-j*.26,.42);cyl('stack lower jacket',(x,.20,1.0),.55,1.1,'Metal');ring('stack jacket band',(x,.20,1.50),.55,.055,'Trim')
  for j in range(3):tube('skid manifold pipe',[(-3.8,-1.5+j*.3,.58),(-.4,-1.5+j*.3,.58),(-.4,1.7,.58),(3.1,1.7,.58),(3.1,1.7,1.4+j*.2)],.075,'Trim',smooth=True)
  for x in [-3,-1,1,3]:wheel(x,-1.65,.80,.15)
  box('gas plant control pedestal',(-3.65,-1.7,.93),(.55,.4,1.1),'Metal');front_panel('gas pressure control screen',-3.65,-1.925,1.10,.38,.30,'AdTeal')
  rail('gas plant rear safety rail',(-4.2,2.3),(4.2,2.3),.38,'Metal')
 elif identifier.startswith('21_'):
  plinth(w,d);tank(0,.12,.22,.96,3.35)
 elif identifier.startswith('22_'):
  plinth(w,d);box('equipment foundation raised skirt',(0,0,.40),(w-.30,d-.30,.40),'Metal');box('orange compressor fascia',(0,-.15,1.50),(6.2,4.5,1.8),'Orange');box('compressor upper equipment case',(0,.1,2.80),(5.65,4.1,1.35),'Dark')
  for j in range(9):box('compressor front cooling fin',(-2.5+j*.63,-2.44,1.30),(.16,.18,1.0),'Metal','Facade')
  for x in [-1.8,0,1.8]:fan(x,.6,3.50,.54)
  tube('large bent exhaust duct',[(1.5,-.75,3.52),(1.5,-.75,4.25),(1.5,-1.6,4.25),(1.5,-1.6,3.6)],.25,'Trim',smooth=True)
  for x,y in [(-3.3,-1.8),(-3.3,.2),(-3.3,2.1)]:fan(x,y,.62,.59)
  for j in range(4):tube('compressor side pipe',[(-2.5+j*.45,-2.7,.70),(-2.5+j*.45,-2.7,2.45),(-2.5+j*.45,-1.5,2.45)],.065,'Trim',smooth=True)
  for x in [-2.2,0,2.2]:cyl('front pressure gauge',(x,-2.45,2.1),.18,.08,'Trim',axis='Y');tube('gauge needle',[(x,-2.50,2.1),(x+.08,-2.50,2.2)],.012,'Dark')
  hvac(3.2,1.6,.65,.85,1.2)
 elif identifier.startswith('23_'):
  for x in [-2.3,0,2.3]:
   for y in [-.60,.60]:box('pipe rack upright',(x,y,1.15),(.13,.13,2.3),'Metal');box('rack anchor foot',(x,y,.10),(.42,.38,.20),'ConcreteShade')
   box('rack upper crossbeam',(x,0,2.25),(.2,1.55,.16),'Metal')
  for j in range(4):
   y=-.55+j*.37;tube('long utility pipe',[(-2.7,y,2.36),(2.5,y,2.36),(2.5,y,1.90)],.09,'Trim' if j%2 else 'Orange',smooth=True)
   for x in [-1.7,1.65]:ring('pipe flange',(x,y,2.36),.15,.033,'Metal',axis='X')
  wheel(0,-.73,2.05,.17)
 elif identifier.startswith('24_'):
  box('barrier weighted footing',(0,0,.12),(4,.8,.24),'ConcreteShade');box('barrier yellow shell',(0,0,.61),(3.75,.57,.76),'Yellow')
  for j in range(9):
   o=box('black warning diagonal',(-1.63+j*.4,-.30,.61),(.18,.025,.78),'Dark','Signs');o.rotation_euler.y=-.4
  box('barrier top rail',(0,0,1.0),(3.9,.64,.10),'Metal')
  for x in [-1.7,1.7]:box('barrier white reflector',(x,-.33,.87),(.15,.02,.13),'Trim','Signs')
 else:
  plinth(w,d);box('control cabinet',(0,0,1.30),(w-.20,d-.20,2.15),'Metal');box('cabinet roof',(0,0,2.45),(w,d,.14),'ConcreteShade');front_panel('cabinet inset',0,-d/2-.01,1.3,w-.55,1.65,'Dark');front_panel('control terminal screen',-.30,-d/2-.05,1.65,.75,.48,'AdTeal')
  for j in range(5):box('cabinet louver',(0,-d/2-.07,.58+j*.10),(1.15,.04,.045),'ConcreteShade','Facade')
  for j in range(3):cyl('terminal status indicator',(.47,-d/2-.07,1.75-j*.19),.045,.025,['Cyan','Yellow','Orange'][j],axis='Y')
  box('cabinet door handle',(.62,-d/2-.12,1.1),(.06,.10,.32),'Trim','Facade')
 # Preserve individually named, editable parts. Export a merged render mesh and a simple collision proxy.
 groups={}
 for o in PARTS:
  tag=o.get('part_group','Structure')
  if tag not in groups:
   c=bpy.data.collections.new('EDIT_'+tag);bpy.context.scene.collection.children.link(c);groups[tag]=c
  for c in list(o.users_collection):c.objects.unlink(o)
  groups[tag].objects.link(o)
 for c in groups.values():c.hide_render=True;c.hide_viewport=True
 clones=[]
 for o in PARTS:
  q=o.copy();q.data=o.data.copy();bpy.context.scene.collection.objects.link(q);clones.append(q)
 bpy.ops.object.select_all(action='DESELECT')
 for o in clones:o.select_set(True)
 bpy.context.view_layer.objects.active=clones[0];bpy.ops.object.join();model=bpy.context.object;model.name='SM_CRB_'+identifier
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 low=[min(v.co[k] for v in model.data.vertices) for k in range(3)];high=[max(v.co[k] for v in model.data.vertices) for k in range(3)];factors=[dims[k]/max(.001,high[k]-low[k]) for k in range(3)]
 for v in model.data.vertices:v.co=Vector(((v.co.x-(low[0]+high[0])/2)*factors[0],(v.co.y-(low[1]+high[1])/2)*factors[1],(v.co.z-low[2])*factors[2]))
 # Apply the identical calibration to the editable source geometry.
 for o in PARTS:
  matrix=o.matrix_world.copy()
  for v in o.data.vertices:
   p=matrix@v.co;v.co=Vector(((p.x-(low[0]+high[0])/2)*factors[0],(p.y-(low[1]+high[1])/2)*factors[1],(p.z-low[2])*factors[2]))
  o.matrix_world=Matrix.Identity(4)
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.015);bpy.ops.object.mode_set(mode='OBJECT')
 collision_boxes=[]
 for part in PARTS:
  if not part.name.startswith('sloped concrete seawall'):continue
  pts=np.array([tuple(v.co) for v in part.data.vertices]);xy=pts[:,:2];center=xy.mean(axis=0);values,vectors=np.linalg.eigh(np.cov((xy-center).T));axis=vectors[:,-1];cross=np.array([-axis[1],axis[0]]);u=xy@axis;v=xy@cross;mid=axis*(u.min()+u.max())/2+cross*(v.min()+v.max())/2
  collision_boxes.append({'center_m':[float(mid[0]),float(mid[1]),float((pts[:,2].min()+pts[:,2].max())/2)],'size_m':[float(u.max()-u.min()),float(v.max()-v.min()),float(pts[:,2].max()-pts[:,2].min())],'yaw_deg':float(math.degrees(math.atan2(axis[1],axis[0])))})
 if not collision_boxes:collision_boxes=[{'center_m':[0,0,H/2],'size_m':[w*.94,d*.94,H],'yaw_deg':0}]
 collisions=[]
 for i,cb in enumerate(collision_boxes):
  bpy.ops.mesh.primitive_cube_add(size=1,location=cb['center_m']);collision=bpy.context.object;collision.name='UCX_'+model.name+'_'+str(i).zfill(2);collision.dimensions=cb['size_m'];collision.rotation_euler.z=math.radians(cb['yaw_deg']);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);collision.hide_render=True;collision.display_type='WIRE';collisions.append(collision)
 bpy.ops.object.select_all(action='DESELECT');model.select_set(True)
 for collision in collisions:collision.select_set(True)
 bpy.context.view_layer.objects.active=model
 bpy.ops.export_scene.fbx(filepath=str(ROOT/(model.name+'.fbx')),use_selection=True,object_types={'MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
 for collision in collisions:collision.select_set(False)
 bpy.ops.export_scene.gltf(filepath=str(ROOT/(model.name+'.glb')),export_format='GLB',use_selection=True)
 # Neutral scale figure, excluded from exports.
 preview=bpy.data.collections.new('PREVIEW_ONLY');bpy.context.scene.collection.children.link(preview)
 def preview_mesh(o):
  for c in list(o.users_collection):c.objects.unlink(o)
  preview.objects.link(o)
  o.data.materials.append(MAT['Orange'])
 px=-w/2-.8;py=-d/2-.6
 bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,radius=.15,location=(px,py,1.63));preview_mesh(bpy.context.object)
 for p,di in [((px,py,1.13),(.42,.23,.72)),((px-.12,py,.43),(.15,.18,.86)),((px+.12,py,.43),(.15,.18,.86)),((px-.29,py,1.16),(.13,.17,.62)),((px+.29,py,1.16),(.13,.17,.62))]:
  bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=di;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);preview_mesh(o)
 bpy.ops.mesh.primitive_plane_add(size=max(w,d,H)*8,location=(0,0,-.04));floor=bpy.context.object;floor.name='Preview stage';floor.data.materials.append(MAT['Roof']);preview_mesh(floor)
 scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=1000;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
 scene.world=bpy.data.worlds.new('Building studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.16,.22,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
 target=Vector((-.3,-.2,H*.43));span=max(w*1.15,d*1.3,H*1.12)
 bpy.ops.object.camera_add(location=target+Vector((1.2,-1.6,1.45))*span);camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=span*1.55;scene.camera=camera
 for pos,power,size,color in [((span,-span,span*2),span*span*70,span*1.6,(1,.90,.76)),((-span,-span*.4,span),span*span*35,span*1.5,(.50,.74,1)),((0,span,span*1.5),span*span*60,span,(.55,.85,1))]:
  bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.data.color=color;l.rotation_euler=(target-l.location).to_track_quat('-Z','Y').to_euler()
 scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ROOT/'Preview'/(identifier+'.png'))
 bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/(identifier+'.blend')));bpy.ops.render.render(write_still=True)
 row={'id':identifier,'name_ko':korean,'family':family,'dimensions_m':list(dims),'dimensions_ue_cm':[v*100 for v in dims],'origin':'ground center','front':'-Y','up':'+Z','triangles':sum(len(p.vertices)-2 for p in model.data.polygons),'vertices':len(model.data.vertices),'editable_parts':len(PARTS),'materials':[m.name for m in model.data.materials],'fbx':model.name+'.fbx','glb':model.name+'.glb','scale_reference_m':1.8}
 row['collision_boxes']=collision_boxes
 REPORT=[x for x in REPORT if x['id']!=identifier]+[row]
 MANIFEST.write_text(json.dumps({'reference_video':'https://youtu.be/ixn4lcqno2o','scale_method':'relative proportions from supplied map images and gameplay; 1.8m player reference, not original asset measurements','buildings':sorted(REPORT,key=lambda x:x['id'])},ensure_ascii=False,indent=2),encoding='utf-8')
 print('BUILDING_READY',identifier,row['triangles'],flush=True)
