import json
from pathlib import Path
from riotmanifest import PatcherManifest,WADExtractor
from loguru import logger
logger.enable('riotmanifest')
out=Path('Saved/OriginalAudio');all=list(json.loads((out/'AllSwarmPaths.json').read_text()).values());audio=list(json.loads((out/'AllAudioPaths.json').read_text()).values())
m=PatcherManifest(out/'Swarm.manifest',path=out/'RemoteCache',bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/')
e=WADExtractor(m,retry_limit=2,bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/')
targets={};names=['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']
for name in names:
 targets['DATA/FINAL/Champions/Strawberry_'+name+'.wad.client']=[p for p in audio if 'mode_strawberry_'+name.lower()+'_' in p]+['data/characters/strawberry_'+name.lower()+'/skins/skin0.bin']
targets['DATA/FINAL/Maps/Shipping/Map33.wad.client']=[]
for wad,paths in targets.items():
 print('FETCH',wad,len(paths),flush=True)
 data=e.extract_files({wad:paths})
 for _,files in data.items():
  for name,blob in files.items():
   if blob is None:continue
   p=out/'SwarmBanks'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(blob);print('SAVED',name,len(blob),flush=True)
