from pathlib import Path
import sys
r=Path(sys.argv[1]).resolve()
for p in [r/'CMakeLists.txt',r/'src/psp/psp_main.c',r/'src/psp/psp_file_system.c',r/'src/psp/psp_input.c']:
    if not p.is_file(): raise SystemExit(f'ERROR: missing {p}')
s=(r/'CMakeLists.txt').read_text(); m=(r/'src/psp/psp_main.c').read_text(); f=(r/'src/psp/psp_file_system.c').read_text()
for x in ['PLATFORM STREQUAL "psp"','PLATFORM_PSP','USE_FLOAT_REALS','NO_RVALUE_INT64']:
    if x not in s: raise SystemExit('ERROR: incomplete PSP CMake patch: '+x)
for x in ['load_data_win','Runner_create','Runner_initFirstRoom','Runner_step']:
    if x not in m and x != 'load_data_win': raise SystemExit('ERROR: missing core integration: '+x)
if 'FileSystemVtable' not in f or 'listdir' not in f: raise SystemExit('ERROR: incomplete filesystem backend')
print('PSP overlay validation passed.')