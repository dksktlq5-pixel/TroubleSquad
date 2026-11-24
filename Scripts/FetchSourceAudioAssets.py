import urllib.request,json,concurrent.futures
from pathlib import Path
from cdtb.wad import Wad
root=Path('D:/Riot Games/League of Legends/Game/DATA/FINAL');out=Path('Saved/OriginalAudio');out.mkdir(exist_ok=True)
names=['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']
paths=[root/'Champions'/(n+'.wad.client') for n in names]+[root/'Audio.wad.client',root/'Maps/Shipping/Common.wad.client']
wads=[Wad(str(p),hashes={}) for p in paths if p.exists()];wanted={f.path_hash for w in wads for f in w.files};print('WAD entries',len(wanted),flush=True)
def scan(i):
 result={};audio={};u=f'https://raw.githubusercontent.com/CommunityDragon/Data/master/hashes/lol/hashes.game.txt.{i}'
 with urllib.request.urlopen(u,timeout=90) as r:
  for line in r:
   line=line.decode('utf-8').rstrip();h,path=line.split(' ',1);key=int(h,16)
   if '/sounds/' in path:audio[h]=path
   if key in wanted:result[h]=path
 print('hash segment',i,'matched',len(result),'audio',len(audio),flush=True);return result,audio
matches={};audio={}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 for m,a in pool.map(scan,range(9)):matches.update(m);audio.update(a)
(out/'InstalledPaths.json').write_text(json.dumps(matches,indent=2));(out/'AllAudioPaths.json').write_text(json.dumps(audio,indent=2))
extracted=[]
for w in wads:
 with open(w.path,'rb') as archive:
  for f in w.files:
   path=matches.get(f'{f.path_hash:016x}','')
   if '/sounds/' not in path or not path.endswith(('.bnk','.wpk')) or '/vo/' in path:continue
   data=w.read_file_data(archive,f)
   if data is None:continue
   dest=out/'Banks'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data);extracted.append(path)
(out/'ExtractedBanks.json').write_text(json.dumps(extracted,indent=2));print('ORIGINAL_SFX_BANKS',len(extracted),flush=True)
