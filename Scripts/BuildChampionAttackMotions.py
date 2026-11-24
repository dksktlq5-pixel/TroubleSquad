"""Authored per-champion key poses. Cosmetic only: no gameplay/root displacement."""
import bpy,math,pathlib,json
from mathutils import Euler
ROOT=pathlib.Path('D:/UnReal/TroubleSquad/ArtSource/AnimaSquad')
# xyz local joint rotations in degrees: anticipation, strike and settle are distinct poses.
def pose(chest=(0,0,0),rs=(0,0,0),ls=(0,0,0),re=(0,0,0),le=(0,0,0),rh=(0,0,0),lh=(0,0,0)):
 return {'Spine2':chest,'R_Shoulder':rs,'L_Shoulder':ls,'R_Elbow':re,'L_Elbow':le,'R_Hand':rh,'L_Hand':lh}
def clip(name,length,wind,strike,settle):return {'identity':name,'duration':length,'anticipation':wind,'strike':strike,'settle':settle}
P={
'Jinx':[
 clip('braced minigun recoil',.24,pose((0,-3,0),(0,0,-4)),pose((0,7,-3),(5,0,7),(-3,0,-4),(8,0,0),(5,0,0)),pose((0,-2,1))),
 clip('five rocket shoulder volley',.42,pose((0,-7,4),(-12,0,-12),(0,0,10)),pose((0,11,-5),(16,0,12),(-4,0,-10),(15,0,0),(9,0,0)),pose((0,2,0))),
 clip('heavy rocket launch and recovery',.68,pose((0,-12,6),(-23,0,-13),(10,0,15)),pose((0,17,-8),(22,0,18),(-9,0,-14),(22,0,0),(15,0,0)),pose((0,4,1)))],
'Seraphine':[
 clip('right palm sound wave',.38,pose((0,-3,-9),(0,0,-18)),pose((0,4,12),(12,0,35),(-4,0,-8),(-12,0,0),(5,0,0),(0,25,0)),pose((0,0,3),(0,0,7))),
 clip('open arm shield cadence',.52,pose((0,-5,0),(8,0,-14),(-8,0,14)),pose((0,0,0),(-12,0,34),(12,0,-34),(-15,0,0),(-15,0,0),(0,25,0),(0,-25,0)),pose((0,0,0),(0,0,12),(0,0,-12))),
 clip('two hand protective crescendo',.84,pose((0,6,0),(15,0,-18),(-15,0,18)),pose((0,-10,0),(-42,0,48),(42,0,-48),(-22,0,0),(-22,0,0),(0,30,0),(0,-30,0)),pose((0,-2,0),(0,0,18),(0,0,-18)))],
'Leona':[
 clip('shield forward sword sweep',.40,pose((0,-4,-14),(0,0,-28),(8,0,-12)),pose((0,8,20),(12,0,42),(-12,0,-26),(15,0,0),(-15,0,0)),pose((0,2,5),(0,0,8),(0,0,-8))),
 clip('shield brace and release',.48,pose((0,8,0),(5,0,-8),(16,0,20),(8,0,0),(24,0,0)),pose((0,-5,0),(0,0,12),(-20,0,-38),(0,0,0),(-18,0,0)),pose((0,0,0),(0,0,0),(0,0,-12))),
 clip('solar sword raised then struck',.78,pose((0,-10,-8),(-52,0,-12),(8,0,8),(-20,0,0)),pose((0,15,12),(26,0,38),(-10,0,-24),(12,0,0)),pose((0,2,3),(0,0,10),(0,0,-6)))],
'Illaoi':[
 clip('heavy idol hammer',.56,pose((0,-13,-15),(-28,0,-22),(12,0,12),(-18,0,0)),pose((0,18,20),(32,0,35),(-18,0,-25),(18,0,0),(10,0,0)),pose((0,4,6),(6,0,8))),
 clip('forward tentacle command',.54,pose((0,-8,-12),(0,0,-34),(0,0,8)),pose((0,16,12),(20,0,44),(-10,0,-16),(-12,0,0)),pose((0,3,4),(0,0,10))),
 clip('two arm idol ground slam',.92,pose((0,-20,0),(-48,0,-28),(48,0,28),(-26,0,0),(-26,0,0)),pose((0,28,0),(40,0,30),(-40,0,-30),(22,0,0),(22,0,0)),pose((0,8,0),(8,0,8),(-8,0,-8)))],
'Briar':[
 clip('cross body claw rake',.28,pose((0,9,-18),(8,0,-30),(-8,0,20)),pose((0,19,24),(18,0,48),(-18,0,-36),(18,0,0),(-12,0,0)),pose((0,7,4))),
 clip('frenzy shackles opened',.42,pose((0,14,0),(18,0,-20),(-18,0,20),(20,0,0),(20,0,0)),pose((0,-10,0),(-12,0,42),(12,0,-42),(-15,0,0),(-15,0,0)),pose((0,10,0),(0,0,12),(0,0,-12))),
 clip('predatory leap landing brace',.76,pose((0,24,0),(20,0,-30),(-20,0,30),(30,0,0),(30,0,0)),pose((0,-16,0),(-35,0,45),(35,0,-45),(-20,0,0),(-20,0,0)),pose((0,20,0),(20,0,22),(-20,0,-22)))],
'Yasuo':[
 clip('draw cut wind thrust',.34,pose((0,-4,-20),(0,0,-38),(0,0,12)),pose((0,7,27),(10,0,55),(-8,0,-15),(-16,0,0),(4,0,0),(0,0,16)),pose((0,0,7),(0,0,12))),
 clip('low dash sword guard',.24,pose((0,12,-8),(0,0,-14)),pose((0,19,12),(14,0,32),(-12,0,-16),(10,0,0)),pose((0,5,3))),
 clip('sword wind curtain sweep',.58,pose((0,-8,-28),(-26,0,-35),(0,0,16)),pose((0,7,32),(12,0,62),(-10,0,-22),(-12,0,0)),pose((0,0,8),(0,0,14)))],
'Riven':[
 clip('diagonal runic cleave',.30,pose((0,-9,-23),(-20,0,-30),(10,0,10)),pose((0,14,30),(24,0,56),(-15,0,-30),(15,0,0)),pose((0,2,8),(0,0,12))),
 clip('forward shielded shoulder dash',.28,pose((0,10,-6),(10,0,-12),(-8,0,14)),pose((0,22,12),(18,0,25),(-18,0,-22),(14,0,0),(8,0,0)),pose((0,5,0))),
 clip('raised blade runic empowerment',.72,pose((0,10,-12),(10,0,-24),(-10,0,12)),pose((0,-16,12),(-50,0,44),(24,0,-20),(-24,0,0)),pose((0,-4,3),(-15,0,20),(8,0,-8)))],
'Aurora':[
 clip('wand wrist bolt flick',.32,pose((0,-2,-10),(0,0,-20),(0,0,8),(15,0,0)),pose((0,4,12),(12,0,32),(-5,0,-10),(-15,0,0),(0,0,0),(20,0,22)),pose((0,0,2),(0,0,5))),
 clip('spirit veil side lean',.40,pose((0,9,-12),(12,0,-15),(-12,0,15)),pose((0,-9,18),(-24,0,30),(24,0,-30),(-10,0,0),(-10,0,0)),pose((0,0,5))),
 clip('realm portal opening gesture',.88,pose((0,8,0),(14,0,-26),(-14,0,26),(24,0,0),(24,0,0)),pose((0,-14,0),(-34,0,50),(34,0,-50),(-18,0,0),(-18,0,0),(0,32,0),(0,-32,0)),pose((0,-3,0),(0,0,20),(0,0,-20)))],
'Xayah':[
 clip('feather fan wrist throw',.30,pose((0,-4,-20),(0,0,-35),(0,0,10),(18,0,0)),pose((0,6,24),(8,0,52),(-7,0,-16),(-20,0,0),(0,0,0),(0,0,28)),pose((0,0,6),(0,0,10))),
 clip('both hands feather recall',.44,pose((0,9,0),(18,0,30),(-18,0,-30),(-20,0,0),(-20,0,0)),pose((0,-12,0),(-18,0,-28),(18,0,28),(32,0,0),(32,0,0)),pose((0,0,0),(0,0,-5),(0,0,5))),
 clip('airborne wing spread storm',.82,pose((0,14,0),(20,0,-22),(-20,0,22),(20,0,0),(20,0,0)),pose((0,-18,0),(-30,0,64),(30,0,-64),(-18,0,0),(-18,0,0),(0,20,15),(0,-20,-15)),pose((0,-4,0),(0,0,24),(0,0,-24))) ]}
report=[]
for champion,clips in P.items():
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(ROOT/champion/f'AN_{champion}_Idle.fbx'));a=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');bpy.context.scene.frame_set(1);base={b.name:b.matrix_basis.copy() for b in a.pose.bones};a.animation_data_clear()
 for slot,c in zip(['Weapon','E','R'],clips):
  end=round(c['duration']*60)+1;a.animation_data_create();a.animation_data.action=bpy.data.actions.new(f'AN_{champion}_{slot}');bpy.context.scene.render.fps=60;bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=end
  keys=[(1,{}),(max(2,round(end*.13)),c['anticipation']),(max(3,round(end*.28)),c['strike']),(round(end*.62),c['settle']),(end,{})]
  for frame,key in keys:
   for b in a.pose.bones:
    b.matrix_basis=base[b.name];b.rotation_mode='QUATERNION'
    angles=key.get(b.name,(0,0,0))
    if angles!=(0,0,0):b.rotation_quaternion=b.rotation_quaternion@Euler(tuple(math.radians(v) for v in angles),'XYZ').to_quaternion()
    b.keyframe_insert(data_path='rotation_quaternion',frame=frame);b.keyframe_insert(data_path='location',frame=frame);b.keyframe_insert(data_path='scale',frame=frame)
  bpy.ops.object.select_all(action='DESELECT');a.select_set(True);bpy.context.view_layer.objects.active=a
  bpy.ops.export_scene.fbx(filepath=str(ROOT/champion/f'AN_{champion}_{slot}.fbx'),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z')
  report.append({'champion':champion,'slot':slot,**c,'frames':end,'fps':60,'strike_fraction':.28});a.animation_data_clear()
(ROOT/'ChampionMotionProfiles.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('DISTINCT_CHAMPION_MOTIONS_READY',len(report))
