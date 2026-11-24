import pathlib
root=pathlib.Path('D:/UnReal/TroubleSquad/Scripts')
for script in ['ImproveReferenceArenas.py','PolishArenaSurfaces.py','ImportReferenceArchitecture.py','PolishArenaReadability.py']:
 exec(compile((root/script).read_text(encoding='utf-8-sig'),str(root/script),'exec'),{})
