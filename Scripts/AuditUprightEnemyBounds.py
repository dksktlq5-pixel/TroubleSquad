import unreal,json
out={}
for n in ['Swarmer','Pix','Ghoul','Fuemigo','Red','Fast','Lancer','Shark','Briar']:
 mesh=unreal.load_asset('/Game/TroubleSquad/Enemies/Locomotion/'+n+'/SK_'+n)
 b=mesh.get_bounds();out[n]={'origin':[b.origin.x,b.origin.y,b.origin.z],'extent':[b.box_extent.x,b.box_extent.y,b.box_extent.z]}
with open('D:/UnReal/TroubleSquad/Saved/Logs/UprightEnemyBounds.json','w') as f:json.dump(out,f)
