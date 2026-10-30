#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Check the ELF service ABI without creating a native game session."""
import ctypes
import json
from pathlib import Path
import subprocess
import sys

library = Path(sys.argv[1]).resolve(strict=True)
symbols = subprocess.check_output(['nm', '-D', '--defined-only', str(library)], text=True)
names = {line.split()[-1] for line in symbols.splitlines()}
expected = {'openrac_mod_query', 'openrac_mod_load', 'openrac_mod_unload',
            'openrac_revolverator_query'}
if names != expected:
    raise RuntimeError(f'Unexpected dynamic exports: {sorted(names)}')
provider = ctypes.CDLL(str(library))
query = provider.openrac_revolverator_query
query.argtypes = [ctypes.c_uint32]
query.restype = ctypes.c_void_p
if any(query(version) is not None for version in [0, 1, 2, 0xffffffff]):
    raise RuntimeError('The service must be unavailable without a loaded session')

class ModInfo(ctypes.Structure):
    _fields_ = [('size', ctypes.c_uint32), ('api_major', ctypes.c_uint32),
                ('api_minor', ctypes.c_uint32), ('id', ctypes.c_char_p),
                ('version', ctypes.c_char_p)]

info = ModInfo(size=ctypes.sizeof(ModInfo))
provider.openrac_mod_query.argtypes = [ctypes.POINTER(ModInfo)]
provider.openrac_mod_query.restype = None
provider.openrac_mod_query(ctypes.byref(info))
if (info.id, info.api_major, info.api_minor) != (b'revolverator', 1, 1):
    raise RuntimeError('Unexpected OpenRAC loader identity or API version')
print(json.dumps({'scope': 'ELF loader; no game session', 'exports': sorted(names),
    'query_without_session': 'null', 'id': info.id.decode(),
    'version': info.version.decode(), 'openrac_api': [info.api_major, info.api_minor]}, indent=2))
