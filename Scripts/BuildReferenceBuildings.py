import bpy,math,pathlib,json,random,sys
from mathutils import Vector,Matrix
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/ReferenceBuildings');ROOT.mkdir(exist_ok=True)
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

for identifier,korean,family,dims in SPECS:
 if FILTER and identifier not in FILTER:continue
 begin();random.seed(identifier);w,d,H=dims;plinth(w,d)
 if identifier.startswith('01_'):
  body=H-.7;box('three level white volume',(0,.25,.2+body/2),(w-.30,d-.8,body));facade(w-.30,d-.8,.2,3,body/3,True,offset=(0,.25))
  for j in range(1,4):
   z=.2+j*body/3;box('projecting balcony slab',(0,-d/2+.12,z),(w+.10,.75,.12),'Trim','Facade');rail('balcony rail',(-w/2+.15,-d/2-.17),(w/2-.15,-d/2-.17),z)
   for x in [-w*.30,w*.28]:planter(x,-d/2+.12,z,.9,.40)
  garden(w-.65,d-1.05,H-.48,offset=(0,.25));roofrim(w-.30,d-.8,.2+body);hvac(w*.28,d*.14,.2+body,.9,.65);door(0,-d/2-.02,.2)
 elif identifier.startswith('02_'):
  fh=(H-.65)/4
  for j in range(4):
   ww=w-j*.85;dd=d-j*.55;yy=j*.18;z=.2+j*fh
   box('stepped apartment tier',(0,yy,z+fh/2),(ww,dd,fh));facade(ww,dd,z,1,fh,True,offset=(0,yy))
   garden(ww-.25,.65,z+fh,offset=(0,-dd/2+.37));rail('terrace rail',(-ww/2,-dd/2-.06),(ww/2,-dd/2-.06),z+fh)
   for x in [-ww*.32,ww*.32]:planter(x,-dd/2+.36,z+fh,.85,.38)
  garden(w-3*.85,d-3*.55,H-.45,offset=(0,.54));hvac(.8,1.2,H-.6,.8,.7);door(0,-d/2-.04,.2)
 elif identifier.startswith('03_'):
  body=H-.75;rad=.85;pts=[]
  for cx,cy,start in [(w/2-rad,d/2-rad,0),(-w/2+rad,d/2-rad,90),(-w/2+rad,-d/2+rad,180),(w/2-rad,-d/2+rad,270)]:
   for j in range(7):a=math.radians(start+j*15);pts.append((cx+rad*math.cos(a),cy+rad*math.sin(a)))
  poly('rounded residential mass',pts,.2+body/2,body)
  for j in range(3):
   z=.2+j*body/3;poly('wraparound orange fascia',[(x*1.015,y*1.015) for x,y in pts],z+.18,.17,'Orange','Facade');facade(w-1.4,d,z,1,body/3)
   for yy in [-d*.23,d*.08,d*.30]:box("rounded block return window",(w/2+.025,yy,z+body/6),(.055,.65,body*.20),"Glass","Facade")
  box('orange end wall',(-w/2+.2,.25,.2+body*.47),(.18,d*.56,body*.85),'Orange','Facade');garden(w-1.2,d-1.1,.2+body+.06);roofrim(w-.35,d-.35,.2+body);solar(.5,.35,H-.45,1.2,.9);door(.7,-d/2-.04,.2)
 elif identifier.startswith('04_'):
  h=H-.55;pts=[(-w/2,-d/2),(w/2,-d/2),(w/2,d/2),(w*.05,d/2),(w*.05,d*.02),(-w/2,d*.02)]
  poly('L shaped park annex',pts,.2+h/2,h);facade(w,d,.2,2,h/2,False);garden(w*.47,d*.40,.2+h,offset=(w*.24,d*.25));garden(w*.52,d*.40,.2+h,offset=(-w*.22,-d*.23));door(-.7,-d/2-.05,.2);hvac(w*.22,-d*.22,.2+h,.8,.7)
  for j in range(3):planter(-w*.42+j*.6,d*.12,.2,.5,.4)
 elif identifier.startswith('05_'):
  body=H-.85;ww=w-.35;dd=d-.35;box('slender media tower',(0,0,.2+body/2),(ww,dd,body),'ConcreteShade');facade(ww,dd,.2,9,body/9)
  front=-dd/2-.08
  for j in range(8):
   z=1.5+j*(body-2)/8;front_panel('cyan billboard floor strip',0,front-.02,z,ww-.30,1.05,'AdBlue')
   for sx,mat in [(-1,'Cyan'),(1,'Pink')]:
    o=box('diagonal media graphic',(sx*ww*.22,front-.07,z),(.34,.045,1.55),mat,'Signs');o.rotation_euler.y=sx*.65
  front_panel('lower media column',ww*.18,front-.10,2.0,ww*.45,2.8,'AdTeal');text('district tower wordmark','CITY',(-ww*.17,front-.14,2.15),.46,'Trim');bunny(ww*.18,front-.15,2.0,.7)
  roofrim(ww,dd,.2+body);hvac(-1,.55,.2+body,1.4,1.1);fan(1.2,.65,.2+body,.35);door(-1.3,front-.06,.2)
 elif identifier.startswith('06_'):
  body=H-1.0;pts=[(-w/2,-d/2+.85),(-w/2+.85,-d/2),(w/2,-d/2),(w/2,d/2),(-w/2,d/2)];poly('chamfered central billboard block',pts,.2+body/2,body,'ConcreteShade')
  front=-d/2-.06;front_panel('primary advertisement',.35,front,body*.54,w-1.10,body*.78,'AdBlue')
  for sx in [-1,1]:box('white advertisement vertical frame',(.35+sx*(w-1.05)/2,front-.05,body*.54),(.17,.14,body*.79),'Trim','Facade')
  for z in [1.2,body*.93]:box('white advertisement header sill',(.35,front-.05,z),(w-1.05,.14,.20),'Trim','Facade')
  front_panel('pink mascot screen',-.65,front-.09,body*.57,4.1,4.1,'Pink');bunny(-.65,front-.15,body*.54,1.35)
  for j in range(3):
   o=box('large diagonal facade stripe',(w*.25,front-.11,body*.72-j*1.8),(.24,.06,4.2),'Cyan','Signs');o.rotation_euler.y=-.85
  for j,mat in enumerate(['Pink','Yellow','Cyan','Yellow','AdTeal']):front_panel('arcade ground storefront',-w*.32+j*w*.16,front-.12,.72,w*.135,.85,mat)
  roofrim(w,d,.2+body);box('roof maintenance hatch',(w*.13,.3,.2+body+.15),(2.2,1.7,.22),'Roof','Rooftop');hvac(-2,1.25,.2+body,1.6,1.0);rail('roof service rail',(-w*.4,d*.38),(w*.35,d*.38),.2+body)
 elif identifier.startswith('07_'):
  body=H-.8;cut=1.30;pts=[(-w/2,-d/2+cut),(-w/2+cut,-d/2),(w/2,-d/2),(w/2,d/2),(-w/2,d/2)];poly('corner hologram tower',pts,.2+body/2,body,'ConcreteShade');front=-d/2-.07
  front_panel('vertical cyan hologram',.65,front,body*.52,w-cut-.25,body*.82,'AdTeal');bunny(.65,front-.07,body*.56,1.05);text('vertical ad wordmark','ANIMA',(.65,front-.09,2.0),.58,'Trim')
  for sx in [-1,1]:box('lightbox front mullion',(.65+sx*(w-cut-.15)/2,front-.03,body*.53),(.16,.15,body*.84),'Trim','Facade')
  box('side portrait advertisement',(w/2+.055,0,body*.55),(.06,d-.4,body*.72),'AdBlue','Signs');text('side neon lettering','CITY',(w/2+.11,0,body*.59),.72,'Pink',True)
  roofrim(w,d,.2+body);fan(0,.7,.2+body,.40);hvac(-1.4,.8,.2+body,.8,.9)
 elif identifier.startswith('08_'):
  segment=w/3;body=H-.8
  for j in range(3):
   x=(j-1)*segment;hh=body*(.88 if j==1 else 1)
   box('attached shop volume',(x,0,.2+hh/2),(segment-.04,d,hh),'ConcreteShade')
   for level in range(3):
    z=.2+level*hh/3;front_panel('shop glazed floor',x,-d/2-.04,z+hh/6,segment-.28,hh/3-.2,'Glass');box('shop floor band',(x,-d/2-.07,z+.1),(segment,.12,.16),'Trim','Facade')
   front_panel('shop vertical sign',x,-d/2-.13,hh*.60,segment-.30,1.60,['AdBlue','Pink','AdTeal'][j]);text('shop number',['C01','C02','C03'][j],(x,-d/2-.17,hh*.60),.45,'Trim');door(x,-d/2-.14,.2,.85,1.35);roofrim(segment-.08,d,.2+hh,offset=(x,0));hvac(x,.50,.2+hh,.9,.8)
 elif identifier.startswith('09_'):
  body=H-.85;box('utility office block',(0,0,.2+body/2),(w,d,body));facade(w,d,.2,5,body/5)
  for x in [w*.20,w*.35]:box('orange facade ribbon',(x,-d/2-.11,.2+body/2),(.32,.15,body-.1),'Orange','Facade')
  roofrim(w,d,.2+body);fan(-w*.23,.6,.2+body,.45);hvac(w*.20,.7,.2+body,1.0,.85);door(-.9,-d/2-.12,.2);tube('office return riser',[(-w/2-.12,d*.2,.4),(-w/2-.12,d*.2,body-.2),(0,d*.2,body-.2)],.075,'Metal','Facade',True)
 elif family=='Industrial':
  body=H*.63;box('factory main shell',(0,0,.2+body/2),(w,d,body),'Metal');box('roof inset',(0,0,.2+body),(w-.24,d-.24,.12),'Roof','Rooftop');roofrim(w,d,.2+body)
  for sy in [-1,1]:box('orange industrial fascia',(0,sy*(d/2+.035),body*.63),(w,.10,.42),'Orange','Facade')
  for sx in [-1,1]:box('corner reinforced pier',(sx*(w/2-.12),-d/2-.08,.2+body/2),(.24,.20,body),'ConcreteShade','Facade')
  for j in range(8):box('wall ventilation grille',(-w*.38+j*w*.105,-d/2-.07,body*.35),(.10,.055,.52),'Dark','Facade')
  door(w*.28,-d/2-.10,.2,1.1,1.7);roof=.2+body
  if identifier.startswith('10_'):
   box('boiler raised roof',(-w*.17,.35,roof+.32),(w*.45,d*.48,.65),'Dark','Rooftop')
   for j in range(3):flue(w*.05+j*.85,-d*.18,roof,1.8-j*.12,.33)
   for j in range(3):tube('boiler exposed feed pipe',[(-w*.42+j*.35,-d/2-.12,.65),(-w*.42+j*.35,-d/2-.12,roof+.18),(-w*.42+j*.35,.65,roof+.18)],.09,'Trim','Rooftop',True)
   fan(-w*.27,d*.27,roof+.65,.42)
  elif identifier.startswith('11_'):
   for sx in [-1,1]:cyl('pump vessel',(sx*w*.21,0,roof+.45),.57,.9,'Dark');ring('pump cap',(sx*w*.21,0,roof+.92),.50,.055,'Cyan')
   for j in range(4):tube('parallel hydraulic line',[(-w*.40+j*.28,-d*.35,roof+.12),(-w*.40+j*.28,d*.33,roof+.12),(w*.28,d*.33,roof+.12)],.06,'Trim','Rooftop',True)
   box('pump control panel',(w*.20,-d*.33,roof+.30),(1.35,.62,.55),'Metal','Rooftop');box('control monitor',(w*.20,-d*.66/2-.335,roof+.30),(.72,.025,.27),'AdTeal','Rooftop')
  else:
   for x,y in [(-w*.25,-d*.12),(-w*.25,d*.22),(w*.08,d*.25)]:fan(x,y,roof,.55)
   box('ventilation penthouse',(w*.20,-d*.15,roof+.30),(w*.46,d*.43,.60),'Dark','Rooftop')
   for j in range(7):box('penthouse grille',(w*.01+j*.35,-d*.37,roof+.32),(.10,.05,.42),'Metal','Rooftop')
   tube('large white gooseneck duct',[(w*.20,d*.18,roof+.65),(w*.20,d*.18,roof+1.4),(w*.20,-d*.08,roof+1.4),(w*.20,-d*.08,roof+.68)],.22,'Trim','Rooftop',True);ring('duct service flange',(w*.20,d*.18,roof+.70),.27,.04,'Metal')
 elif family=='Utility':
  body=H*(.85 if identifier.startswith('13_') else .76);box('industrial tower shaft',(0,0,.2+body/2),(w,d,body),'Metal')
  for j in range(5):
   z=.2+j*body/5;box('stacked service belt',(0,-d/2-.04,z+.18),(w+.12,.15,.22),'ConcreteShade','Facade')
   front_panel('recessed tower service bay',0,-d/2-.05,z+body/10,w-.5,body/5-.42,'Dark')
   for x in [-w*.29,0,w*.29]:box('tower vertical conduit',(x,-d/2-.14,z+body/10),(.055,.08,body/5-.55),'Metal','Facade')
  for sx in [-1,1]:box('reinforced vertical tower mullion',(sx*(w/2-.12),-d/2-.07,.2+body/2),(.22,.23,body),'ConcreteShade','Facade')
  roof=.2+body;roofrim(w,d,roof);door(0,-d/2-.17,.2,1,1.55)
  for j in range(4):box('side maintenance rung',(w/2+.07,d*.23,1+j*.32),(.14,.64,.04),'Trim','Facade')
  if identifier.startswith('13_'):
   fan(-.7,.5,roof,.42);hvac(.7,.45,roof,.8,.7);front_panel('stack control display',0,-d/2-.18,body*.73,w*.47,.85,'AdTeal')
   for j in range(3):cyl('top valve manifold',(-w*.28+j*w*.28,-d*.23,roof+.32),.19,.64,'Orange')
  else:
   for sx in [-1,1]:flue(sx*.82,.35,roof,1.9,.43)
   cyl('teal intake filter',(-w*.27,-d*.24,roof+.40),.34,.8,'AdTeal');tube('white connecting riser',[(w*.24,-d*.24,roof+.12),(w*.24,-d*.24,roof+.83),(-w*.05,-d*.24,roof+.83),(-w*.05,-d*.24,roof+.36)],.14,'Trim','Rooftop',True)
 # Additional readable service details on commercial return walls.
 if family=='Commercial':
  for level in range(1,6):
   z=H*.12+level*H*.12
   box('return wall horizontal panel joint',(w/2-.02,0,z),(.035,d*.88,.045),'Dark','Facade')
   for yy in [-d*.26,d*.18]:
    box('return wall recessed glazing',(w/2+.015,yy,z+H*.045),(.045,d*.23,H*.055),'Glass','Facade')
  tube('rear utility downpipe',[(-w*.40,d/2+.04,.3),(-w*.40,d/2+.04,H*.88)],.055,'Metal','Facade')
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
 MANIFEST.write_text(json.dumps({'reference_video':'https://youtu.be/ixn4lcqno2o','scale_method':'relative proportions from supplied map images and gameplay; 1.8m player reference, not original asset measurements','buildings':sorted(REPORT,key=lambda x:x['id'])},ensure_ascii=False,indent=2),encoding='utf-8')
 print('BUILDING_READY',identifier,row['triangles'],flush=True)
