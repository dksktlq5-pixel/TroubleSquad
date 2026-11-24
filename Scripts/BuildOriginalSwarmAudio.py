"""Bake named Swarm SFX events from original Wwise banks. No mixed video audio or VO banks."""
from pathlib import Path
import subprocess,json,re,wave,hashlib,shutil
import numpy as np
from scipy.io import wavfile
ROOT=Path('Saved/OriginalAudio');TX=ROOT/'TXTPNativeMix';OUT=Path('ArtSource/AudioArcade');META=Path('ArtSource/OriginalSwarmAudio');META.mkdir(exist_ok=True)
index=json.loads((ROOT/'OriginalEvents.json').read_text());alltxt=sorted(TX.glob('*.txtp'));rows=[];counts={};cache={}
def event(short):
 if short is None:return None
 if short.startswith('Play_'):return short
 return 'Play_sfx_'+short

def files(name):
 result=[p for p in alltxt if p.name.startswith(name) and p.name[len(name):len(name)+1] in [' ','~','.'] and '~{' not in p.name]
 return sorted(result,key=lambda p: ('{r' not in p.name,p.name))
def save(asset,name,variants=3,chosen=None):
 paths=chosen if chosen is not None else files(name) if name else []
 if name and not paths:raise RuntimeError('No named TXTP '+name)
 paths=paths[:variants] if paths else [None];counts[asset]=len(paths)
 for i,txtp in enumerate(paths):
  dest=OUT/(asset+('' if i==0 else '_V'+str(i))+'.wav');dest.parent.mkdir(parents=True,exist_ok=True)
  if txtp:
   decoded=ROOT/'Decoded'/(hashlib.sha1((str(txtp)+txtp.read_text()+'float_v1').encode()).hexdigest()+'.wav');decoded.parent.mkdir(exist_ok=True)
   if not decoded.exists():
    run=subprocess.run(['Saved/Tools/audio/vgmstream/vgmstream-cli.exe','-i','-W','4','-o',str(decoded),str(txtp)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if run.returncode:raise RuntimeError(run.stdout.decode(errors='replace'))
   rate,a=wavfile.read(decoded);a=a.astype(np.float64)
   if np.max(np.abs(a))>100:a/=32768.
   # Retain the original event mix in float until the final safe PCM conversion.
   peak=float(np.max(np.abs(a)));assert peak>.0001,'Unexpected silent original event '+str(name);gain=min(1.,.80/max(.001,peak));a*=gain
   fade=min(int(rate*(.02 if asset.startswith('Music/') else .004)),len(a)//8)
   ramp=np.linspace(0,1,fade).reshape((-1,1)) if a.ndim==2 else np.linspace(0,1,fade)
   a[:fade]*=ramp;a[-fade:]*=ramp[::-1]
   wavfile.write(dest,rate,(a*32767).astype('<i2'))
   source_ids=[int(x) for x in re.findall(r'##(\d+)\.wem',txtp.read_text())]
  else:
   with wave.open(str(dest),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(np.zeros(4410,dtype='<i2').tobytes())
   source_ids=[]
  with wave.open(str(dest)) as w:duration=w.getnframes()/w.getframerate();channels=w.getnchannels()
  rows.append(dict(asset=asset+('' if i==0 else '_V'+str(i)),event=name,txtp=str(txtp) if txtp else None,media_ids=source_ids,duration=duration,channels=channels,source='Riot patch 14.16 game SFX bank' if txtp else 'intentionally silent: no victim hit/continuous contact sound',mixed_video_audio=False,voice_bank=False,decode='float32 then peak-safe PCM16, short endpoint fades',sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))

def hero(n,w,e,r,impact,evolved=None):
 for slot,s in [('Weapon',w),('E',e),('R',r),('WeaponImpact',impact),('WeaponImpactEvolved','JinxBasicAttackEvolve_OnHit' if n=='Jinx' else impact),('WeaponEvolved',evolved or w)]:save('Champions/'+n+'_'+slot,event('Strawberry_'+n+'_'+s))
hero('Jinx','JinxAutoAttack_Missile_OnMissileLaunch','JinxLockDirection_OnBuffCast','JinxR_OnCast','JinxBasicAttack_OnHit','JinxBasicAttackEvolve_OnMissileLaunch')
hero('Seraphine','SeraphineEMissile_OnMissileLaunch','SeraphineW_OnCast','SeraphineR_cast','SeraphineEMissile_OnHit')
hero('Leona','LeonaBasicAttack_hit','LeonaSolarBarrier_OnCast','LeonaSolarFlare_OnCast','LeonaSolarBarrier_hit')
hero('Illaoi','IllaoiTentacleAttack_OnBuffActivate','IllaoiQ_OnCast','IllaoiR_OnCast','IllaoiTentacleAttack_minion_hit')
hero('Briar','BriarWAttackSpell_OnCast','BriarW_OnCast','BriarR_OnCast','BriarQ_hit')
hero('Yasuo','YasuoAutoMis_OnMissileLaunch','YasuoE_OnCast','YasuoWMovingWall_cast','YasuoAutoMis_OnHit')
hero('Riven','RivenTriCleave_hit_ground_sml','RivenFeint_OnCast','RivenFengShuiEngine_OnCast','RivenTriCleave_hit_ground_sml','RivenTriCleave_hit_ground_lrg')
hero('Aurora','AuroraQ_OnMissileLaunch','AuroraE_OnCast','AuroraR_OnCast','AuroraQ_OnHit')
hero('Xayah','XayahAutoMis_OnMissileLaunch','XayahE_OnCast','XayahR_OnCast','XayahAutoMis_OnHit')
for n,s in [('Jinx_RImpact','Jinx_JinxR_hit'),('Leona_RImpact','Leona_LeonaSolarFlare_hit'),('Leona_EImpact','Leona_LeonaSolarBarrier_hit'),('Aurora_Detonate','Aurora_AuroraQ_detonate')]:save('Champions/'+n,event('Strawberry_'+s))
# Missing constant hit/tick sounds are intentionally silent rather than fabricated pops.
general={
'Mine':('mine_OnMissileCast','mine_explosion'), 'Train':('trainmissile_OnMissileLaunch','trainmissile_OnHit'),
'Boomerang':('boomerangmissile_OnMissileLaunch','boomerangmissile_OnHit'), 'Trail':(None,None),
'Crescent':('dianaqinnermissile_OnMissileLaunch','dianaqinnermissile_OnHit'), 'Launcher':('rapidshotmis_OnMissileLaunch','rapidshotmis_OnHit'),
'Blade':('pierceshotmis_OnMissileLaunch','pierceshotmis_OnHit'),'Tibber':('tibbersburnaura_OnBuffCast',None),
'Gatling':('corkiecast_OnBuffCast','corkiecast_hit'),'Bomb':('mine_buffactivate','mine_explosion'),
'Fire':('firebombmissile_OnMissileCast','firebombmissile_OnHitLocation'),'Crossbow':('scatterarrowsmis_OnMissileCast','scatterarrowsmis_OnHit'),
'Orbital':('starstrike_warning','starstrike_explode'),'IceArmor':('frostnova_ready_buffactivate','frostnova_detonate_buffactivate'),
'Statikk':('statikkshiv_beam_buffactivate','statikkshiv_hit'),'Radiance':(None,'hollowradiance_hit'),
'Glove':('circleshotmis_OnMissileLaunch','circleshotmis_OnHit'),'Cyclone':('holybauble_buffactivate','holybauble_hit'),
'Yuumi':(None,None),'Annihilator':('nuke_warning','nuke_explosion')}
evolved={'Boomerang':('boomerangevolvemissile_OnMissileLaunch','boomerangevolvemissile_OnHit'),'Launcher':('rapidshot_evolvemis_OnMissileLaunch','rapidshot_evolvemis_OnHit'),'Blade':('pierceshotevolve_cast','pierceshotmisevolve_OnHit'),'Glove':('circleshotmis_OnMissileLaunch','circleshot_evolve_mis_OnHit'),'Crescent':('dianaq_evolve_buffactivate','dianaq_evolve_detonate')}
for n,pair in general.items():
 for ev,values in [(False,pair),(True,evolved.get(n,pair))]:
  for slot,s in zip(['Weapon','Impact'],values):save('General/'+n+'_'+slot+('Evolved' if ev else ''),event('augment_weapon_'+s) if s else None)
# Actual monster family deaths. Audio banks reuse deaths across related rigs.
deaths={'Swarmer':'Enemy_Swarmer_Death','Pix':'Enemy_Pix_Death','Ghoul':'Enemy_BasicMelee_Death','Fuemigo':'Enemy_BasicMelee3_Fuemigo_Death','Red':'Enemy_BasicMelee4_RED_Death','Fast':'Enemy_Fast_Death','Exploder':'Enemy_Exploder_Vox_Death','Shy':'Enemy_Shy_Death','Divider':'Enemy_Divider_Death','Spawner':'Enemy_Bruiser_Death','Tibbers':'Enemy_Bruiser_Death','Daisy':'Enemy_Bruiser_Death','Shield':'Enemy_Shield_Death','Hunter':'Enemy_Hunter_Death','Lancer':'Enemy_Lancer_Death','Minidasher':'Enemy_MiniDasher_Death','MiniReksai':'Enemy_Ram_Death','Shark':'Enemy_Cannon_Death','Toad':'Enemy_Poisonous_Death','Spider':'MiniBoss_Artillery_Death','Beetle':'MiniBoss_Dasher_Death','Crusher':'MiniBoss_Bruiser_Death','Artillery':'MiniBoss_Artillery_Death','Seahorse':'MiniBoss_Dasher_Death','Loot':'Enemy_Bruiser_Death'}
attacks={'Shark':('MiniBoss_Shooter_cast','MiniBoss_Shooter_shot_missile_OnHit'),'Spider':('MiniBoss_Artillery_AoE_fall','MiniBoss_Artillery_AoE_hit'),'Crusher':('MiniBoss_Bruiser_slam_cast','MiniBoss_Bruiser_slam_aoe'),'Artillery':('MiniBoss_Artillery_AoE_fall','MiniBoss_Artillery_AoE_hit'),'Seahorse':('Miniboss_Dasher_Dash_OnCast','Miniboss_Dasher_Dash_OnHit'),'Beetle':('Miniboss_Dasher_Dash_OnCast','Miniboss_Dasher_Dash_OnHit'),'Briar':('Boss_Briar_BasicAttack_cast','Boss_Briar_BasicAttack_hit'),'Reksai':('Boss_Reksai_ReksaiQAttack_cast','Boss_Reksai_ReksaiQAttack_hit'),'Belveth':('Boss_Belveth_BelvethQ_charge_buffactivate','Boss_Belveth_BelvethQ_hit'),'Aatrox':('Boss_Aatrox_AatroxQ1_cast','Boss_Aatrox_AatroxQ1_hit_ground')}
profiles=json.loads(Path('ArtSource/EnemyMotion/Attacks/Manifest.json').read_text())
for p in profiles:
 n=p['name'];pair=attacks.get(n,(None,None))
 for cue,s in [('Hit',None),('Death',deaths.get(n)),('Windup',pair[0]),('Attack',pair[1])]:save('Enemies/Variants/'+n+'_'+cue,event('Strawberry_'+s) if s else None)
for i,n in enumerate(['Swarmer','Pix','Ghoul']):save('Enemies/Minion_Death_'+str(i),event('Strawberry_'+deaths[n]))
for asset,s in [('Minion_Hit',None),('Minion_Death','Enemy_Swarmer_Death'),('Elite_Windup','MiniBoss_Bruiser_slam_cast'),('Elite_Impact','MiniBoss_Bruiser_slam_aoe'),('Boss_Entrance','Boss_Reksai_spawn_buffactivate'),('Boss_Shockwave','Boss_Aatrox_LineRift_slam')]:save('Enemies/'+asset,event('Strawberry_'+s) if s else None)
for n,spawn,impact in [('Reksai','Reksai_spawn_buffactivate','Reksai_ReksaiW_hit'),('Briar','Briar_spawn_buffactivate','Briar_push_hit'),('Belveth','Belveth_BelvethR_OnCast','Belveth_BelvethR_hit'),('Aatrox','Aatrox_Taunt3D_buffactivate_into','Aatrox_LineRift_slam')]:
 for slot,ev in [('Entrance',spawn),('Shockwave',impact)]:save('Enemies/Variants/'+n+'_'+slot,event('Strawberry_Boss_'+ev))
save('Enemies/Variants/Aatrox_Revive',event('Strawberry_Boss_Aatrox_Revive_buffactivate'))
save('Enemies/Exploder_Explosion',event('Strawberry_Enemy_Exploder_Death'))
save('Pickups/Experience',event('Strawberry_Drop_XP_Orb_Missile_hit'));save('Pickups/Gold',event('Strawberry_Rebuild_drop_gold_missile_OnHit'))
for asset,ev in [('UI/LevelUp','Play_sfx_hud_base_spell_levelup'),('UI/CardSelect','Play_sfx_hud_base_LevelUp_release'),('UI/Evolution','Play_sfx_hud_base_spell_upgrade'),('Pickups/AccessCard','Play_sfx_hud_base_spell_upgrade')]:save(asset,ev)
# Music states read from the original SetState events, not guessed soundtrack labels.
for asset,state in [('Map0',1228344690),('Map1',1228344689),('Map2',1228344688),('Map3',1228344695),('Mid',604807442),('Late',1976480282),('Boss0',3303962426),('Boss1',4010728055),('Boss2',1039808533),('Boss3',3350642212),('Boss3Phase2',1642469326)]:
 chosen=[p for p in alltxt if p.name.startswith('Play_mus_map33_game_start (2671167549='+str(state)+')')]
 if not chosen:raise RuntimeError('Missing music state '+str(state))
 save('Music/'+asset,'Play_mus_map33_game_start',1,chosen=chosen)
for label in ['Victory','Defeat']:save('Music/'+label,'Play_mus_map33_'+label.lower(),1)
shutil.copy2(OUT/'Music/Map0.wav',OUT/'Music/Combat_Loop.wav');r=next(x for x in rows if x['asset']=='Music/Map0').copy();r['asset']='Music/Combat_Loop';rows.append(r)
(META/'Manifest.json').write_text(json.dumps(rows,indent=2));(META/'VariantCounts.json').write_text(json.dumps(counts,indent=2))
inc=''.join('{TEXT('+json.dumps(k)+'), '+str(v)+'},\n' for k,v in counts.items() if v>1);Path('Source/TroubleSquad/OriginalAudioVariants.inl').write_text(inc,encoding='utf-8')
Path('Source/TroubleSquad/OriginalAudioSilence.inl').write_text(''.join('TEXT('+json.dumps(x['asset'])+'),\n' for x in rows if x['event'] is None),encoding='utf-8')
print('ORIGINAL_AUDIO_BAKED',len(rows),'cues',len(counts),'intentional_silence',sum(x['event'] is None for x in rows))
