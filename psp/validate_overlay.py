from pathlib import Path
import sys

r = Path(sys.argv[1]).resolve()
required = [
    r / "CMakeLists.txt",
    r / "src/runner.h",
    r / "src/runner.c",
    r / "src/loop.c",
    r / "src/psp/psp_main.c",
    r / "src/psp/psp_file_system.c",
    r / "src/psp/psp_input.c",
    r / "src/psp/psp_renderer.c",
]
for p in required:
    if not p.is_file():
        raise SystemExit(f"ERROR: missing {p}")

s = (r / "CMakeLists.txt").read_text()
h = (r / "src/runner.h").read_text()
m = (r / "src/psp/psp_main.c").read_text()
f = (r / "src/psp/psp_file_system.c").read_text()
loop = (r / "src/loop.c").read_text()

for x in ['PLATFORM STREQUAL "psp"', 'PLATFORM_PSP', 'USE_FLOAT_REALS', 'NO_RVALUE_INT64']:
    if x not in s:
        raise SystemExit('ERROR: incomplete PSP CMake patch: ' + x)

for x in ['Runner_create', 'Runner_reset']:
    if x not in h:
        raise SystemExit('ERROR: missing core API: ' + x)

# PSP main only needs to drive the already-created Runner; construction lives
# in the platform-neutral runner/loop layer.
for x in ['Runner_step', 'Runner']:
    if x not in m:
        raise SystemExit('ERROR: missing PSP main integration: ' + x)

for x in ['FileSystemVtable', 'listdir']:
    if x not in f:
        raise SystemExit('ERROR: incomplete filesystem backend: ' + x)

for x in ['PLATFORM_PSP', 'PSPRenderer_create']:
    if x not in loop:
        raise SystemExit('ERROR: missing PSP renderer integration: ' + x)

print('PSP overlay validation passed.')