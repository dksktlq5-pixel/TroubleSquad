import bpy,pathlib,json
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/AnimaSquad');r={}
for n in ['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/n/f'AN_{n}_Idle.fbx'));a=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');r[n]=[b.name for b in a.pose.bones if any(k in b.name.lower() for k in ['shoulder','elbow','upperarm','forearm','spine','chest','hip','knee','sword','blade','shield','weapon','clavicle'])]
pathlib.Path('D:/UnReal/TroubleSquad/Saved/Logs/ChampionMotionRigAudit.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
