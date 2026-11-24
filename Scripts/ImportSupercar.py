import unreal
source='/Game/Cartoon_City_Free/Meshes/Cars/SM_Futuristic_Car_1'
target='/Game/TroubleSquad/CombatVisuals/SM_Supercar'
asset=unreal.load_asset(target)
if not asset:
    asset=unreal.EditorAssetLibrary.duplicate_asset(source,target)
assert asset, 'Supercar source mesh not found'
for slot,name in {1:'M_SummonArmor',2:'M_SummonVisor',3:'M_SummonCyan',4:'M_SummonPink'}.items():
    material=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/'+name)
    assert material, name
    asset.set_material(slot,material)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
unreal.log('SUPERCAR_ASSET_READY')
