import bpy,math,pathlib,json,random,sys
from mathutils import Vector,Matrix
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/LabRebuild/Models');ROOT.mkdir(exist_ok=True,parents=True)
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
 ('01_HorizontalCultureTank','수평 배양 탱크','Lab',(7.2,3.6,3.1)),
 ('02_SpecimenPod','수직 실험체 캡슐','Lab',(3.2,3.2,5.2)),
 ('03_ArmoredPartition','실험실 장갑 격벽','Lab',(8,1.2,3.3)),
 ('04_PipeManifold','냉각수 분배 매니폴드','Lab',(6.5,3.0,3.0)),
 ('05_ReactorBank','다중 실험 반응기','Lab',(6.0,5.0,4.2)),
 ('06_ConsoleIsland','실험 제어 콘솔','Lab',(4.8,3.0,1.9)),
 ('07_CryoPylon','급속 냉각 장치','Lab',(4.2,4.2,1.8)),
 ('08_ServiceBridge','수로 연결 다리','Lab',(6.2,5.0,1.0)),
 ('09_PartitionCorner','코너 장갑 격벽','Lab',(4.2,4.2,3.3)),
 ('10_CoolantPipe','대형 냉각 배관','Lab',(8.0,1.7,1.6)),
 ('11_CableTray','차폐 케이블 트레이','Lab',(5.0,1.4,.65)),
 ('12_VentilationCluster','실험실 환기 설비','Lab',(4.5,3.5,2.7)),
 ('13_BossDoorFrame','보스실 차폐 출입구','Lab',(5.8,2.2,5.2)),
 ('14_ChamberRim','보스실 원형 방호 림','Lab',(18.4,18.4,1.1)),
 ('15_PumpSkid','순환 냉각 펌프','Lab',(5.5,3.5,2.6)),
 ('16_ServiceLocker','실험실 보관 함체','Lab',(3.0,1.4,2.7))]
PALETTE.update({'Concrete':(.13,.19,.18,1),'ConcreteShade':(.07,.12,.13,1),'Metal':(.065,.105,.115,1),'Trim':(.37,.46,.43,1),'Dark':(.012,.022,.027,1),'Glass':(.018,.23,.16,1),'AdTeal':(.018,.45,.22,1),'Cyan':(.025,.65,.82,1),'Orange':(.65,.29,.08,1)})
REPORT=[]
if MANIFEST.exists():REPORT=json.loads(MANIFEST.read_text(encoding='utf-8'))['buildings']
def tank(x,y,z,r=.95,L=5.5):
 cyl('containment green cylinder',(x,y,z+r),r,L,'AdTeal',axis='X',verts=32)
 for sign in [-1,1]:
  xx=x+sign*L/2;cyl('armored tank endcap',(xx,y,z+r),r*1.06,.28,'Metal',axis='X',verts=24);ring('silver cap retaining rim',(xx+sign*.15,y,z+r),r*.83,.06,'Trim',axis='X');cyl('endcap hatch',(xx+sign*.18,y,z+r),r*.39,.04,'ConcreteShade',axis='X')
 for f in [-.28,.28]:ring('containment reinforcement band',(x+f*L,y,z+r),r*1.025,.09,'Metal',axis='X')
 for f in [-.33,.33]:box('tank mounting saddle',(x+f*L,y,z+.20),(.50,r*1.9,.5),'ConcreteShade')
 tube('tank service cable',[(x-L*.3,y-r*.85,z+.30),(x,y-r*1.2,z+.12),(x+L*.3,y-r*.85,z+.30)],.065,'Dark',smooth=True)
def slit(n,x,y,z,w,h):front_panel(n,x,y,z,w,h,'Dark')
def slab_joint(x,y,z,w,d):box('equipment base plate',(x,y,z),(w,d,.20),'Metal')
for identifier,korean,family,dims in SPECS:
 if FILTER and identifier not in FILTER:continue
 begin();w,d,H=dims
 MAT['AdTeal'].node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value=.85
 if identifier.startswith('01_'):
  plinth(w,d);tank(0,0,.28,.95,5.9)
  for x in [-2.5,2.5]:tube('tank coolant elbow',[(x,.3,.5),(x,1.3,.5),(x,1.3,1.0)],.15,'Metal',smooth=True)
  box('containment top service spine',(0,0,2.35),(3.7,.6,.22),'ConcreteShade');box('tank status screen',(1.3,-1.02,1.25),(.66,.08,.44),'Cyan','Signs');text('tank serial','BX-07',(-1.2,-1.02,1.15),.28,'Trim')
 elif identifier.startswith('02_'):
  plinth(w,d);cyl('capsule reinforced base',(0,0,.45),1.25,.55,'Metal',verts=24);cyl('green specimen core',(0,0,2.45),.93,3.4,'AdTeal',verts=32);cyl('capsule top shell',(0,0,4.35),1.2,.44,'ConcreteShade',verts=24)
  for j in range(4):
   a=j*math.tau/4;box('capsule vertical spine',(1.03*math.cos(a),1.03*math.sin(a),2.4),(.17,.17,3.55),'Metal')
  for z in [.85,3.95]:ring('capsule pressure seal',(0,0,z),1.05,.08,'Trim')
  fan(0,0,4.58,.38);front_panel('pod diagnostic display',0,-1.31,.7,.70,.3,'Cyan')
 elif identifier.startswith(('03_','09_')):
  lengths=[((0,0),w,0)] if identifier.startswith('03_') else [((0,-1.5),4,0),((1.5,0),4,math.pi/2)]
  for (x,y),L,a in lengths:
   start=len(PARTS);box('armored wall footing',(0,0,.2),(L,1.0,.4),'ConcreteShade');box('heavy beveled lab wall',(0,0,1.65),(L,.65,2.8),'Metal');box('wall coping lip',(0,0,3.12),(L+.1,.95,.16),'Trim')
   for xx in [-L*.43,0,L*.43]:box('wall reinforcement pier',(xx,-.05,1.6),(.28,1.05,3.1),'ConcreteShade')
   for xx in [-L*.23,L*.23]:slit('wall recessed panel',xx,-.34,1.65,L*.34,1.65);box('cyan recessed foot strip',(xx,-.37,.55),(L*.28,.035,.12),'Cyan','Signs')
   for o in PARTS[start:]:
    xx,yy=o.location.x,o.location.y;o.location.x=x+math.cos(a)*xx-math.sin(a)*yy;o.location.y=y+math.sin(a)*xx+math.cos(a)*yy;o.rotation_euler.z+=a
 elif identifier.startswith('04_'):
  plinth(w,d)
  for j in range(3):
   y=-.75+j*.75;tube('routed manifold trunk',[(-2.7,y,.45),(-2.7,y,1.75),(1.9,y,1.75),(1.9,y,.6)],.20,'Metal',smooth=True)
   cyl('green fluid indicator',(-.4,y,1.75),.21,2.2,'AdTeal',axis='X');ring('manifold flange',(.7,y,1.75),.29,.07,'Trim',axis='X')
  box('manifold side housing',(2.7,0,1.2),(.75,2.45,1.9),'ConcreteShade');front_panel('flow control readout',2.7,-1.26,1.5,.45,.6,'Cyan');fan(2.7,0,2.2,.28)
 elif identifier.startswith('05_'):
  plinth(w,d);box('reactor bank lower cabinet',(0,0,.65),(w-.25,d-.25,.9),'Metal')
  for x in [-1.5,1.5]:
   for y in [-1.3,1.3]:cyl('reactor pressure vessel',(x,y,2.1),.75,2.1,'AdTeal',verts=24);cyl('reactor armored lid',(x,y,3.25),.9,.25,'Metal',verts=24);ring('reactor lid gasket',(x,y,3.4),.75,.06,'Trim');cyl('reactor top valve',(x,y,3.65),.16,.45,'Orange')
  for x in [-1.5,1.5]:tube('reactor crossfeed',[(x,-1.3,1.0),(x,0,1.0),(x,1.3,1.0)],.12,'Trim')
 elif identifier.startswith('06_'):
  plinth(w,d);box('console lower cabinet',(0,0,.75),(w-.3,d-.35,1.1),'Metal')
  o=box('sloped console fascia',(0,-.3,1.45),(w-.15,1.8,.18),'ConcreteShade');o.rotation_euler.x=.30
  for x in [-1.45,0,1.45]:
   o=box('cyan terminal display',(x,-.3,1.59),(1.10,1.02,.04),'AdTeal','Signs');o.rotation_euler.x=.30
   for j in range(3):box('terminal input key',(x-.28+j*.28,-1.03,1.38),(.13,.12,.035),'Trim','Facade')
  for y in [-.4,.3,.8]:box('console side cooling louver',(w/2-.12,y,.85),(.04,.07,.50),'Dark')
 elif identifier.startswith('07_'):
  cyl('freeze mechanism base',(0,0,.16),1.95,.32,'Metal',verts=32);cyl('icy activation surface',(0,0,.36),1.46,.12,'AdBlue',verts=32);ring('cryogenic cyan halo',(0,0,.48),1.6,.08,'Cyan')
  for j in range(6):
   a=j*math.tau/6;box('freeze station armored radial',(1.75*math.cos(a),1.75*math.sin(a),.75),(.55,.38,1.0),'ConcreteShade','Structure',a)
   tube('snowflake control spoke',[(0,0,.45),(1.1*math.cos(a),1.1*math.sin(a),.45)],.04,'Cyan','Signs')
   for side in [-1,1]:tube('snowflake branch',[(.65*math.cos(a),.65*math.sin(a),.45),(.82*math.cos(a+side*.36),.82*math.sin(a+side*.36),.45)],.03,'Cyan','Signs')
 elif identifier.startswith('08_'):
  box('bridge deck slab',(0,0,.16),(w,d,.32),'Metal')
  for x in [-w*.44,w*.44]:box('bridge edge landing',(x,0,.17),(.4,d+.05,.34),'ConcreteShade')
  for j in range(17):box('bridge traction rib',(-w*.42+j*w*.052,0,.34),(.06,d*.87,.025),'Trim','Facade')
  for y in [-d/2+.08,d/2-.08]:rail('bridge guard',(-w*.44,y),(w*.44,y),.32,'Metal')
 elif identifier.startswith('10_'):
  cyl('coolant pressure pipe',(0,0,.75),.48,w-.3,'AdTeal',axis='X',verts=24)
  for x in [-w*.45,-w*.22,w*.22,w*.45]:ring('large pipe coupling',(x,0,.75),.55,.10,'Metal',axis='X')
  for x in [-w*.32,w*.32]:box('coolant pipe saddle',(x,0,.25),(.35,1.35,.5),'ConcreteShade')
 elif identifier.startswith('11_'):
  box('cable tray case',(0,0,.14),(w,d,.28),'Metal')
  for y in [-.4,0,.4]:tube('insulated signal cable',[(-w*.47,y,.32),(w*.47,y,.32)],.10,'Dark')
  for j in range(12):box('tray grate crossbar',(-w*.44+j*w*.08,0,.49),(.07,d,.08),'Trim')
 elif identifier.startswith('12_'):
  plinth(w,d);box('fan cluster machinery shell',(0,0,1.05),(w-.1,d-.1,1.7),'Metal')
  for x,y in [(-1.1,-.6),(1.1,-.6),(0,.8)]:fan(x,y,1.9,.62)
  for j in range(8):box('fan bank front intake',(-1.7+j*.48,-d/2-.025,1.2),(.10,.04,.75),'Dark','Facade')
 elif identifier.startswith('13_'):
  for x in [-w*.43,w*.43]:box('doorframe armored post',(x,0,(H-.6)/2),(.72,d,H-.6),'Metal');box('portal cyan side band',(x,-d/2-.025,H*.52),(.16,.04,H*.65),'Cyan','Signs')
  box('doorframe overhead lintel',(0,0,H-.3),(w,d,.6),'ConcreteShade');box('portal cyan header',(0,-d/2-.03,H-.38),(w*.72,.05,.12),'Cyan','Signs');text('portal sector text','SECTOR 03',(0,-d/2-.08,H-.10),.28,'Trim')
 elif identifier.startswith('14_'):
  for j in range(36):
   a=j*math.tau/36;b=(j+1)*math.tau/36
   if 220<=math.degrees(a)<270:continue
   pts=[(r*math.cos(t),r*math.sin(t)) for r,t in [(8.35,a),(9.2,a),(9.2,b),(8.35,b)]];poly('chamber rim armored segment',pts,.40,.80,'Metal')
   tube('chamber rim cyan strip',[(8.5*math.cos(a),8.5*math.sin(a),.84),(8.5*math.cos(b),8.5*math.sin(b),.84)],.055,'Cyan','Signs')
   if j%3==0:box('chamber outer brace',(8.8*math.cos(a),8.8*math.sin(a),.65),(.6,.4,1.1),'ConcreteShade','Structure',a)
 elif identifier.startswith('15_'):
  plinth(w,d)
  for x in [-1.4,1.4]:
   cyl('pump motor casing',(x,0,1.1),.65,2.0,'Metal',axis='Y',verts=24);cyl('green pump sight glass',(x,-1.05,1.1),.43,.04,'AdTeal',axis='Y')
   tube('pump outlet riser',[(x,.9,1.1),(x,1.2,1.1),(x,1.2,2.1),(0,1.2,2.1)],.15,'Trim',smooth=True)
  box('pump electrical case',(0,-.7,.6),(.8,.7,.75),'ConcreteShade');front_panel('pump status',0,-1.07,.75,.45,.30,'Cyan')
 else:
  box('locker plinth',(0,0,.12),(w,d,.24),'ConcreteShade');box('locker bank',(0,0,1.45),(w-.1,d-.1,2.5),'Metal')
  for x in [-w/3,0,w/3]:
   slit('locker door inset',x,-d/2-.02,1.45,w*.28,2.1);box('locker door handle',(x+.22,-d/2-.07,1.3),(.05,.10,.3),'Trim','Facade')
   for j in range(3):box('locker vent',(x,-d/2-.05,2.1+j*.12),(.6,.03,.04),'ConcreteShade')
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
 bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,H/2));collision=bpy.context.object;collision.name='UCX_'+model.name+'_00';collision.dimensions=(w*.94,d*.94,H);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);collision.hide_render=True;collision.display_type='WIRE'
 bpy.ops.object.select_all(action='DESELECT');model.select_set(True);collision.select_set(True);bpy.context.view_layer.objects.active=model
 bpy.ops.export_scene.fbx(filepath=str(ROOT/(model.name+'.fbx')),use_selection=True,object_types={'MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
 collision.select_set(False);bpy.ops.export_scene.gltf(filepath=str(ROOT/(model.name+'.glb')),export_format='GLB',use_selection=True)
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
 REPORT=[x for x in REPORT if x['id']!=identifier]+[row]
 MANIFEST.write_text(json.dumps({'reference_video':'https://youtu.be/n_9mwZX9zgU','scale_method':'relative proportions from supplied map images and gameplay; 1.8m player reference, not original asset measurements','buildings':sorted(REPORT,key=lambda x:x['id'])},ensure_ascii=False,indent=2),encoding='utf-8')
 print('BUILDING_READY',identifier,row['triangles'],flush=True)
