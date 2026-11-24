import bpy,math,json
from pathlib import Path
root=Path('D:/UnReal/TroubleSquad/ArtSource/EnemyMotion');out=root/'Attacks';out.mkdir(exist_ok=True)
bosses={'Reksai','Briar','Belveth','Aatrox'};flyers={'Pix','Beetle','Belveth','Seahorse','Hunter','Fast'};ranged={'Shark','Artillery','Lancer','Shy','Pix'};heavy={'Crusher','Tibbers','Daisy','Shield','Red'};rows=[]
def curve(t):
 knots=[(0,0),(.40,-.65),(.60,1),(.75,.25),(1,0)]
 for (a,x),(b,y) in zip(knots,knots[1:]):
  if a<=t<=b:u=(t-a)/(b-a);u=u*u*(3-2*u);return x+(y-x)*u
 return 0
for src in sorted(root.glob('SK_*.fbx')):
 name=src.stem[3:];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(src));rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');rig.animation_data_clear();action=bpy.data.actions.new('AN_'+name+'_Attack');rig.animation_data_create();rig.animation_data.action=action
 height=max((o.dimensions.z for o in bpy.context.scene.objects if o.type=='MESH'),default=1);duration=1.3 if name in bosses else 1.05 if name in heavy or name=='Seahorse' else .85;frames=round(duration*30)+1
 for frame in range(1,frames+1):
  t=(frame-1)/(frames-1);v=curve(t)
  for b in rig.pose.bones:
   b.rotation_mode='XYZ';b.rotation_euler=(0,0,0);b.location=(0,0,0)
   if b.name=='Torso':
    b.rotation_euler=(.08*v,.40*v if name not in ranged else -.18*v,.28*v if name in bosses or name in heavy else .12*v)
    if name in flyers:b.rotation_euler=(.20*v,.30*v,.07*v)
    if name=='Aatrox':b.rotation_euler=(.10*v,.60*v,.65*v)
    elif name=='Briar':b.rotation_euler=(.12*v,.48*v,.60*v)
    elif name=='Reksai':b.rotation_euler=(.06*v,.65*v,.16*v)
    elif name=='Belveth':b.rotation_euler=(.40*v,.30*v,.28*v)
    elif name=='Seahorse':b.rotation_euler=(.12*v,.48*v,.04*v)
   elif b.name=='Root':b.location.z=max(0,-v)*height*(.06 if name in bosses else .025);b.location.x=max(0,v)*height*.035
   elif b.name.startswith('Limb_'):
    side=-1 if '_-1_' in b.name else 1;b.rotation_euler=(side*.20*v,.20*v,side*(.55 if name in flyers else .24)*v)
   b.keyframe_insert('rotation_euler',frame=frame);b.keyframe_insert('location',frame=frame)
 bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=frames;bpy.context.scene.render.fps=30;bpy.context.scene.frame_set(1)
 bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
 bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'_Attack.blend')))
 bpy.ops.export_scene.fbx(filepath=str(out/('AN_'+name+'_Attack.fbx')),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z')
 rows.append({'name':name,'duration':duration,'impact_phase':.6,'bones':len(rig.pose.bones),'profile':'boss' if name in bosses else 'flyer' if name in flyers else 'ranged' if name in ranged else 'heavy' if name in heavy else 'melee'});print('ATTACK_EXPORTED',name,flush=True)
(out/'Manifest.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
