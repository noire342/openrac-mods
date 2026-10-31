# Arsenal File Overlay 0.1.0 — private bundled utility

Read-only file-alias utility for the unchanged official OpenRAC Windows x64
NTSC-U executables: OpenRAC0.10.0 SHA256
`f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b`
and legacy0.9.0 SHA256
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
This prototype does not install assets, change the client, edit original
manifests or provide weapons. The caller stages replacement manifests/assets.

The utility redirects exactly 19 configured original manifest paths to existing
staged copies. Native `--levels` can keep pointing at the original extraction;
the staged JSON can refer to absolute mesh/texture paths owned by the mod.
All other files and all write/create/truncate/delete requests pass through.
No new game command-line option or SDK table extension is needed.
The consumer preserves the original manifest version and unknown fields.
OpenRAC0.10 requires freshly extracted format4 levels; a format3 extraction
is rejected before mounting, with an instruction to use the official launcher's
Check and play. The format number is never changed to disguise old data.

## Configuration contract

UTF-8 JSON, optionally with a BOM, at most 256KiB:

```json
{
  "format": 1,
  "aliases": [
    {
      "original": "C:/game/port/ntsc/1/manifest.json",
      "replacement": "C:/mod/staged/1/manifest.json"
    }
  ]
}
```

The example shows one record's shape; the real array must contain exactly 19.
Both paths must be absolute and refer to existing regular files. Relative
configuration paths, duplicate normalized originals, self-aliases, unknown
keys, malformed UTF-8/Unicode escapes and overlong strings are rejected.
Sources remain untouched. The map is immutable after successful installation.

Paths use `GetFullPathNameW` normalization, slash normalization and invariant
case folding. Extended DOS/UNC prefixes are normalized; device paths are
excluded. Only exact full paths match. There is no prefix, suffix, glob or
directory redirection, and replacement aliases are not recursively chained.

## Standalone C ABI

Load `arsenal_file_overlay.dll` from the caller's own DLL directory with an
appropriate `LoadLibraryExW` search policy. Resolve these C exports; OpenRAC's
`openrac_mod_load` is not required:

- `int openrac_file_overlay_install(const wchar_t* absolute_config_file)`
- `int openrac_file_overlay_remove()`
- `const char* openrac_file_overlay_last_error()`
- `int openrac_file_overlay_stats(file_overlay_stats_v1* caller_owned_out)`

The typedefs, result codes and stats layout are in
[include/file_overlay_api.h](include/file_overlay_api.h). Set the stats output's
`size`; version 1 reports alias count, installation state, redirected open
attempts and pass-through opens. `last_error` returns a thread-local copied
description, valid until that thread next calls it. Install/remove serialize
their lifecycle; the file hook never calls the SDK or logs.

The SDK wrapper is only a QA entry point: id `arsenal-file-overlay`, version
0.1.0, DLL `lib/windows-x86_64/arsenal_file_overlay.dll`. It locates
`file-overlay.json` in its own mod root and calls the same install export.
Load it before native level loading. The production consumer may instead own
this DLL privately, prepare its own config and call the standalone ABI.

## Read and teardown boundaries

The typed MinHook detour wraps the actual `KernelBase!CreateFileW` entry used
by UCRT API-set imports. The distinct Kernel32 wrapper is bypassed by this path. It redirects only
`OPEN_EXISTING` calls that request read data and no write/append/delete/execute
access, and excludes `FILE_FLAG_DELETE_ON_CLOSE`. Sharing, security, attributes,
template handles, returned handles and native `GetLastError` are preserved.
Worker threads read a shared immutable map; no SDK calls happen in the hook.
Normalization/allocation failures fall back to the original request.

Remove atomically clears the map, disables only the owned hook, switches late
callbacks to the restored native function and waits up to 1 second for tracked
calls before freeing the trampoline. A busy or failed removal retains the
disabled resources for retry. The utility DLL is process-pinned to keep late
worker callback code valid; normal removal still restores/removes its own
detour. No `MH_ALL_HOOKS` or `MH_Uninitialize` call affects another provider.

## Build and evidence

```sh
cmake -S . -B build -DOpenRACMod_DIR=/path/to/sdk/cmake -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Windows x64 builds `package/arsenal_file_overlay`. Linux builds only the portable
config/policy/map tests. The first host suite passes 1,865 checks, including every
truncated input prefix, strict Unicode/schema errors, count/duplicate checks,
exact lookup and denial of all tested write/disposition combinations.
The [native receipt](docs/NATIVE_VALIDATION.json) records a 2,400-frame stock
Novalis fixture with original `--levels`, no `--mods`, 345 models, the real drill
and cartridge visible, and 39 hashed input files unchanged. Other 18 aliases,
combat/save and full-campaign paths are not certified by that asset fixture. The current weapon package owns this utility privately. Its standalone SDK
wrapper remains a development entry point; it is not another launcher card.

Implementation: [GPL-3.0-or-later](LICENSE), Copyright (c) 2026 noire342.
The C interoperability header is MIT; see [the retained MIT text](docs/LICENSE-MIT.md).
Exact MinHook 1.3.4 source is copied unchanged from pinned upstream commit
`c3fcafdc10146beb5919319d0683e44e3c30d537`; its BSD/HDE notices and file hashes
remain in [third_party/minhook.lock.json](third_party/minhook.lock.json) and
[third_party/minhook/LICENSE.txt](third_party/minhook/LICENSE.txt).

QA diagnostics are opt-in with `OPENRAC_FILE_OVERLAY_DIAGNOSTICS=1`. Only the
SDK QA wrapper registers the frame logger. File callbacks collect bounded
manifest traces/counters; they never log or call SDK functions. Production
standalone use stays silent by default.
