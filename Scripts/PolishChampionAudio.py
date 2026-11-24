raise SystemExit("Retired mixed-recording pipeline. Use BuildCleanArcadeAudio.py and ImportCleanArcadeAudio.py")
"""Readable arcade mix; reference footage is mixed audio, not isolated stems."""
from pathlib import Path
import wave, json, shutil
import numpy as np
base=Path('ArtSource/AudioMotionRevision/BeforeArcadePolish');base.mkdir(exist_ok=True)
rows=[]
for p in Path('ArtSource/AudioArcade/Champions').glob('*.wav'):
 src=base/p.name
 if not src.exists():shutil.copy2(p,src)
 with wave.open(str(src)) as w:
  rate=w.getframerate();a=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float)/32768
 f=np.fft.rfftfreq(len(a),1/rate);s=np.fft.rfft(a)
 # Remove sub-bass masking, preserve attack definition without harsh hiss.
 gain=(.18+.82*np.minimum(1,f/240))*(1+.30*np.exp(-((f-2400)/1800)**2))/(1+(f/9000)**6)
 a=np.fft.irfft(s*gain,n=len(a));a=np.tanh(a*1.15)/1.15
 fade=min(int(rate*.015),len(a)//4);a[-fade:]*=np.linspace(1,0,fade)
 a*=min(1,.78/max(.001,max(abs(a))))
 with wave.open(str(p),'wb') as w:
  w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes((a*32767).astype('<i2').tobytes())
 rows.append({'file':p.name,'seconds':len(a)/rate,'peak':float(max(abs(a))),'isolated_reference':False})
assert len(rows)==27
Path('ArtSource/AudioMotionRevision/ArcadePolish.json').write_text(json.dumps(rows,indent=2))
print('ARCADE_AUDIO_POLISH 27/27')
