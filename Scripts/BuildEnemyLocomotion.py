"""Create upright, skinned locomotion meshes from existing enemy geometry; preserves UVs."""
import bpy,math,pathlib
from mathutils import Vector
root=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/EnemyMotion')
bipeds={'Ghoul','Fuemigo','Red','Tibbers','Daisy','Shield','Lancer','Crusher','Briar','Aatrox','Spawner'}
flyers={'Pix','Beetle','Belveth','Seahorse','Hunter','Fast'}
for src in sorted((root/'Original').glob('*.obj')):
 name=src.stem;bpy.ops.wm.read_factory_settings(use_empty=True)
 vertices=[];uvs=[];faces=[];face_uv=[]
 for line in src.read_text().splitlines():
  fields=line.split()
  if not fields:continue
  if fields[0]=='v':
   x,y,z=map(float,fields[1:4]);vertices.append((x/100,z/100,y/100))
  elif fields[0]=='vt':uvs.append(tuple(map(float,fields[1:3])))
  elif fields[0]=='f':
   corners=[p.split('/') for p in fields[1:]]
   # Swap Y/Z to make an upright model and reverse the reflected winding.
   corners.reverse();faces.append([int(p[0])-1 for p in corners]);face_uv.append([int(p[1])-1 for p in corners])
 data=bpy.data.meshes.new('Geometry_'+name);data.from_pydata(vertices,[],faces);data.update()
 mesh=bpy.data.objects.new('Body_'+name,data);bpy.context.collection.objects.link(mesh)
 layer=data.uv_layers.new(name='UVMap')
 for poly,indices in zip(data.polygons,face_uv):
  poly.use_smooth=True
  for loop_index,uv_index in zip(poly.loop_indices,indices):layer.data[loop_index].uv=uvs[uv_index]
 mat=bpy.data.materials.new('M_'+name);mesh.data.materials.append(mat)
 low=min(v.co.z for v in mesh.data.vertices)
 for v in mesh.data.vertices:v.co.z-=low
 h=max(v.co.z for v in mesh.data.vertices);w=max(abs(v.co.y) for v in mesh.data.vertices);l=max(abs(v.co.x) for v in mesh.data.vertices)
 arm=bpy.data.armatures.new('Rig_'+name);rig=bpy.data.objects.new('Rig_'+name,arm);bpy.context.collection.objects.link(rig);bpy.context.view_layer.objects.active=rig;rig.select_set(True);mesh.select_set(False);bpy.ops.object.mode_set(mode='EDIT')
 bone=arm.edit_bones.new('Root');bone.head=(0,0,0);bone.tail=(0,0,max(.1,h*.2))
 bone=arm.edit_bones.new('Torso');bone.head=(0,0,h*.35);bone.tail=(0,0,h*.75);bone.parent=arm.edit_bones['Root']
 legs=[]
 for side in [-1,1]:
  for fore in ([0] if name in bipeds else [-1,1]):
   n='Limb_'+str(side)+'_'+str(fore);b=arm.edit_bones.new(n);b.head=(l*.4*fore,w*.42*side,h*.38);b.tail=(l*.4*fore,w*.52*side,h*.04);b.parent=arm.edit_bones['Root'];legs.append((n,side,fore))
 bpy.ops.object.mode_set(mode='OBJECT')
 for n in ['Root','Torso']+[x[0] for x in legs]:mesh.vertex_groups.new(name=n)
 for v in mesh.data.vertices:
  if name in flyers:
   mask=min(1,max(0,(abs(v.co.y)/max(w,.01)-.2)/.65))*.8
  else:mask=min(1,max(0,(.55-v.co.z/max(h,.01))/.3))
  mesh.vertex_groups['Torso'].add([v.index],1-mask,'REPLACE')
  n,_,_=min(legs,key=lambda x:(v.co.y-w*.42*x[1])**2+(v.co.x-l*.4*x[2])**2)
  if mask>0:mesh.vertex_groups[n].add([v.index],mask,'REPLACE')
 mod=mesh.modifiers.new('LocomotionSkin','ARMATURE');mod.object=rig;mesh.parent=rig
 rig.animation_data_create();action=bpy.data.actions.new('AN_'+name+'_Walk');rig.animation_data.action=action
 for frame in range(1,26):
  phase=(frame-1)/24*math.tau
  for n,side,fore in legs:
   b=rig.pose.bones[n];b.rotation_mode='XYZ';b.rotation_euler=(0,0,math.sin(phase+(0 if side*(fore or 1)>0 else math.pi))*(.22 if name in flyers else .38));b.keyframe_insert('rotation_euler',frame=frame)
  b=rig.pose.bones['Root'];b.location.z=abs(math.sin(phase))*h*.025;b.keyframe_insert('location',frame=frame)
 bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=25;bpy.context.scene.render.fps=24;bpy.context.scene.frame_set(1)
 bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
 bpy.ops.export_scene.fbx(filepath=str(root/('SK_'+name+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z')
 print('ENEMY_RIGGED',name,len(mesh.data.vertices))
