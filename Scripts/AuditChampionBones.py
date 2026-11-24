import bpy,json
bpy.ops.wm.open_mainfile(filepath='D:/UnReal/TroubleSquad/ArtSource/AnimaSquad/Jinx/Jinx.blend')
a=next(o for o in bpy.data.objects if o.type=='ARMATURE')
open('D:/UnReal/TroubleSquad/Saved/Logs/ChampionBones.json','w').write(json.dumps([b.name for b in a.pose.bones]))
