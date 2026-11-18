import unreal
from pathlib import Path
root='/Game/TroubleSquad/CombatVisuals'
source=Path(__file__).resolve().parents[1]/'ArtSource'/'CombatSummons'
tools=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary
palette={
 'SummonArmor':((.24,.045,.38),.55,False), 'SummonTrim':((.025,.065,.13),.6,False),
 'SummonMetal':((.34,.46,.62),.7,False), 'SummonVisor':((.008,.012,.035),.3,False),
 'SummonCyan':((.03,.75,1.5),0,True), 'SummonPink':((1.4,.045,.4),0,True),
 'SummonGold':((1,.48,.06),.6,False)
}
materials={}
for name,(color,metal,glow) in palette.items():
    m=unreal.load_asset(root+'/M_'+name)
    if not m:m=tools.create_asset('M_'+name,root,unreal.Material,unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT if glow else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    c=edit.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-400,0)
    c.set_editor_property('constant',unreal.LinearColor(*color,1))
    assert edit.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if glow else unreal.MaterialProperty.MP_BASE_COLOR)
    if not glow:
        for prop,value,y in [(unreal.MaterialProperty.MP_METALLIC,metal,150),(unreal.MaterialProperty.MP_ROUGHNESS,.36,250)]:
            n=edit.create_material_expression(m,unreal.MaterialExpressionConstant,-400,y);n.set_editor_property('r',value)
            edit.connect_material_property(n,'',prop)
    edit.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m);materials[name]=m
for name in ['SM_RobotTibber','SM_YuumiDrone']:
    options=unreal.FbxImportUI()
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_materials',False)
    options.set_editor_property('import_textures',False)
    data=options.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes',True)
    data.set_editor_property('auto_generate_collision',False)
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(source/(name+'.fbx')))
    task.set_editor_property('destination_path',root)
    task.set_editor_property('destination_name',name)
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',True)
    task.set_editor_property('save',True)
    task.set_editor_property('options',options)
    task.set_editor_property('factory',unreal.FbxFactory())
    tools.import_asset_tasks([task])
    task.get_objects()  # Wait for Interchange before applying material assignments.
    mesh=unreal.load_asset(root+'/'+name)
    assert mesh,name
    height=mesh.get_bounds().box_extent.z*2
    if height<20 or height>600:
        data.set_editor_property('import_uniform_scale',100 if height<20 else .01)
        tools.import_asset_tasks([task]);mesh=unreal.load_asset(root+'/'+name)
        height=mesh.get_bounds().box_extent.z*2
    assert 50<height<300,(name,height)
    slots=mesh.get_editor_property('static_materials')
    for index,slot in enumerate(slots):
        key=str(slot.get_editor_property('imported_material_slot_name')).split('.')[0]
        assert key in materials,(name,key)
        mesh.set_material(index,materials[key])
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
    unreal.log('SUMMON_IMPORTED '+name+' height='+str(height)+' slots='+str(len(slots)))
