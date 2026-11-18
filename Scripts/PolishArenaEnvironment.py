"""Replace arena blockout decoration while retaining its gameplay collision proxies."""
import unreal
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rock = unreal.load_asset('/Game/WaterPlane/Environment/Props/SM_Rock')
bush = unreal.load_asset('/Game/Cartoon_City_Free/Meshes/Vegetation/SM_Bush_07')
tank = unreal.load_asset('/Game/ModSciInteriors/Meshes/SM_WVC_Tank_A')
case = unreal.load_asset('/Game/ModSci_EngiProps/Meshes/SM_Case_A')
assert rock and bush and tank and case
for arena in ['Outskirts', 'SubterraneanLab', 'Beachhead']:
    assert levels.load_level('/Game/TroubleSquad/Maps/L_' + arena)
    originals = list(actors.get_all_level_actors())
    existing = {actor.get_actor_label(): actor for actor in originals}
    count = 0
    for source in originals:
        label = source.get_actor_label()
        if label.startswith('Environment Art | '):
            continue
        shrub = 'shore scrub' in label or 'dune scrub' in label
        stone = label.startswith(('Boundary detail', 'Rock outcrop')) or any(word in label for word in ['shore outcrop', 'waterline rock', 'crater debris'])
        coolant = arena == 'SubterraneanLab' and label == 'Coolant tank'
        if not (shrub or stone or coolant):
            continue
        # Unreal allows duplicate actor labels, so the source object name identifies each placement.
        art_label = 'Environment Art | ' + source.get_name()
        mesh = tank if coolant else case if arena == 'SubterraneanLab' else bush if shrub else rock
        bounds = mesh.get_bounds()
        extent = bounds.box_extent
        _, target = source.get_actor_bounds(False)
        position = source.get_actor_location()
        width = max(target.x, target.y) * 2.0
        scale = width / max(extent.x * 2, extent.y * 2, 1)
        scale_z = max(target.z * 2, 20.0) / max(extent.z * 2, 1)
        # Bury lower rock faces slightly so they meet the existing ground naturally.
        ground = -12.0 if stone else 3.0
        location = unreal.Vector(position.x, position.y, ground - (bounds.origin.z - extent.z) * scale_z)
        art = existing.get(art_label)
        if art is None:
            art = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, source.get_actor_rotation())
        else:
            art.set_actor_location(location, False, False)
            art.set_actor_rotation(source.get_actor_rotation(), False)
        art.set_actor_label(art_label)
        component = art.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        art.set_actor_scale3d(unreal.Vector(scale, scale, scale_z))
        source.set_actor_hidden_in_game(True)
        existing[art_label] = art
        count += 1
    assert levels.save_current_level()
    unreal.log('ARENA_ENVIRONMENT_SAVED ' + arena + ' replaced=' + str(count))
