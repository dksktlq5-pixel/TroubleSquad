import bpy,json
p='D:/UnReal/TroubleSquad/ArtSource/CoastalResearch/ReferenceGroundDesign.blend'
with bpy.data.libraries.load(p,link=False) as (a,b):b.objects=[n for n in a.objects if 'ReferenceGroundDesign' in n]
for o in b.objects:
 if o and o.type=='MESH':
  vs=[o.matrix_world@v.co for v in o.data.vertices];print(o.name,[(min(v[k] for v in vs),max(v[k] for v in vs)) for k in range(3)])
