from pathlib import Path
import subprocess,json,sys,wave
import numpy as np
from scipy.signal import butter,sosfilt,fftconvolve,resample_poly
sys.path.insert(0,'Saved/Tools/video');import imageio_ffmpeg
rate=16000;filt=butter(3,[700,6500],fs=rate,btype='bandpass',output='sos');rows=[]
def pcm(p):
 with wave.open(str(p)) as w:
  a=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float)/32768;sr=w.getframerate();ch=w.getnchannels()
 if ch>1:a=a.reshape(-1,ch).mean(axis=1)
 return resample_poly(a,rate,sr)
for name in ['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']:
 src=Path('Saved/VideoReference/SoundMotion')/(name+'_Combat.mp4');tmp=Path('Saved/OriginalAudio')/(name+'_Compare.wav')
 subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-y','-i',str(src),'-vn','-ac','1','-ar',str(rate),'-c:a','pcm_s16le',str(tmp)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
 ref=sosfilt(filt,pcm(tmp));best={'score':0.}
 for p in Path('ArtSource/AudioArcade/Champions').glob(name+'_Weapon*.wav'):
  if 'Impact' in p.stem or 'Evolved' in p.stem:continue
  cue=sosfilt(filt,pcm(p));energy=np.convolve(cue**2,np.ones(160)/160,'same');k=max(0,int(np.argmax(energy))-320);cue=cue[k:k+6400]
  for cents in [-100,-50,0,50,100]:
   speed=2**(cents/1200);sample=np.interp(np.arange(int(len(cue)/speed))*speed,np.arange(len(cue)),cue);n=len(sample)
   corr=fftconvolve(ref,sample[::-1],mode='valid');norm=np.sqrt(np.maximum(1e-12,fftconvolve(ref**2,np.ones(n),mode='valid')*sum(sample**2)));scores=np.abs(corr)/norm;at=int(np.argmax(scores));score=float(scores[at])
   if score>best['score']:best={'score':score,'clip_offset_seconds':at/rate,'candidate':p.stem,'pitch_cents':cents}
 best['character']=name;rows.append(best);print(name,round(best['score'],3),flush=True)
Path('Saved/Logs/OriginalAudioReferenceComparison.json').write_text(json.dumps({'method':'normalized band-limited waveform cross-correlation; not a fidelity percentage','results':rows},indent=2))
