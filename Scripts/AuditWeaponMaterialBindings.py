import unreal,json
r=[]
for n in ['Train','Boomerang','Crescent','Blade','Tibber','Crossbow','Cyclone']:
 a=unreal.load_asset('/Game/TroubleSquad/CombatVisuals/GeneralWeapons/Models/SM_GW_'+n)
 r.append({'name':n,'materials':[str(x.get_editor_property('material_interface')) for x in a.get_editor_property('static_materials')]})
open('D:/UnReal/TroubleSquad/Saved/Logs/WeaponMaterialAudit.json','w').write(json.dumps(r,indent=2))
