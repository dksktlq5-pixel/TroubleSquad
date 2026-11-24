from PIL import Image
from pathlib import Path
import json,sys
MAP=sys.argv[1] if len(sys.argv)>1 else "L_Outskirts_Rebuilt"
DEST="LabRebuild" if MAP=="L_SubterraneanLab_Rebuilt" else "OutskirtsRebuild"
rows=[]
for name,ratio in [('Health69',1111/1610),('Health20',.2),('Health80',.8)]:
 im=Image.open('Saved/HealthMinimapReview/'+MAP+'/'+name+'.png').convert('RGB');w,h=im.size;px=im.load();x0=int(w/2-210+17);y=int(h-124+75)
 green=lambda c:c[1]>75 and c[1]>c[0]*1.6 and c[1]>c[2]*1.5
 filled=[x for x in range(x0,x0+386) if green(px[x,y])];hud=max(filled)-x0+1 if filled else 0
 spans=[]
 for yy in range(int(h*.40),int(h*.48)):
  hits=[x for x in range(int(w/2-80),int(w/2+80)) if green(px[x,yy])]
  if hits:spans.append(max(hits)-min(hits)+1)
 overhead=max(spans or [0]);expected_hud=386*ratio;expected_overhead=90*ratio
 assert abs(hud-expected_hud)<4,(name,hud,expected_hud)
 assert abs(overhead-expected_overhead)<4,(name,overhead,expected_overhead)
 rows.append({'case':name,'fraction':ratio,'hud_width_px':hud,'expected_hud_px':expected_hud,'overhead_width_px':overhead,'expected_overhead_px':expected_overhead,'passed':True})
Path('ArtSource/'+DEST+'/HealthBarPixelVerification.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print('PASS rendered HUD + overhead health: 69%, 20%, 80%')
