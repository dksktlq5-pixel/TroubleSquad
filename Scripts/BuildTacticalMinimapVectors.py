import json,math,pathlib,html
root=pathlib.Path('ArtSource/TacticalMinimaps');root.mkdir(exist_ok=True)
SIZE=1024;WIDTH=106.;BKG='#111115';ops=[]
def uv(p):return [(p[0]/WIDTH+.5)*SIZE,(p[1]/WIDTH+.5)*SIZE]
def poly(pts,color,stroke=None,sw=0):ops.append({'kind':'polygon','points':[uv(p) for p in pts],'color':color,'stroke':stroke,'width':sw*SIZE/WIDTH})
def line(pts,color,width):ops.append({'kind':'line','points':[uv(p) for p in pts],'color':color,'width':width*SIZE/WIDTH})
def ellipse(x,y,rx,ry,color):
 p=uv((x-rx,y-ry));ops.append({'kind':'ellipse','rect':[p[0],p[1],2*rx*SIZE/WIDTH,2*ry*SIZE/WIDTH],'color':color})
def rect(x,y,w,d,color,yaw=0):
 a=math.radians(yaw);poly([(x+math.cos(a)*xx-math.sin(a)*yy,y+math.sin(a)*xx+math.cos(a)*yy) for xx,yy in [(-w/2,-d/2),(w/2,-d/2),(w/2,d/2),(-w/2,d/2)]],color)
def save(name):
 data={'name':name,'size':SIZE,'world_width_cm':10600,'world_center_cm':[0,0],'north':'-Y','background':BKG,'operations':list(ops)};(root/(name+'.json')).write_text(json.dumps(data,indent=2),encoding='utf-8')
 svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{SIZE}" height="{SIZE}" viewBox="0 0 {SIZE} {SIZE}"><rect width="1024" height="1024" fill="{BKG}"/>']
 for o in ops:
  if o['kind']=='ellipse':
   x,y,w,h=o['rect'];svg.append(f'<ellipse cx="{x+w/2}" cy="{y+h/2}" rx="{w/2}" ry="{h/2}" fill="{o["color"]}"/>')
  else:
   pts=' '.join(f'{p[0]:.3f},{p[1]:.3f}' for p in o['points'])
   if o['kind']=='line':svg.append(f'<polyline points="{pts}" fill="none" stroke="{o["color"]}" stroke-width="{o["width"]}" stroke-linejoin="round"/>')
   else:svg.append(f'<polygon points="{pts}" fill="{o["color"]}" stroke="{o.get("stroke") or o["color"]}" stroke-width="{o.get("width",0)}"/>')
 svg.append('</svg>');(root/(name+'.svg')).write_text('\n'.join(svg),encoding='utf-8')
# Coastal district: lavender walkable ground, dark building silhouettes and southwest water.
ops=[];poly([(-100,24),(-48,24),(-32,24),(-7,48),(39,48),(49,39),(100,100),(-100,100)],'#6cabb5');coast=[(-48,-44),(29,-44),(42,-29),(49,11),(49,40),(40,49),(-8,49),(-32,25),(-48,25)];poly(coast,'#a69aba','#bcb2c9',.25)
line([(-17,-37),(-17,-6),(-27,21)],'#bcb0cb',3);line([(-32,24),(0,0),(35,-23)],'#bcb0cb',3.5);line([(0,0),(30,32),(39,41)],'#bcb0cb',3.5);ellipse(0,0,8,8,'#c4b7cf');ellipse(0,0,6.6,6.6,'#a69aba')
layout=json.load(open('ArtSource/CoastalRebuild/Layout.json',encoding='utf-8'));dims={}
for folder in ['ReferenceBuildings','ReferenceIndustrialModules']:
 for b in json.load(open('ArtSource/'+folder+'/BuildingManifest.json',encoding='utf-8'))['buildings']:dims[b['id']]=b['dimensions_m']
for row in layout['placements']:
 if int(row['id'][:2])>=15 and row['id'].startswith(('15_','19_','24_')):continue
 x,y,z=row['position_m'];w,d,h=dims[row['id']];rect(x,y,w*row['scale'],d*row['scale'],BKG,row['yaw_deg'])
save('Coastal')
# Outskirts: the same coordinate data used to place the new physical map.
ops=[];poly([(-100,-100),(100,-100),(30,-49),(-16,-49),(-31,-37),(-41,-21),(-49,23),(-100,50)],'#65a6b4');boundary=[(-49,23),(-47,0),(-41,-21),(-31,-37),(-16,-49),(30,-49),(47,-40),(49,-14),(48,43),(34,49),(-40,49),(-49,36)];poly(boundary,'#b59b6d','#ceb586',.2)
road=[(-37,40),(-37,22),(-33,7),(-25,-9),(-14,-22),(2,-31),(20,-34),(38,-32)];line(road,'#88785b',8.8)
base=[(-27,17),(-3,17),(3,23),(3,31),(-3,36),(-27,36)];poly(base,'#b09a73','#d4bd8d',.4);line([(-21,21),(-6,21),(-3,24),(-3,29),(-6,32),(-21,32),(-21,21)],'#d2bb89',.4)
layout=json.load(open('ArtSource/OutskirtsRebuild/Layout.json',encoding='utf-8'));dims={b['id']:b['dimensions_m'] for b in json.load(open('ArtSource/OutskirtsRebuild/Models/BuildingManifest.json',encoding='utf-8'))['buildings']}
for row in layout['placements']:
 x,y,z=row['position_m'];w,d,h=dims[row['id']];sc=row['scale'];id=int(row['id'][:2])
 if id==9:
  ellipse(x,y,w*sc*.5,d*sc*.5,'#746143');ellipse(x,y,w*sc*.37,d*sc*.37,'#b59b6d')
 elif id in [10,11]:
  a=math.radians(row['yaw_deg']);poly([(x+math.cos(a)*xx-math.sin(a)*yy,y+math.sin(a)*xx+math.cos(a)*yy) for xx,yy in [(-w*.5,0),(-w*.3,-d*.47),(w*.18,-d*.5),(w*.5,-d*.08),(w*.33,d*.45),(-w*.3,d*.4)]],BKG)
 elif id not in [5,13,14,16]:rect(x,y,w*sc,d*sc,BKG,row['yaw_deg'])
# Relay's C silhouette remains legible while dynamic markers are drawn in game.
ellipse(34,34,2.8,2.8,BKG);ellipse(34,34,1.8,1.8,'#b59b6d');poly([(34,34),(37,34),(37,37),(34,37)],'#b59b6d')
save('Outskirts')
ops=[];layout=json.load(open('ArtSource/LabRebuild/Layout.json',encoding='utf-8'));poly(layout['main_floor'],'#99a6c5','#b0bbd0',.2)
x,y=layout['boss_floor_center'];r=layout['boss_floor_radius'];ellipse(x,y,r,r,'#b4d5df');ellipse(x,y,r-.6,r-.6,'#a6c4d3');poly(layout['entry_floor'],'#99a6c5')
for c in layout['channels']:
 x0,x1,y0,y1=c['x_min'],c['x_max'],c['y_min'],c['y_max'];rect((x0+x1)/2,(y0+y1)/2,x1-x0,y1-y0,'#10212c');line([(x0,y0),(x1,y0)],'#73a2bd',.20);line([(x0,y1),(x1,y1)],'#73a2bd',.20)
 for x in layout['bridge_x']:rect(x,(y0+y1)/2,5.0,6.2,'#adbcd0')
dims={b['id']:b['dimensions_m'] for b in json.load(open('ArtSource/LabRebuild/Models/BuildingManifest.json',encoding='utf-8'))['buildings']}
for row in layout['placements']:
 if int(row['id'][:2]) in [7,8,13,14]:continue
 x,y,z=row['position_m'];w,d,h=dims[row['id']];rect(x,y,w*row['scale'],d*row['scale'],BKG,row['yaw_deg'])
rect(35.5,0,.9,5.3,'#70d5e4');save('Lab')
ops=[];poly([(45,-48),(100,-48),(100,55),(39,55),(39,35),(44,11)],'#73b1b6');beach=[(-48,-40),(-25,-40),(-17,-31),(-9,-34),(2,-23),(12,-31),(25,-32),(33,-28),(46,-28),(45,-5),(40,17),(42,40),(30,46),(13,42),(2,36),(-14,41),(-35,39),(-47,23)];poly(beach,'#9bb77f');poly([(-48,-40),(-25,-40),(-17,-31),(-9,-34),(2,-23),(2,34),(-15,41),(-35,39),(-47,23)],'#83aaa8')
for x,y,w,d in [(-36,-23,8,17),(-24,-23,6,14),(-37,9,9,13),(-26,9,8,14),(-7,-8,10,4),(-1,14,13,4),(-11,23,9,4)]:rect(x,y,w,d,BKG)
for x,y,r in [(24,-18,9),(26,20,8),(18,38,7)]:
 ellipse(x,y,r,r,BKG);ellipse(x-2,y+1,r*.75,r*.75,'#9bb77f');rect(x-7,y+6,r*1.1,r*.8,'#9bb77f',-20)
line([(-42,-28),(-42,-35),(-32,-35),(-23,-27),(-23,-18)],'#b0d3cb',1.3);line([(-40,23),(-32,31),(-20,31),(-16,22)],'#b0d3cb',1.3);save('Beachhead')
print('TACTICAL_VECTOR_MINIMAPS_READY 4')
