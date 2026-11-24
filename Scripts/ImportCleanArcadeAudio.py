raise SystemExit("Original game-bank pipeline is active. Use BuildOriginalSwarmAudio.py / ImportOriginalSwarmAudio.py")
import unreal,json,hashlib
from pathlib import Path
root=Path('D:/UnReal/TroubleSquad');rows=json.loads((root/'ArtSource/CleanArcadeAudio/Manifest.json').read_text());tasks=[]
assert len(rows)==193
for row in rows:
 source=root/'ArtSource/AudioArcade'/(row['asset']+'.wav');assert hashlib.sha256(source.read_bytes()).hexdigest()==row['sha256']
 assert row['recorded_audio_inputs']==0 and row['human_voice_inputs']==0
 rel=Path(row['asset']);t=unreal.AssetImportTask();t.filename=str(source);t.destination_path='/Game/TroubleSquad/Audio/'+rel.parent.as_posix();t.destination_name=rel.name;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for row in rows:
 obj=unreal.load_asset('/Game/TroubleSquad/Audio/'+row['asset']);assert obj,row['asset']
 assert abs(obj.get_editor_property('duration')-row['duration'])<.05,row['asset']
unreal.log('CLEAN_AUDIO_IMPORTED 193 recording_inputs=0 voice_inputs=0')
