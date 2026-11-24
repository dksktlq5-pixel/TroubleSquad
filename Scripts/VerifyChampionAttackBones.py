import bpy,pathlib,json
r=[]
for p in pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/AnimaSquad').glob('*/AN_*.fbx'):
 if not any(p.stem.endswith('_'+s) for s in ['Weapon','E','R']):continue
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(p));a=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');bpy.context.scene.frame_set(1);base={b.name:b.matrix.copy() for b in a.pose.bones};act=a.animation_data.action;bpy.context.scene.frame_set(round(act.frame_range[1]*.45));changed=sum((base[b.name].to_quaternion().rotation_difference(b.matrix.to_quaternion()).angle>.01) for b in a.pose.bones);r.append({'name':p.stem,'moving_bones':changed})
open('D:/UnReal/TroubleSquad/Saved/Logs/AttackMotionBoneVerification.json','w').write(json.dumps(r,indent=2))
assert len(r)==27 and all(x['moving_bones']>0 for x in r)
