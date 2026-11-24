raise SystemExit("Retired mixed-recording pipeline. Use BuildCleanArcadeAudio.py and ImportCleanArcadeAudio.py")
import unreal,json
from pathlib import Path
rows=json.loads(Path('D:/UnReal/TroubleSquad/ArtSource/AudioMotionRevision/Manifest.json').read_text());tools=unreal.AssetToolsHelpers.get_asset_tools();tasks=[]
for row in rows:
 rel=Path(row['asset']);t=unreal.AssetImportTask();t.filename=str(Path('D:/UnReal/TroubleSquad/ArtSource/AudioArcade')/(row['asset']+'.wav'));t.destination_path='/Game/TroubleSquad/Audio/'+str(rel.parent).replace('\\','/');t.destination_name=rel.name;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
tools.import_asset_tasks(tasks)
for row in rows:assert unreal.load_asset('/Game/TroubleSquad/Audio/'+row['asset']),row['asset']
unreal.log('SOUND_MOTION_AUDIO_IMPORTED '+str(len(rows)))
