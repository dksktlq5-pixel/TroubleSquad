import json,re
from pathlib import Path
from riotmanifest import PatcherManifest,WADExtractor
out=Path('Saved/OriginalAudio');m=PatcherManifest(out/'Swarm.manifest',path=out/'RemoteCache');e=WADExtractor(m,retry_limit=2,bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/');audio=json.loads((out/'AllAudioPaths.json').read_text())
for f in m.filter_files(pattern='wad.client'):
 if not re.search(r'/Strawberry_Boss_(Aatrox|Belveth|Briar|Reksai)\.wad\.client$',f.name,re.I):continue
 h=e.get_wad_header(f);ids={x.path_hash for x in h.files};paths=[p for k,p in audio.items() if int(k,16) in ids and '/sfx/characters/' in p and p.endswith(('.bnk','.wpk')) and not (out/'SwarmBanks'/p).exists()]
 print(f.name,'banks',len(paths),flush=True)
 for _,files in e.extract_files({f.name:paths}).items():
  for name,blob in files.items():
   if blob is None:continue
   p=out/'SwarmBanks'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(blob);print('SAVED',name,flush=True)
