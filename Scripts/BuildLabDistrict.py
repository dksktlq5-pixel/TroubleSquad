import bpy,math,pathlib,json,random,sys
from mathutils import Vector,Matrix
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/LabRebuild');ROOT.mkdir(exist_ok=True)
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


PALETTE.update({'Deck':(.065,.10,.12,1),'DeckPanel':(.09,.145,.155,1),'Metal':(.055,.09,.105,1),'Trim':(.28,.39,.40,1),'Dark':(.008,.016,.024,1),'ConcreteShade':(.035,.065,.075,1),'Cyan':(.025,.62,.82,1),'AdTeal':(.015,.40,.25,1),'Water':(.025,.37,.53,1),'Pale':(.31,.52,.57,1)})
begin();layout=json.loads((ROOT/'Layout.json').read_text(encoding='utf-8'));INDEX=0

def slab(n,pts,z,h,mat):return poly(n,[(x,-y) for x,y in reversed(pts)],z,h,mat)
def ub(n,x,y,z,w,d,h,mat='Deck',yaw=0):return box(n,(x,-y,z),(w,d,h),mat,yaw=-math.radians(yaw))
def line(n,a,b,width=.10,mat='Trim',z=.09):
 global INDEX
 INDEX+=1;dx=b[0]-a[0];dy=b[1]-a[1];return ub(n,(a[0]+b[0])/2,(a[1]+b[1])/2,z+INDEX*.000005,math.hypot(dx,dy),width,.015,mat,math.degrees(math.atan2(dy,dx)))
def loop(n,pts,width=.10,mat='Trim',z=.09):
 for i,a in enumerate(pts):line(n+str(i),a,pts[(i+1)%len(pts)],width,mat,z)
slab('subterranean structural foundation',layout['main_floor'],-.35,.7,'Metal');slab('continuous experimental deck',layout['main_floor'],.025,.05,'Deck');loop('outer deck edging',layout['main_floor'],.28,'Trim',.075)
# Large panel seams with narrow manufactured bevels instead of plain unbroken floor.
for x in range(-44,24,4):
 for y in range(-26,28,4):
  ub('deck inset panel',x,y,.059,3.92,3.92,.012,'DeckPanel')
  for dx in [-1.65,1.65]:
   for dy in [-1.65,1.65]:cyl('deck panel fastener',(x+dx,-y+dy,.071),.035,.014,'Metal',cat='Facade',verts=6)
# Lower east access apron and the boss-room approach are left free of machines.
ub('east service apron',33,17,.035,15,26,.07,'Deck');slab('boss entry connecting deck',layout['entry_floor'],.065,.12,'DeckPanel');line('evacuation guide stripe',(-46,-6.8),(19,-6.8),.10,'Cyan',.09)
for x in range(-44,30,3):
 for y in [-3.1,3.1]:line('central corridor dotted lane',(x,y),(x+1.3,y),.07,'Trim',.085)
for y in [-7.9,7.9]:line('central corridor raised side trim',(-46,y),(23,y),.15,'Metal',.12)
# Cyan coolant trenches, segmented railings and wide bridges share the same data as AI crossing hints.
for c in layout['channels']:
 x0,x1,y0,y1=c['x_min'],c['x_max'],c['y_min'],c['y_max'];ub('cyan coolant trough',(x0+x1)/2,(y0+y1)/2,.08,x1-x0,y1-y0,.06,'Water')
 for y in [y0,y1]:
  breaks=[x0]+[v for x in layout['bridge_x'] for v in [x-2.65,x+2.65] if x0<v<x1]+[x1]
  for i in range(0,len(breaks)-1,2):
   a,b=breaks[i],breaks[i+1]
   if b-a<.1:continue
   line('coolant trench steel curb',(a,y),(b,y),.23,'Metal',.12)
   tube('coolant guard upper rail',[(a,-y,.95),(b,-y,.95)],.035,'Trim','Facade')
   for j in range(max(2,int((b-a)/1.7))+1):
    t=j/max(2,int((b-a)/1.7));ub('coolant guard stanchion',a+(b-a)*t,y,.50,.065,.065,.90,'Metal')
  line('recessed coolant guide light',(x0,y),(x1,y),.08,'Cyan',.17)
 for x in range(int(x0)+1,int(x1),4):
  for y in [y0+.7,y1-.7]:ub('coolant surface shimmer',x,y,.115,1.1,.08,.012,'Cyan')
# Freeze-pad approach markings remain visible around the non-blocking floor switches.
for x,y in layout['freeze_stations']:
 pts=[(x+2.7*math.cos(j*math.tau/8),y+2.7*math.sin(j*math.tau/8)) for j in range(8)];slab('freeze activation apron',pts,.08,.05,'DeckPanel');loop('freeze activation outline',pts,.12,'Cyan',.13)
# A separate circular east chamber is reached through the runtime gate at X=35.5m.
x,y=layout['boss_floor_center'];r=layout['boss_floor_radius'];pts=[(x+r*math.cos(j*math.tau/64),y+r*math.sin(j*math.tau/64)) for j in range(64)];slab('boss chamber raised foundation',pts,-.20,.6,'ConcreteShade');slab('boss chamber interior deck',pts,.12,.05,'Pale')
for rr,mat,ww in [(7.8,'Metal',.15),(6.8,'Cyan',.09),(3.9,'Deck',.08)]:
 loop('boss chamber concentric band',[(x+rr*math.cos(j*math.tau/64),y+rr*math.sin(j*math.tau/64)) for j in range(64)],ww,mat,.16)
for j in range(12):
 a=j*math.tau/12;line('boss chamber radial expansion joint',(x+1.6*math.cos(a),y+1.6*math.sin(a)),(x+7.7*math.cos(a),y+7.7*math.sin(a)),.055,'Deck',.17)
# Consolidate authored deck and low rails into one collision-tested environment mesh.
bpy.ops.object.select_all(action='DESELECT')
for o in PARTS:o.select_set(True)
bpy.context.view_layer.objects.active=PARTS[0];bpy.ops.object.join();ground=bpy.context.object;ground.name='SM_LabRebuildDeck';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.2,island_margin=.003);bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_LabRebuildDeck.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
cache={}
for i,row in enumerate(layout['placements']):
 name='SM_CRB_'+row['id']
 if name not in cache:
  with bpy.data.libraries.load(str(ROOT/'Models'/(row['id']+'.blend')),link=False) as (a,b):b.objects=[name]
  cache[name]=b.objects[0]
 obj=cache[name].copy();obj.data=cache[name].data;bpy.context.collection.objects.link(obj);px,py,pz=row['position_m'];obj.location=(px,-py,pz);obj.rotation_euler.z=-math.radians(row['yaw_deg']);obj.scale=(row['scale'],)*3;obj.name='Lab '+str(i)+' '+row['id']
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=2200;scene.render.resolution_y=1500;scene.world=bpy.data.worlds.new('Lab studio');scene.world.color=(.015,.022,.035)
target=Vector((0,0,0));bpy.ops.object.camera_add(location=(12,-66,108));cam=bpy.context.object;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=130;scene.camera=cam
for pos,power,size in [((25,-40,85),110000,65),((-65,-10,60),65000,60),((0,70,80),100000,60)]:
 bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.rotation_euler=(target-l.location).to_track_quat('-Z','Y').to_euler()
scene.view_settings.view_transform='AgX';scene.view_settings.exposure=-.10;scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ROOT/'LabOverview.png');bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SubterraneanLabRebuilt.blend'))
bpy.ops.render.render(write_still=True);print('LAB_SOURCE_COMPLETE',len(layout['placements']),flush=True)
