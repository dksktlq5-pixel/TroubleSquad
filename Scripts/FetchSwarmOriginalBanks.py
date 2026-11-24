import json
from loguru import logger
logger.enable("riotmanifest")
from pathlib import Path
from riotmanifest import PatcherManifest,WADExtractor
out=Path('Saved/OriginalAudio');paths=list(json.loads((out/'AllAudioPaths.json').read_text()).values());targets=[p for p in paths if ('mode_strawberry_' in p or 'strawberry_npc_missfortune' in p) and 'strawberryrebuild' not in p]
m=PatcherManifest(out/'Swarm.manifest',path=out/'RemoteCache',bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/')
e=WADExtractor(m,retry_limit=2,bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/')
print('Fetching exact game banks',len(targets),flush=True)
data=e.extract_files({'DATA/FINAL/Maps/Shipping/Map33.wad.client':targets});found=[]
for wad,files in data.items():
 for name,blob in files.items():
  if blob is None:print('MISSING',name,flush=True);continue
  p=out/'SwarmBanks'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(blob);found.append(name);print('SAVED',name,len(blob),flush=True)
(out/'SwarmBankManifest.json').write_text(json.dumps(found,indent=2));print('SWARM_ORIGINAL_BANKS',len(found),flush=True)
