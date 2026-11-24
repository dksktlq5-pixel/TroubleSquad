raise SystemExit("DISABLED: recording contains human speech; use BuildCleanArcadeAudio.py / ImportCleanArcadeAudio.py")
"""Extract actual recording samples. Event labels are candidates, not isolated game stems."""
import sys,json,wave,subprocess,shutil
from pathlib import Path
import numpy as np
from scipy.signal import stft,istft,find_peaks
from scipy.ndimage import median_filter
sys.path.insert(0,'Saved/Tools/video');import imageio_ffmpeg
RATE=22050;root=Path('ArtSource/ReferenceAudio');root.mkdir(exist_ok=True);backup=root/'Previous';backup.mkdir(exist_ok=True)
ff=imageio_ffmpeg.get_ffmpeg_exe();rows=[];cache={}
def read(src):
 raw=root/(src.stem+'.wav')
 subprocess.run([ff,'-y','-i',str(src),'-vn','-ac','1','-ar',str(RATE),'-c:a','pcm_s16le',str(raw)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
 with wave.open(str(raw)) as w:return np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float)/32768

def separate(src):
 if str(src) in cache:return cache[str(src)]
 a=read(src);_,_,z=stft(a,fs=RATE,nperseg=1024,noverlap=768);mag=abs(z)
 harmonic=median_filter(mag,size=(1,31));transient=median_filter(mag,size=(17,1))
 hm=harmonic**2/(harmonic**2+transient**2+1e-12)
 _,fx=istft(z*(1-.92*hm),fs=RATE,nperseg=1024,noverlap=768)
 _,music=istft(z*hm**2,fs=RATE,nperseg=1024,noverlap=768)
 novelty=np.maximum(0,np.diff(mag,axis=1)).sum(axis=0)
 peaks,_=find_peaks(novelty,distance=int(.36*RATE/256),prominence=np.std(novelty)*.4)
 peaks=sorted([int(k*256) for k in peaks if 1<k*256/RATE<len(a)/RATE-1],key=lambda k:novelty[min(k//256,len(novelty)-1)],reverse=True)
 if not peaks:raise RuntimeError('No reference transients '+str(src))
 cache[str(src)]=(fx,music,peaks);return cache[str(src)]
def save(asset,a,src,offset,method,target=.065):
 dst=Path('ArtSource/AudioArcade')/(asset+'.wav');old=backup/(asset+'.wav');old.parent.mkdir(parents=True,exist_ok=True)
 if dst.exists() and not old.exists():shutil.copy2(dst,old)
 a=np.nan_to_num(a.copy());a-=a.mean();fade=min(int(.008*RATE),len(a)//4);a[:fade]*=np.linspace(0,1,fade);a[-fade:]*=np.linspace(1,0,fade)
 rms=np.sqrt(np.mean(a*a));a*=min(4,target/max(.001,rms));a*=min(1,.72/max(.001,max(abs(a))))
 dst.parent.mkdir(parents=True,exist_ok=True)
 with wave.open(str(dst),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes((a*32767).astype('<i2').tobytes())
 rows.append(dict(asset=asset,source=str(src),clip_offset_seconds=round(offset,3),duration=len(a)/RATE,peak=float(max(abs(a))),rms=float(np.sqrt(np.mean(a*a))),method=method,event_identity_verified=False,isolated_original_stem=False))
def sample(src,rank,length):
 fx,music,peaks=separate(src);k=max(0,peaks[rank%len(peaks)]-int(.012*RATE));return fx[k:k+int(length*RATE)],k/RATE
names=['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']
for name in names:
 for slot,rank,duration in [('Weapon',0,.20),('E',1,.42),('R',0,.72)]:
  src=Path('Saved/VideoReference/SoundMotion')/(name+('_Boss.mp4' if slot=='R' else '_Combat.mp4'))
  a,t=sample(src,rank,duration);save('Champions/'+name+'_'+slot,a,src,t,'recorded transient, harmonic background suppression')
# Multiple distinct recording events replace the shared synthesized balloon-like death.
src=Path('Saved/VideoReference/StageOneAudio.webm');fx,music,peaks=separate(src)
profiles=json.loads(Path('ArtSource/EnemyMotion/Attacks/Manifest.json').read_text())
for i,p in enumerate(profiles):
 for cue,length,rank in [('Hit',.085,8),('Death',.19,16),('Windup',.40,24),('Attack',.25,32)]:
  a,t=sample(src,rank+i%8,length);save('Enemies/Variants/'+p['name']+'_'+cue,a,src,t,'recorded event candidate; species identity unverified',.035 if cue=='Hit' else .055)
for i in range(3):
 a,t=sample(src,16+i,.19);save('Enemies/Minion_Death_'+str(i),a,src,t,'recorded death candidate',.055)
for cue,rank,length in [('Minion_Hit',8,.085),('Minion_Death',16,.19),('Elite_Windup',24,.4),('Elite_Impact',32,.25)]:
 a,t=sample(src,rank,length);save('Enemies/'+cue,a,src,t,'recorded event candidate',.045)
# Reuse reference families without fabricated pitch variation; exact weapon identities remain unverified.
general=['Mine','Train','Boomerang','Trail','Crescent','Launcher','Blade','Tibber','Gatling','Bomb','Fire','Crossbow','Orbital','IceArmor','Statikk','Radiance','Glove','Cyclone','Yuumi','Annihilator']
families=['Jinx','Leona','Yasuo','Briar','Yasuo','Jinx','Xayah','Illaoi','Jinx','Jinx','Aurora','Xayah','Leona','Leona','Aurora','Seraphine','Riven','Yasuo','Seraphine','Jinx']
for w,n in zip(general,families):
 src2=Path('Saved/VideoReference/SoundMotion')/(n+'_Combat.mp4')
 for cue,rank,length in [('Weapon',0,.18),('Impact',2,.13)]:
  a,t=sample(src2,rank,length);save('General/'+w+'_'+cue,a,src2,t,'recorded family candidate',.045)
# Harmonic component from the actual video; not a clean studio music stem.
bgm=music[20*RATE:52*RATE];cross=RATE
bgm=np.concatenate([bgm[cross:-cross],bgm[-cross:]*np.linspace(1,0,cross)+bgm[:cross]*np.linspace(0,1,cross)])
save('Music/Combat_Loop',bgm,src,20,'harmonic music estimate, one-second loop crossfade; SFX residue possible',.12)
for dst,source in [('Enemies/Boss_Entrance','Champions/Leona_R'),('Enemies/Boss_Shockwave','Champions/Briar_R')]:
 row=next(x for x in rows if x['asset']==source).copy();row['asset']=dst;row['method']='recorded boss-event candidate; exact boss identity unverified'
 target=Path('ArtSource/AudioArcade')/(dst+'.wav');old=backup/(dst+'.wav');old.parent.mkdir(parents=True,exist_ok=True)
 if target.exists() and not old.exists():shutil.copy2(target,old)
 shutil.copy2(Path('ArtSource/AudioArcade')/(source+'.wav'),target);rows.append(row)
meta=json.loads(Path('Saved/VideoReference/SoundMotionMetadata.json').read_text(encoding='utf-8-sig'));chapters=dict(zip(names,meta['chapters'][1:10]))
for row in rows:
 stem=Path(row['source']).stem;parts=stem.split('_');offset=0
 if parts[0] in chapters:
  c=chapters[parts[0]];offset=c['start_time']+120 if parts[-1]=='Combat' else c['end_time']-90
 row['video_url']='https://youtu.be/ixn4lcqno2o' if stem=='StageOneAudio' else 'https://youtu.be/TaGA8xu5LFc'
 row['video_time_seconds']=round(offset+row['clip_offset_seconds'],3)
assert len(rows)==193
(root/'Manifest.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print('REFERENCE_RECORDING_ASSETS',len(rows))
