import bpy,math,pathlib,json,random,sys
from mathutils import Vector,Matrix
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/OutskirtsRebuild/Models');ROOT.mkdir(exist_ok=True,parents=True)
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
 ('01_AbandonedHangar','폐쇄된 해안 격납고','Outpost',(9,7,5.2)),
 ('02_RuinedBarracks','붕괴된 병영','Outpost',(7.5,5.5,3.8)),
 ('03_ForwardBunker','전방 방호 벙커','Outpost',(6.8,4.8,3.2)),
 ('04_RadarRelay','해안 통신 레이더','Outpost',(3.2,3.2,6.0)),
 ('05_BatteryCannon','충전식 방어 대포','Outpost',(7.5,6.5,4.4)),
 ('06_GeneratorTrailer','견인식 발전기','Outpost',(5.8,2.8,2.6)),
 ('07_CargoCache','군용 화물 적재물','Outpost',(4.8,3.5,2.2)),
 ('08_BrokenPipeline','파손된 송유 배관','Outpost',(6.2,2.4,1.7)),
 ('09_ImpactCrater','대형 충돌 구덩이','Terrain',(12,10,1.6)),
 ('10_ShoreCliff','해안 절벽 군집','Terrain',(8,4.5,4.4)),
 ('11_RockSpine','날카로운 암석 능선','Terrain',(6,3.5,4.8)),
 ('12_ConcreteBarrier','파손된 방호벽','Outpost',(4.2,1.0,1.5)),
 ('13_HealingRelay','야전 회복 중계기','Outpost',(4.6,4.6,1.9)),
 ('14_BatteryCache','에너지 배터리 보관대','Outpost',(3.5,2.5,1.7)),
 ('15_RuinedWatchtower','폐허 감시 초소','Outpost',(4.2,3.5,6.0)),
 ('16_CableSpool','야전 케이블 릴','Outpost',(2.2,1.8,1.9))]
REPORT=[]
if MANIFEST.exists():REPORT=json.loads(MANIFEST.read_text(encoding='utf-8'))['buildings']
PALETTE.update({'Sand':(.46,.31,.16,1),'Earth':(.23,.14,.075,1),'Rock':(.24,.245,.215,1),'RockLight':(.38,.36,.29,1),'Military':(.18,.22,.19,1),'Rust':(.34,.15,.055,1),'Healing':(.03,.65,.28,1)})

def riveted_panel(n,p,d,mat='Military'):
 box(n,p,d,mat)
 x,y,z=p;w,dep,h=d
 for sx in [-1,1]:
  for sz in [-1,1]:cyl('panel corner fastener',(x+sx*(w/2-.13),y-dep/2-.018,z+sz*(h/2-.13)),.035,.035,'Metal',axis='Y',cat='Facade',verts=6)
def rock(n,p,d,seed=0):
 rng=random.Random(seed);bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=p);o=bpy.context.object
 for v in o.data.vertices:
  v.co.x*=rng.uniform(.8,1.12);v.co.y*=rng.uniform(.8,1.12);v.co.z*=rng.uniform(.75,1.2)
 o.scale=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);finish(o,n,'Rock' if seed%2 else 'RockLight','Structure',.025)
def crate(x,y,z,w=1.3,d=1.0,h=.8):
 riveted_panel('military cargo shell',(x,y,z+h/2),(w,d,h),'Military')
 for sx in [-1,1]:box('cargo locking strap',(x+sx*w*.32,y,z+h/2),(.09,d+.04,h+.04),'Metal')
 box('cargo top lid',(x,y,z+h+.025),(w+.02,d+.02,.06),'ConcreteShade');front_panel('cargo ID strip',x,y-d/2-.03,z+h*.6,w*.35,.13,'Yellow')
def ladder(x,y,z,h):
 for dx in [-.3,.3]:tube('ladder upright',[(x+dx,y,z),(x+dx,y,z+h)],.035,'Metal')
 for j in range(int(h/.30)):tube('ladder rung',[(x-.30,y,z+.2+j*.30),(x+.30,y,z+.2+j*.30)],.025,'Trim')

for identifier,korean,family,dims in SPECS:
 if FILTER and identifier not in FILTER:continue
 begin();random.seed(identifier);w,d,H=dims
 if identifier.startswith('01_'):
  plinth(w,d);box('hangar reinforced shell',(0,0,1.95),(w-.30,d-.3,3.5),'Military');poly('pitched hangar roof',[(-w/2,-d/2),(w/2,-d/2),(w/2,d/2),(-w/2,d/2)],3.82,.25,'ConcreteShade')
  for sx in [-1,1]:
   o=box('sloped roof armor',(sx*w*.24,0,4.05),(w*.51,d+.10,.20),'Metal');o.rotation_euler.y=sx*.24
  front_panel('hangar dark doorway',0,-d/2-.06,1.80,w*.62,2.8,'Dark')
  for j in range(12):front_panel('shutter door rib',-w*.285+j*w*.052,-d/2-.095,1.80,.24,2.65,'Metal')
  for x in [-w*.43,w*.43]:box('heavy door pier',(x,-d/2-.08,1.9),(.40,.45,3.6),'Concrete')
  box('hangar lintel',(0,-d/2-.16,3.40),(w*.9,.5,.35),'Concrete');text('hangar stencil','B-02',(0,-d/2-.42,3.43),.42,'Trim')
  for j in range(6):box('roof transverse seam',(-w*.4+j*w*.16,0,4.25),(.06,d+.12,.08),'ConcreteShade','Rooftop')
  hvac(w*.32,d*.26,3.90,.9,.7);tube('hangar utility riser',[(-w*.43,-d*.45,.4),(-w*.43,-d*.45,3.3),(-w*.28,-d*.45,3.3)],.075,'Rust',smooth=True)
 elif identifier.startswith('02_'):
  plinth(w,d);box('exposed barrack floor',(0,0,.28),(w-.25,d-.25,.20),'ConcreteShade')
  box('remaining rear wall',(0,d/2-.2,1.8),(w,.32,3.1),'Military');box('remaining side wall',(-w/2+.18,0,1.8),(.36,d,3.1),'Concrete')
  box('broken facade left',(-w*.32,-d/2+.12,1.6),(w*.30,.28,2.7),'ConcreteShade');box('broken facade right',(w*.35,-d/2+.12,.75),(w*.22,.28,1.1),'Military')
  box('partial roof plate',(-w*.23,d*.05,3.45),(w*.48,d*.70,.20),'Metal')
  for i in range(6):rock('collapsed wall rubble',(-.2+i*.42,-d*.17+random.uniform(-.4,.3),.5),(random.uniform(.25,.5),.35,.35),i)
  for i in range(4):tube('exposed bent rebar',[(w*.22+i*.18,-d/2,1.0),(w*.22+i*.18,-d/2,1.7),(w*.22+i*.18+.13,-d/2,1.8)],.02,'Rust')
  crate(-1,.8,.40,1.3,.9,.8);front_panel('barrack sealed window',-1,d/2-.38,2.1,1.2,.6,'Dark')
 elif identifier.startswith('03_'):
  plinth(w,d);pts=[(-w/2,-d/2),(w/2,-d/2),(w/2,d/2),(-w/2,d/2)];poly('sloped bunker armor',pts,1.45,2.5,'Military');box('bunker concrete overhang',(0,0,2.85),(w+.12,d+.10,.40),'ConcreteShade')
  for x in [-1.9,0,1.9]:front_panel('bunker observation slit',x,-d/2-.04,2.15,1.35,.32,'Dark');front_panel('slit armored lower edge',x,-d/2-.08,1.95,1.45,.08,'Metal')
  for x in [-w*.43,w*.43]:box('bunker front cheek',(x,-d/2-.13,1.35),(.5,.45,2.6),'Concrete')
  fan(-1,.5,3.05,.45);box('bunker roof access',(1.5,.5,3.07),(1.1,1.2,.14),'Metal');text('bunker number','04',(0,-d/2-.07,1.1),.6,'Trim')
 elif identifier.startswith('04_'):
  plinth(w,d);box('relay base',(0,0,.75),(2.0,1.8,1.2),'Military');cyl('antenna mast',(0,0,2.5),.12,3.5,'Metal');cyl('radar dish backing',(0,-.15,4.5),1.15,.22,'ConcreteShade',axis='Y',verts=32);ring('radar dish outer rim',(0,-.29,4.5),1.14,.055,'Trim',axis='Y')
  for a in [0,math.pi/4,math.pi/2,3*math.pi/4]:tube('dish rib',[(-math.cos(a)*1.0,-.30,4.5-math.sin(a)),(math.cos(a)*1.0,-.30,4.5+math.sin(a))],.025,'Metal')
  tube('radar feed arm',[(0,-.3,3.6),(0,-1.2,4.4),(0,-.4,4.5)],.055,'Metal');cyl('radar feed horn',(0,-1.2,4.4),.14,.28,'Orange',axis='Y');box('relay status panel',(0,-.94,.85),(.7,.05,.45),'AdTeal');ladder(.6,.2,.4,3.8)
 elif identifier.startswith('05_'):
  cyl('turret fortified base',(0,0,.25),2.65,.5,'ConcreteShade',verts=32);cyl('turret swivel bearing',(0,0,.65),1.65,.35,'Metal',verts=32);ring('turret charged base ring',(0,0,.85),1.48,.09,'Cyan');box('turret armored cradle',(0,.2,1.6),(3.2,2.4,1.5),'Military')
  for x in [-1.35,1.35]:cyl('elevation trunnion',(x,-.3,1.9),.45,.35,'ConcreteShade',axis='X')
  cyl('long defense barrel',(0,-1.9,2.2),.35,3.8,'Metal',axis='Y',verts=24);cyl('reinforced muzzle',(0,-3.55,2.2),.48,.48,'ConcreteShade',axis='Y',verts=24);cyl('dark muzzle bore',(0,-3.8,2.2),.31,.025,'Dark',axis='Y');ring('cyan muzzle energy',(0,-3.82,2.2),.32,.045,'Cyan',axis='Y')
  for y in [-2.8,-2.3,-1.8]:ring('barrel cooling collar',(0,y,2.2),.37,.06,'Metal',axis='Y')
  crate(2.3,.5,.5,1.1,.8,1.0);front_panel('charging control screen',1.3,-1.02,1.8,.55,.38,'AdTeal');tube('charging cable',[(1.4,0,1.4),(2.2,-.3,.45),(2.3,.5,.65)],.07,'Dark',smooth=True)
  for x,y in [(-2,1.5),(2,1.5)]:box('turret stabilizer',(x,y,.35),(.45,.8,.70),'Military')
 elif identifier.startswith('06_'):
  box('trailer steel chassis',(0,0,.60),(w-.2,d-.2,.3),'Metal');riveted_panel('generator equipment case',(0,.1,1.5),(4.2,2.1,1.6),'Military');box('teal power bank',(.3,-.01,2.32),(2.7,1.8,.13),'AdTeal')
  for x in [-1.65,1.65]:
   for y in [-1.12,1.12]:cyl('trailer rubber tire',(x,y,.5),.48,.28,'Dark',axis='Y');cyl('trailer wheel hub',(x,y*1.13,.5),.20,.04,'Metal',axis='Y')
  for j in range(8):front_panel('generator cooling louver',-1.4+j*.38,-1.08,1.45,.12,.80,'Dark')
  tube('trailer towing tongue',[(1.9,.65,.65),(2.8,0,.65),(1.9,-.65,.65)],.08,'Metal');flue(-1.3,.6,2.3,.5,.13)
 elif identifier.startswith('07_'):
  box('cargo pallet',(0,0,.1),(w,d,.20),'Metal')
  for x,y,z in [(-1.4,-.8,.20),(0,-.8,.20),(1.4,-.8,.20),(-.7,.6,.20),(.7,.6,.20),(-.65,-.3,1.05),(.7,-.3,1.05)]:crate(x,y,z,1.25,.95,.8)
 elif identifier.startswith('08_'):
  for x in [-2.5,0,2.5]:box('pipe support foot',(x,0,.25),(.55,1.8,.5),'ConcreteShade')
  for y in [-.5,.5]:
   cyl('broken pipe section',(-1.2,y,.88),.27,3.4,'Rust',axis='X');cyl('opposite pipe stub',(2.25,y,.88),.27,1.1,'Metal',axis='X');ring('open broken pipe flange',(.5,y,.88),.35,.065,'Metal',axis='X');cyl('pipe hollow dark bore',(.515,y,.88),.22,.01,'Dark',axis='X')
  tube('bent ruptured pipe',[(.7,-.5,.88),(1.1,-.5,1.3),(1.7,-.5,1.4)],.14,'Rust',smooth=True)
 elif identifier.startswith('09_'):
  # Concentric crater mesh remains traversable; the center is lower than the raised broken rim.
  rings=[(0,.08),(.24,.12),(.55,.30),(.77,1.15),(1,.24)];verts=[];N=48
  for ri,(r,z) in enumerate(rings):
   for j in range(N):
    a=j*math.tau/N;rr=r*(1+.055*math.sin(j*2.7));verts.append((math.cos(a)*rr*w/2,math.sin(a)*rr*d/2,z*(1+.14*math.sin(j*1.9))))
  faces=[]
  for ri in range(len(rings)-1):
   for j in range(N):a=ri*N+j;b=ri*N+(j+1)%N;faces.append((a,a+N,b+N,b))
  mesh=bpy.data.meshes.new('crater concentric rubble');mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new('crater concentric rubble',mesh);bpy.context.collection.objects.link(o);finish(o,'crater concentric rubble','Earth','Structure',0)
  for j in range(12):a=j*math.tau/12;rock('rim debris',(math.cos(a)*w*.39,math.sin(a)*d*.39,.75),(.35,.35,.35),j)
 elif identifier.startswith(('10_','11_')):
  for j in range(5 if identifier.startswith('10_') else 3):
   x=(j-2)*1.3 if identifier.startswith('10_') else (j-1)*1.7;rock('fractured coastal boulder',(x,random.uniform(-.35,.35),1.4),(1.25,1.25,random.uniform(1.3,2.3)),j*17+3)
 elif identifier.startswith('12_'):
  box('barrier broad base',(0,0,.15),(w,d,.3),'ConcreteShade');box('aged concrete barrier',(0,0,.85),(w-.15,d*.58,1.1),'Concrete')
  for j in range(5):
   o=box('barrier black warning band',(-1.6+j*.8,-d*.31,.85),(.27,.025,.7),'Dark','Signs');o.rotation_euler.y=-.45
  for x in [-1.7,1.7]:front_panel('barrier amber marker',x,-d*.34,1.15,.16,.10,'Yellow')
 elif identifier.startswith('13_'):
  cyl('healing relay foundation',(0,0,.18),2.1,.36,'ConcreteShade',verts=32);cyl('healing relay housing',(0,0,.65),1.55,.6,'Military',verts=32);ring('healing luminous perimeter',(0,0,.98),1.4,.12,'AdTeal');cyl('healing relay lens',(0,0,1.1),.9,.22,'Glass',verts=32)
  for j in range(6):a=j*math.tau/6;box('relay armored petal',(1.75*math.cos(a),1.75*math.sin(a),.7),(.65,.45,.55),'Metal','Structure',a)
  box('healing plus vertical',(0,0,1.30),(.20,1.05,.05),'Cyan','Signs');box('healing plus horizontal',(0,0,1.30),(1.05,.20,.05),'Cyan','Signs')
 elif identifier.startswith('14_'):
  box('battery storage pallet',(0,0,.15),(w,d,.30),'Metal')
  for x in [-1.1,0,1.1]:
   box('energy cell armor',(x,0,.85),(.85,1.7,1.1),'Military');box('energy cell luminous lid',(x,0,1.43),(.65,1.45,.10),'AdTeal');front_panel('battery charge meter',x,-.88,.90,.50,.30,'Cyan')
 elif identifier.startswith('15_'):
  plinth(w,d)
  for x in [-1.35,1.35]:
   for y in [-1.1,1.1]:box('tower support column',(x,y,2.35),(.20,.20,4.3),'Metal')
  for y in [-1.1,1.1]:tube('tower diagonal crossbrace',[(-1.35,y,.3),(1.35,y,4.2)],.07,'Metal');tube('tower reverse crossbrace',[(1.35,y,.3),(-1.35,y,4.2)],.07,'Metal')
  box('watch platform',(0,0,4.35),(3.4,2.8,.25),'ConcreteShade');box('watch cabin rear',(0,.95,5.0),(3.3,.15,1.15),'Military');box('watch cabin cover',(-.50,0,5.70),(2.4,2.8,.18),'Metal');rail('watch platform guard',(-1.5,-1.25),(1.5,-1.25),4.45,'Metal');ladder(1.35,-1.25,.3,4.05)
 else:
  for y in [-.65,.65]:cyl('cable reel flange',(0,y,.95),.9,.15,'Military',axis='Y',verts=24);ring('reel perimeter',(0,y,.95),.84,.05,'Metal',axis='Y')
  cyl('wound dark field cable',(0,0,.95),.58,1.15,'Dark',axis='Y',verts=32);cyl('reel spindle',(0,0,.95),.13,1.8,'Metal',axis='Y')
  tube('loose cable tail',[(.5,-.2,.4),(1,-.3,.1),(1.4,-.6,.08)],.035,'Dark',smooth=True)
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
