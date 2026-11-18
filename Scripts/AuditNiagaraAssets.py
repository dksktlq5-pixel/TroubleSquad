import unreal
for i in range(1,16):
    path='/Game/Basic_VFX/Niagara/NS_Basic_'+str(i)
    system=unreal.load_asset(path)
    if not system:continue
    if i==1:unreal.log('NIAGARA_METHODS '+str([n for n in dir(system) if 'parameter' in n.lower()]))
    try:
        params=system.get_editor_property('exposed_parameters')
        unreal.log('NIAGARA_ASSET '+str(i)+' '+str(params)[:700])
    except Exception as e:unreal.log('NIAGARA_ASSET '+str(i)+' '+str(e)[:160])
