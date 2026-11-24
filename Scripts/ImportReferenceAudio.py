raise SystemExit("DISABLED: recording contains human speech; use BuildCleanArcadeAudio.py / ImportCleanArcadeAudio.py")
import unreal,json
from pathlib import Path
rows=json.loads(Path('D:/UnReal/TroubleSquad/ArtSource/ReferenceAudio/Manifest.json').read_text());tasks=[]
for row in rows:
 rel=Path(row['asset']);t=unreal.AssetImportTask();t.filename=str(Path('D:/UnReal/TroubleSquad/ArtSource/AudioArcade')/(row['asset']+'.wav'));t.destination_path='/Game/TroubleSquad/Audio/'+rel.parent.as_posix();t.destination_name=rel.name;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for row in rows:
 obj=unreal.load_asset('/Game/TroubleSquad/Audio/'+row['asset']);assert obj,row['asset']
 assert obj.get_editor_property('duration')>0,row['asset']
unreal.log('REFERENCE_AUDIO_IMPORTED '+str(len(rows)))
