import bpy,json,pathlib
r={}
for n in ['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']:
 try:bpy.ops.wm.open_mainfile(filepath=f'D:/UnReal/TroubleSquad/ArtSource/AnimaSquad/{n}/{n}.blend')
 except:continue
 r[n]=[a.name for a in bpy.data.actions]
open('D:/UnReal/TroubleSquad/Saved/Logs/ChampionActions.json','w').write(json.dumps(r))
