raise SystemExit("Retired mixed-recording pipeline. Use BuildCleanArcadeAudio.py and ImportCleanArcadeAudio.py")
import sys,json,wave,subprocess,shutil,hashlib
from pathlib import Path
import numpy as np
sys.path.insert(0,'Saved/Tools/video');import imageio_ffmpeg
ff=imageio_ffmpeg.get_ffmpeg_exe();root=Path('ArtSource/AudioMotionRevision');root.mkdir(exist_ok=True);base=root/'BaseChampions';base.mkdir(exist_ok=True)
for f in Path('ArtSource/AudioArcade/Champions').glob('*.wav'):
 if not (base/f.name).exists():shutil.copy2(f,base/f.name)
rate=22050

def read(p):
 with wave.open(str(p)) as w:a=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float64)/32768;sr=w.getframerate();ch=w.getnchannels()
 if ch>1:a=a.reshape(-1,ch).mean(axis=1)
 return np.interp(np.arange(round(len(a)*rate/sr))*sr/rate,np.arange(len(a)),a)
def write(p,a):
 p.parent.mkdir(parents=True,exist_ok=True);a=a.copy();n=min(100,len(a)//4);a[:n]*=np.linspace(0,1,n);a[-n:]*=np.linspace(1,0,n);r=np.sqrt(np.mean(a*a));a*=.12/max(.001,r);a*=min(1,.8/max(.001,np.max(np.abs(a))))
 with wave.open(str(p),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes((a*32767).astype('<i2').tobytes())
 return {'duration':len(a)/rate,'peak':float(max(abs(a))),'rms':float(np.sqrt(np.mean(a*a)))}
manifest=[];textures={}
for name in ['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']:
 src=Path('Saved/VideoReference/SoundMotion')/(name+'_Combat.mp4');raw=root/(name+'_Reference.wav');subprocess.run([ff,'-y','-i',str(src),'-vn','-ac','1','-ar',str(rate),'-c:a','pcm_s16le',str(raw)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True);a=read(raw)
 size=512;hop=128;window=np.hanning(size);frames=np.stack([a[i:i+size]*window for i in range(0,len(a)-size,hop)]);spec=np.fft.rfft(frames);mag=abs(spec);floor=np.median(mag,axis=0);mask=np.maximum(0,1-floor*1.8/(mag+1e-9));mask[:,np.fft.rfftfreq(size,1/rate)<1600]=0
 den=np.zeros(len(a));norm=np.zeros(len(a))
 for j,v in enumerate(np.fft.irfft(spec*mask,n=size)):
  i=j*hop;den[i:i+size]+=v*window;norm[i:i+size]+=window**2
 den/=np.maximum(norm,.01);energy=np.convolve(den**2,np.ones(220)/220,'same');energy[:rate]=0;energy[-rate:]=0;onset=max(0,int(np.argmax(energy)) - 440);tex=den[onset:onset+int(rate*.16)];tex/=max(.001,np.max(abs(tex)));textures[name]=tex
 for slot in ['Weapon','E','R']:
  f=base/(name+'_'+slot+'.wav');original=read(f);layer=np.zeros_like(original);limit=min(len(tex),len(layer));layer[:limit]=tex[:limit];mixed=original+layer*({'Weapon':.10,'E':.055,'R':.045}[slot]);dst=Path('ArtSource/AudioArcade/Champions')/f.name;row=write(dst,mixed);row.update({'asset':'Champions/'+f.stem,'reference':str(src),'reference_offset':onset/rate,'texture_seconds':.16,'isolated_original_stem':False});manifest.append(row)
profiles=json.loads(Path('ArtSource/EnemyMotion/Attacks/Manifest.json').read_text());destination=Path('ArtSource/AudioArcade/Enemies/Variants')
for row in profiles:
 name=row['name'];profile=row['profile'];pitch={'boss':.88,'heavy':.95,'flyer':1.25,'ranged':1.12,'melee':1.12}[profile]+(int(hashlib.sha1(name.encode()).hexdigest()[:2],16)%9-4)*.018
 for cue,source in [('Hit','Minion_Hit'),('Death','Minion_Death_0'),('Windup','Elite_Windup'),('Attack','Elite_Impact')]:
  a=read(Path('ArtSource/AudioArcade/Enemies')/(source+'.wav'));a=np.interp(np.arange(max(2,round(len(a)/pitch)))*pitch,np.arange(len(a)),a);dst=destination/(name+'_'+cue+'.wav');r=write(dst,a);r.update({'asset':'Enemies/Variants/'+dst.stem,'profile':profile,'pitch':pitch});manifest.append(r)
(root/'Manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8');assert len(manifest)==143;print('AUDIO_REVISION_READY 27 champion layers + 116 enemy variants')
