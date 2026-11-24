from pathlib import Path
import subprocess,shutil
r=Path('Saved/OriginalAudio');banks=[p for p in (r/'SwarmBanks/assets/sounds/wwise2016/sfx/shared').glob('*.bnk') if p.name!='init.bnk']
for p in banks:shutil.copy2(p,r/'AllWems'/p.name)
cmd=['python','Saved/Tools/audio/wwiser.pyz','-d','none','-g','-gd','-gra','-gwd','-gxnl','-nl',str(r/'EventNames.txt'),'-go',str(r/'TXTPNativeMix'),'-gw',str((r/'AllWems').resolve())]+[str(p) for p in banks]
p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(r/'TxtpFullGeneration.log').write_bytes(p.stdout);print('TXTP',p.returncode,len(list((r/'TXTPNativeMix').rglob('*.txtp'))))
