import bpy,math
from mathutils import Vector
bpy.ops.wm.open_mainfile(filepath='D:/UnReal/TroubleSquad/ArtSource/HealthPack/HealthPack.blend')
bpy.ops.object.camera_add(location=(1.1,-1.6,1.4));c=bpy.context.object;c.rotation_euler=(Vector((0,0,.25))-c.location).to_track_quat('-Z','Y').to_euler();c.data.type='ORTHO';c.data.ortho_scale=1.3;bpy.context.scene.camera=c
for loc,power in [((1,-2,3),400),((-2,-1,2),250)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=3;l.rotation_euler=(-l.location).to_track_quat('-Z','Y').to_euler()
s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=16;s.render.resolution_x=480;s.render.resolution_y=480;s.world.color=(.04,.04,.05);s.render.filepath='D:/UnReal/TroubleSquad/ArtSource/HealthPack/Preview.png';bpy.ops.render.render(write_still=True)
