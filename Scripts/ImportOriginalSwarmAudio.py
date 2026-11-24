import unreal,json,hashlib
from pathlib import Path
root=Path('D:/UnReal/TroubleSquad');rows=json.loads((root/'ArtSource/OriginalSwarmAudio/Manifest.json').read_text());tools=unreal.AssetToolsHelpers.get_asset_tools()
for start in range(0,len(rows),32):
 tasks=[]
 for row in rows[start:start+32]:
  src=root/'ArtSource/AudioArcade'/(row['asset']+'.wav');assert hashlib.sha256(src.read_bytes()).hexdigest()==row['sha256']
  assert not row['mixed_video_audio'] and not row['voice_bank']
  rel=Path(row['asset']);t=unreal.AssetImportTask();t.filename=str(src);t.destination_path='/Game/TroubleSquad/Audio/'+rel.parent.as_posix();t.destination_name=rel.name;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
 tools.import_asset_tasks(tasks)
for row in rows:
 obj=unreal.load_asset('/Game/TroubleSquad/Audio/'+row['asset']);assert obj,row['asset'];assert abs(obj.get_editor_property('duration')-row['duration'])<.05,row['asset']
 unreal.EditorAssetLibrary.set_metadata_tag(obj,'TroubleSquadAudioSource',row['event'] or 'Intentionally silent: no original contact/victim hit event')
 unreal.EditorAssetLibrary.save_loaded_asset(obj)
unreal.log('ORIGINAL_SWARM_AUDIO_IMPORTED '+str(len(rows))+' mixed_video=0 voice_banks=0')
