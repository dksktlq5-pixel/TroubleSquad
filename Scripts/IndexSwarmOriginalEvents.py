import subprocess,re,json,xml.etree.ElementTree as ET
from pathlib import Path
root=Path('Saved/OriginalAudio');banks=root/'SwarmBanks/assets/sounds/wwise2016/sfx/shared';wems=root/'AllWems';wems.mkdir(exist_ok=True);xml=root/'BankXML';xml.mkdir(exist_ok=True)
for p in banks.glob('*audio.*'):
 if p.suffix not in ['.bnk','.wpk']:continue
 subprocess.run(['Saved/Tools/audio/bnk-extract.exe','--audio',str(p),'--wems-only','-o',str(wems)],stdout=subprocess.DEVNULL,check=True)
for p in banks.glob('*events.bnk'):
 dest=xml/p.stem
 subprocess.run(['python','Saved/Tools/audio/wwiser.pyz','-d','xml','-dn',str(dest),str(p)],stdout=subprocess.DEVNULL,check=True)
objects={};eventids=set()
for p in xml.glob('*.xml'):
 r=ET.parse(p).getroot()
 for obj in r.findall('./object[@name="HircChunk"]/list/object'):
  field=obj.find('./field[@name="ulID"]')
  if field is None:continue
  ident=int(field.get('value'));objects[ident]=obj
  if obj.get('name')=='CAkEvent':eventids.add(ident)
def hashname(s):
 h=2166136261
 for c in s.lower().encode():h=((h*16777619)&0xffffffff)^c
 return h
names={}
for p in (root/'SwarmBanks').rglob('*.bin'):
 for b in re.findall(rb'[Pp]lay_[A-Za-z0-9_]+',p.read_bytes()):
  text=b.decode();variants=[text,text.replace('Illaoi_Strawberry_Illaoi','Illaoi_Illaoi').replace('Riven_Strawberry_Riven','Riven_Riven')]
  for text in variants:
   for length in range(len(text),max(10,len(text)-6),-1):
    t=text[:length];h=hashname(t)
    if h in eventids:names[h]=t;break
# Follow only action targets and children, never parent/bus links.
def media(ident,seen=None):
 seen=set() if seen is None else seen
 if ident in seen or ident not in objects:return []
 seen.add(ident);obj=objects[ident];out=[]
 if obj.get('name')=='CAkSound':
  out.extend(int(x.get('value')) for x in obj.iter('field') if x.get('name')=='sourceID')
 else:
  for f in obj.iter('field'):
   if f.get('name') in ['ulActionID','idExt','ulChildID','ulPlayID','audioNodeId']:out+=media(int(f.get('value')),seen)
 return sorted(set(out))
result={name:{'event_id':ident,'media_ids':media(ident)} for ident,name in names.items()}
(root/'OriginalEvents.json').write_text(json.dumps(result,indent=2));(root/'EventNames.txt').write_text(''.join(f'{h} {n}\n' for h,n in sorted(names.items())))
print('NAMED_ORIGINAL_EVENTS',len(result),'WITH_MEDIA',sum(bool(x['media_ids']) for x in result.values()),'WEMS',len(list(wems.glob('*.wem'))))
