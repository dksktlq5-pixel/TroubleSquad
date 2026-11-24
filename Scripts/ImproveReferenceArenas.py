"""Reference-driven arena landmarks and layered surfaces; preserves boss routes and arena bounds."""
import unreal,math,random,json
random.seed(19011)
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
base='/Game/TroubleSquad/Maps/ArenaMaterials/'
meshes={
'box':'/Engine/BasicShapes/Cube','disc':'/Engine/BasicShapes/Cylinder','rock':'/Game/WaterPlane/Environment/Props/SM_Rock',
'case':'/Game/ModSci_EngiProps/Meshes/SM_Case_A','tank':'/Game/ModSciInteriors/Meshes/SM_WVC_Tank_A',
'bus':'/Game/Cartoon_City_Free/Meshes/Props/SM_Bus_Stop_02','bush':'/Game/Cartoon_City_Free/Meshes/Vegetation/SM_Bush_07',
'building':'/Game/Cartoon_City_Free/Meshes/Buildings/SM_Eco_Building_Grid','car':'/Game/Cartoon_City_Free/Meshes/Cars/SM_Futuristic_Car_1',
'light':'/Game/Cartoon_City_Free/Meshes/Props/SM_Spotlight_01','pipe':'/Game/ModSci_EngiProps/Meshes/SM_CoatedPipe_A'}
meshes={k:unreal.load_asset(v) for k,v in meshes.items()};assert all(meshes.values())
mats={k:unreal.load_asset(base+n) for k,n in {'road':'M_ReferenceRoadPanels','sand':'M_ReferenceSandGrain','steel':'M_ArenaSteel','water':'M_ArenaWater','cyan':'M_ArenaCyan','grass':'M_ArenaGrass','stripe':'M_ArenaStripe','lab':'M_LabDarkDeck','rock':'M_ArenaRock'}.items()}
report=[]
for arena in ['CoastalResearch','Outskirts','SubterraneanLab','Beachhead']:
 assert ls.load_level('/Game/TroubleSquad/Maps/L_'+arena)
 existing={a.get_actor_label():a for a in es.get_all_level_actors()};placed=[]
 def add(name,key,p,size,yaw=0,mat=None,collision=False):
  label='Reference polish | '+name;a=existing.get(label)
  if a is None:a=es.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p));a.set_actor_label(label)
  mesh=meshes[key];b=mesh.get_bounds();e=b.box_extent
  scale=unreal.Vector(size[0]/max(1,e.x*2),size[1]/max(1,e.y*2),size[2]/max(1,e.z*2))
  a.static_mesh_component.set_static_mesh(mesh);a.set_actor_scale3d(scale);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
  a.set_actor_location(unreal.Vector(p[0],p[1],p[2]-(b.origin.z-e.z)*scale.z),False,False)
  c=a.static_mesh_component;c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision or key=='building' else unreal.CollisionEnabled.NO_COLLISION)
  c.set_editor_property('cast_shadow',size[2]>30)
  if mat:c.set_material(0,mats[mat])
  placed.append(label);return a
 def line(name,a,b,width=8,mat='stripe',z=7):
  dx,dy=b[0]-a[0],b[1]-a[1];add(name,'box',((a[0]+b[0])/2,(a[1]+b[1])/2,z),(math.hypot(dx,dy),width,1),math.degrees(math.atan2(dy,dx)),mat)
 def ring(name,x,y,r,width=15,mat='stripe',z=7):
  for j in range(24):
   t=j*math.tau/24;u=(j+1)*math.tau/24;line(name+str(j),(x+r*math.cos(t),y+r*math.sin(t)),(x+r*math.cos(u),y+r*math.sin(u)),width,mat,z)
 def stock(name,x,y):
  add(name+' platform','box',(x,y,3),(460,340,8),0,'steel')
  for i,(dx,dy) in enumerate([(-110,-70),(100,-60),(-30,90)]):add(name+' crate'+str(i),'case',(x+dx,y+dy,12),(120,100,95),i*17)
  add(name+' beacon','light',(x+170,y+110,10),(50,50,100),180)
 if arena=='Outskirts':
  floor=existing.get('Arena floor');floor.static_mesh_component.set_material(0,mats['sand'])
  road=existing.get('Access road');road.set_actor_scale3d(unreal.Vector(90,12,.035));road.static_mesh_component.set_material(0,mats['road'])
  # A narrow diagonal service road, shore rocks, abandoned hangars and a southern staging pad.
  for sign in [-1,1]:
   for i in range(-10,11):
    x=i*410;y=-280+x*.208+sign*620;line('outpost edge%d_%d'%(sign,i),(x-140,y-29),(x+140,y+29),9,'stripe')
  for name,x,y in [('west cache',-900,-1200),('east cache',1100,1150),('cannon stores',-2800,-1850),('ruined checkpoint',2600,900)]:stock(name,x,y)
  add('staging pad','disc',(800,-2000,2),(1450,1450,5),0,'steel');ring('pad perimeter',800,-2000,650,18,'stripe',10);ring('pad inner',800,-2000,460,9,'cyan',11)
  for x,y in [(-1900,2100),(2600,2050),(3350,-1850)]:
   add('impact basin%d'%x,'disc',(x,y,1),(920,770,2),20,'rock');ring('crater rim%d'%x,x,y,420,32,'sand',6)
   for j in range(7):t=j*math.tau/7;add('crater debris%d_%d'%(x,j),'rock',(x+430*math.cos(t),y+350*math.sin(t),0),(90,130,65),j*35)
  for i in range(8):add('coastal hangar'+str(i),'building',(-4100+i%2*600,-2600+i//2*1450,0),(500,650,360),90)
 elif arena=='SubterraneanLab':
  existing['Arena floor'].static_mesh_component.set_material(0,mats['lab'])
  # Keep the continuous evacuation corridor to the east chamber clear.
  for x in range(-4000,4001,500):
   for y in [-3100,-850,850,3100]:
    line('deck seam%d_%d'%(x,y),(x,y),(x+340,y),5,'steel',3)
  for x,y in [(-2400,0),(-900,1350),(800,-1300),(2100,1250),(-650,-650),(700,650)]:
   add('machine island%d'%x,'disc',(x,y,3),(580,580,10),0,'steel');ring('island light%d'%x,x,y,275,11,'cyan',15)
   add('specimen tank%d'%x,'tank',(x,y,15),(220,220,240),0)
   for j in range(3):add('island console%d_%d'%(x,j),'case',(x+240*math.cos(j*math.tau/3),y+240*math.sin(j*math.tau/3),12),(85,100,80),j*120)
  for side in [-1,1]:
   for i in range(8):
    x=-3500+i*900;y=side*2350
    add('wall equipment%d_%d'%(side,i),'tank',(x,y,0),(180,150,150),90)
    add('coolant trunk%d_%d'%(side,i),'pipe',(x,y+side*300,25),(780,60,65),0)
  for x in [-2900,0,2900]:
   for y in [-2000,2000]:
    for sign in [-1,1]:line('bridge cyan%d_%d_%d'%(x,y,sign),(x-360,y+sign*260),(x+360,y+sign*260),12,'cyan',16)
  ring('chamber danger perimeter',3350,2350,850,18,'stripe',12)
 elif arena=='Beachhead':
  existing['Urban landing pad'].static_mesh_component.set_material(0,mats['road']);existing['Beach strip'].static_mesh_component.set_material(0,mats['sand'])
  # City blocks contrast with the open eastern dunes; the middle passage and two boss arenas remain open.
  for x,y in [(-3150,-1500),(-3000,1500),(-1100,-2100),(-1200,2100)]:
   add('city block%d_%d'%(x,y),'box',(x,y,3),(900,700,10),0,'steel')
   add('city structure%d_%d'%(x,y),'building',(x,y,13),(440,450,320),0)
   for sign in [-1,1]:line('block curb%d_%d_%d'%(x,y,sign),(x-450,y+sign*370),(x+450,y+sign*370),15,'stripe',15)
  for x in [-2150,-450]:
   for y in range(-2700,2701,450):line('urban lane%d_%d'%(x,y),(x,y),(x,y+200),10,'stripe',7)
  stock('forward base',-650,800);stock('beach supply',1300,-850)
  add('abandoned transport','car',(-2200,550,7),(370,190,130),90)
  for i in range(15):
   y=-3400+i*480;x=700+90*math.sin(i*.8)
   add('coastal grass'+str(i),'bush',(x,y,2),(260,220,65),i*39)
   line('sea foam'+str(i),(4500,y-120),(4550,y+170),18,'cyan',2)
  for x,y in [(2100,1900),(3100,-1300),(3700,1100)]:
   add('dune basin%d'%x,'disc',(x,y,1),(800,650,2),15,'rock');ring('dune rim%d'%x,x,y,360,25,'sand',5)
 else:
  # Existing city geometry is retained; reinforce districts rather than replacing its central plaza.
  for name,x,y in [('north logistics',2200,1800),('south checkpoint',-1700,-2200),('east supply',2000,-1600)]:stock(name,x,y)
  for i in range(6):add('quay greenery'+str(i),'bush',(-2900+i*450,-2850,5),(180,180,70),i*35)
 assert ls.save_current_level();report.append({'arena':arena,'added_or_updated':len(placed)})
 unreal.log('REFERENCE_ARENA_UPDATED '+arena+' placements='+str(len(placed)))
with open('D:/UnReal/TroubleSquad/Saved/Logs/ReferenceArenaPolish.json','w') as f:json.dump(report,f)
