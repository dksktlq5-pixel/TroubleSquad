raise SystemExit("Original game-bank pipeline is active. Use BuildOriginalSwarmAudio.py / ImportOriginalSwarmAudio.py")
"""Voice-free arcade sound design. No recordings or existing audio are read."""
from pathlib import Path
import json,wave,hashlib
import numpy as np
from scipy.signal import butter,sosfilt
RATE=44100
OUT=Path('ArtSource/AudioArcade');AUDIT=Path('ArtSource/CleanArcadeAudio');AUDIT.mkdir(exist_ok=True)
rows=[]
def canvas(seconds):return np.zeros(int(RATE*seconds))
def env(t,attack=.008,decay=.10):return (1-np.exp(-t/attack))*np.exp(-t/decay)
def tone(seconds,f0,f1=None,decay=.12,attack=.008):
 t=np.arange(int(RATE*seconds))/RATE;f=f0 if f1 is None else f0+(f1-f0)*t/max(seconds,.001)
 return np.sin(2*np.pi*np.cumsum(np.broadcast_to(f,t.shape))/RATE)*env(t,attack,decay)
def noise(seconds,seed,low=500,high=6500,decay=.09,attack=.008):
 t=np.arange(int(RATE*seconds))/RATE;a=np.random.default_rng(seed).normal(0,1,len(t));a=sosfilt(butter(2,[low,high],fs=RATE,btype='bandpass',output='sos'),a)
 return a*env(t,attack,decay)
def add(a,b,at=0,gain=1):
 k=int(at*RATE);n=min(len(b),len(a)-k)
 if n>0:a[k:k+n]+=b[:n]*gain
 return a
def chime(seconds,freq,seed):
 a=canvas(seconds)
 for ratio,gain in [(1,.45),(2.01,.20),(3.98,.075)]:add(a,tone(seconds,freq*ratio,decay=.11/ratio**.4),gain=gain)
 return add(a,noise(seconds,seed,1600,8500,.025),gain=.10)
def whoosh(seconds,seed,low=700,high=6500):
 t=np.arange(int(RATE*seconds))/RATE;a=noise(seconds,seed,low,high,seconds*.8,.025)
 return a*np.sin(np.pi*np.minimum(1,t/seconds))
def spell(name,slot,seed):
 scale=[.24,.55,1.05][slot];a=canvas(scale);strength=[1,1.1,1.3][slot]
 if name=='Jinx':
  for j in range([2,3,4][slot]):
   at=j*.045;add(a,tone(.11,2400,900,.024,.002),at,.14);add(a,noise(.09,seed+j,1600,9000,.018,.003),at,.36)
  if slot==2:add(a,noise(.7,seed+9,150,2600,.16,.012),.12,.40)
 elif name=='Seraphine':
  for j,f in enumerate([880,1108.73,1318.51][:slot+1]):add(a,chime(scale-j*.06,f,seed+j),j*.06,.55)
  add(a,whoosh(scale,seed,1400,9500),gain=.28)
 elif name=='Leona':
  for j,f in enumerate([680,1190,1850,2730]):add(a,tone(scale,f,decay=.07+.03*slot),gain=.23/(j+1))
  add(a,noise(scale,seed,500,6500,.04+.02*slot),gain=.55)
  if slot: add(a,chime(scale,1568,seed+1),.05,.30)
 elif name=='Illaoi':
  for j in range(slot+1):
   at=j*.17;add(a,whoosh(.24,seed+j,180,2200),at,.7);add(a,noise(.17,seed+j+20,250,3800,.065,.012),at+.06,.50);add(a,tone(.20,110,80,.05,.012),at+.08,.12)
 elif name=='Briar':
  for j in range(3+slot):add(a,whoosh(.11,seed+j,1000,8500),j*.035,.72)
  add(a,noise(scale,seed+30,300,2700,.08),gain=.27)
 elif name=='Yasuo':
  add(a,whoosh(scale,seed,800,8200),gain=.9);add(a,tone(scale,1700,1100,.09),gain=.12)
  if slot==2:
   for j in range(3):add(a,whoosh(.28,seed+j+10,1200,7500),j*.18,.55)
 elif name=='Riven':
  add(a,whoosh(scale,seed,650,6000),gain=.8)
  for j,f in enumerate([740,1480,2220]):add(a,tone(scale,f,decay=.07+.04*slot),gain=.17/(j+1))
 elif name=='Aurora':
  for j in range(3+slot):add(a,chime(max(.05,scale-j*.035),[1174.66,1567.98,1760,2349.32,3135.96][j],seed+j),j*.035,.36)
  add(a,whoosh(scale,seed,1800,10000),gain=.20)
 elif name=='Xayah':
  for j in range(2+slot):
   at=j*.035;add(a,tone(.14,1900+j*130,1300+j*110,.035,.006),at,.16);add(a,whoosh(.13,seed+j,2000,10000),at,.60)
 return a*strength

def save(asset,a,target=.065,category='effect'):
 a=np.nan_to_num(a);a-=a.mean();fade=min(int(.006*RATE),len(a)//8);a[:fade]*=np.linspace(0,1,fade);a[-fade:]*=np.linspace(1,0,fade)
 rms=float(np.sqrt(np.mean(a*a)));a*=min(3,target/max(.001,rms));a*=min(1,.65/max(.001,max(abs(a))))
 p=OUT/(asset+'.wav');p.parent.mkdir(parents=True,exist_ok=True)
 with wave.open(str(p),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes((a*32767).astype('<i2').tobytes())
 rows.append(dict(asset=asset,duration=len(a)/RATE,peak=float(max(abs(a))),rms=float(np.sqrt(np.mean(a*a))),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),source='procedural oscillator and filtered noise only',recorded_audio_inputs=0,human_voice_inputs=0,category=category))
 return a
names=['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah'];heroes={}
for i,name in enumerate(names):
 for slot,label in enumerate(['Weapon','E','R']):heroes[name,label]=save('Champions/'+name+'_'+label,spell(name,slot,100+i*11+slot))
profiles=json.loads(Path('ArtSource/EnemyMotion/Attacks/Manifest.json').read_text())
for i,p in enumerate(profiles):
 name=p['name'];seed=900+i*13;heavy=p['profile'] in ['boss','heavy'];flying=name in ['Seahorse','Beetle','Shark','Artillery']
 for j,cue in enumerate(['Hit','Death','Windup','Attack']):
  length=[.10,.34,.48,.23][j]*(1.25 if heavy else 1);a=canvas(length)
  if cue=='Hit':
   # Dry contact, no swept sine or explosive low-frequency balloon pop.
   add(a,noise(length,seed,850,6500,.022,.006),gain=.70)
  elif cue=='Death':
   add(a,noise(length,seed+1,350 if heavy else 650,4300,.12,.012),gain=.60)
   for k in range(3):add(a,noise(.065,seed+k+10,1800,8500,.016,.006),k*.038,.16)
   if name in ['Toad','Fuemigo','Exploder']:add(a,noise(length,seed+2,140,1400,.13,.015),gain=.35)
  elif cue=='Windup':
   add(a,whoosh(length,seed+3,450 if heavy else 1200,4800),gain=.65)
   if heavy:add(a,tone(length,170,220,.24,.025),gain=.10)
  else:
   add(a,whoosh(length,seed+4,1200 if flying else 500,8200),gain=.75)
   if heavy:add(a,noise(.17,seed+5,180,2600,.06,.01),.04,.35)
  save('Enemies/Variants/'+name+'_'+cue,a,.025 if cue=='Hit' else .05)
for cue,source in [('Minion_Hit','Swarmer_Hit'),('Minion_Death','Swarmer_Death'),('Minion_Death_0','Swarmer_Death'),('Minion_Death_1','Pix_Death'),('Minion_Death_2','Ghoul_Death'),('Elite_Windup','Crusher_Windup'),('Elite_Impact','Crusher_Attack'),('Boss_Entrance','Aatrox_Windup'),('Boss_Shockwave','Aatrox_Attack')]:
 src=next(x for x in rows if x['asset']=='Enemies/Variants/'+source)
 with wave.open(str(OUT/(src['asset']+'.wav'))) as w:a=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float)/32768
 save('Enemies/'+cue,a,.035 if 'Hit' in cue else .05)
general=['Mine','Train','Boomerang','Trail','Crescent','Launcher','Blade','Tibber','Gatling','Bomb','Fire','Crossbow','Orbital','IceArmor','Statikk','Radiance','Glove','Cyclone','Yuumi','Annihilator']
family=['Jinx','Leona','Yasuo','Briar','Yasuo','Jinx','Xayah','Illaoi','Jinx','Jinx','Aurora','Xayah','Leona','Leona','Aurora','Seraphine','Riven','Yasuo','Seraphine','Jinx']
for i,(name,fam) in enumerate(zip(general,family)):
 for slot,label in enumerate(['Weapon','Impact']):
  a=spell(fam,0,2000+i*7+slot)
  if name in ['Mine','Bomb','Annihilator'] and slot:a=add(canvas(.4),noise(.4,3000+i,220,5500,.10,.008),gain=.65)
  if name=='Statikk':a=noise(.15,3200+i,2600,11000,.035,.003)
  if name=='Train':a=add(chime(.30,420,3100+i),noise(.30,3101+i,600,6500,.045),gain=.3)
  save('General/'+name+'_'+label,a,.045)
# Original instrumental loop, deliberately no captured video/microphone material.
bpm=128;beat=60/bpm;bars=16;music=canvas(bars*4*beat)
chords=[[62,65,69],[58,62,65],[65,69,72],[60,64,67]]
for b in range(bars*4):
 at=b*beat;chord=chords[(b//4)%4]
 # Short, bright pluck arpeggio and soft bass; no large cinematic booms.
 for half in range(2):
  midi=chord[(b*2+half)%3]+12;f=440*2**((midi-69)/12)
  add(music,chime(.32,f,4000+b*2+half),at+half*beat/2,.13)
 bass=440*2**((chord[0]-12-69)/12);add(music,tone(.25,bass,decay=.07,attack=.010),at,.14)
 if b%2==0:add(music,tone(.12,95,65,.032,.006),at,.17)
 else:add(music,noise(.10,5000+b,1600,8500,.025,.004),at,.10)
 for half in range(2):add(music,noise(.045,6000+b*2+half,6500,14000,.01,.003),at+half*beat/2,.05)
save('Music/Combat_Loop',music,.10,'original instrumental music')
assert len(rows)==193
(AUDIT/'Manifest.json').write_text(json.dumps(rows,indent=2));print('CLEAN_ARCADE_AUDIO',len(rows),'recorded_audio_inputs=0 human_voice_inputs=0')
