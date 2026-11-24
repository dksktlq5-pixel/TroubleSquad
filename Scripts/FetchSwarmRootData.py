import json
from pathlib import Path
from riotmanifest import PatcherManifest,WADExtractor
out=Path('Saved/OriginalAudio');all=list(json.loads((out/'AllSwarmPaths.json').read_text()).values())
m=PatcherManifest(out/'Swarm.manifest',path=out/'RemoteCache',bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/');e=WADExtractor(m,retry_limit=2,bundle_url='https://lol.secure.dyn.riotcdn.net/channels/public/bundles/')
paths=[p for p in all if p.startswith('data/characters/') and p.endswith('.bin') and ('/skins/root.bin' in p or '/animations/skin0.bin' in p or p.rsplit('/',1)[-1]==p.split('/')[2]+'.bin')]
targets={'DATA/FINAL/Maps/Shipping/Map33.wad.client':paths+['data/maps/shipping/map33/map33.bin']}
for name in ['Jinx','Seraphine','Leona','Illaoi','Briar','Yasuo','Riven','Aurora','Xayah']:
 targets['DATA/FINAL/Champions/Strawberry_'+name+'.wad.client']=[p for p in paths if '/strawberry_'+name.lower()+'/' in p]
for wad,ps in targets.items():
 print('FETCH',wad,len(ps),flush=True)
 for _,files in e.extract_files({wad:ps}).items():
  for name,blob in files.items():
   if blob is None:continue
   p=out/'SwarmBanks'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(blob)
 print('DONE',wad,flush=True)
