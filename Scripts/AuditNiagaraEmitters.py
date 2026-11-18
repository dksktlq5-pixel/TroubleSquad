import unreal
s=unreal.load_asset('/Game/Basic_VFX/Niagara/NS_Basic_1')
try:
 hs=s.get_editor_property('emitter_handles')
 unreal.log('EMITTER_HANDLES '+str(hs)[:1800])
except Exception as e:unreal.log('EMITTER_QUERY '+str(e))
