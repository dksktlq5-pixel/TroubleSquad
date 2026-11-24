import json,re
from pathlib import Path
from riotmanifest import PatcherManifest,WADExtractor
out=Path('Saved/OriginalAudio');m=PatcherManifest(out/'Swarm.manifest',path=out/'RemoteCache');e=WADExtractor(m,retry_limit=2,bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/');f=next(x for x in m.filter_files(pattern='Map33.wad.client') if x.name.endswith('/Map33.wad.client'));h=e.get_wad_header(f);ids={x.path_hash for x in h.files};audio=json.loads((out/'AllAudioPaths.json').read_text());paths=[p for k,p in audio.items() if int(k,16) in ids and '/sfx/' in p and p.endswith(('.bnk','.wpk')) and re.search(r'/characters/(aatrox|belveth|briar|reksai|strawberry_[^/]+|yuumi)/',p) and not (out/'SwarmBanks'/p).exists()]
print('EXACT_DEPENDENCIES',len(paths),flush=True);print('\n'.join(paths),flush=True)
for _,files in e.extract_files({f.name:paths}).items():
 for name,blob in files.items():
  if blob is None:continue
  p=out/'SwarmBanks'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(blob)
print('SWARM_DEPENDENCIES_SAVED',len(paths),flush=True)
