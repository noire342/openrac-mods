# Historical arsenal engine patch

**Historical reference only. The current preview uses the unchanged official
client plus [Arsenal Compatibility](../../arsenal-compat/README.md). Do not apply
this patch for current preview setup.**

An earlier development runtime added these30 typed native hooks on OpenRAC base
`77a2f05257c5dfc2ab9b1ec52a84fc78e8f64225`:
[openrac-arsenal-bridge.patch](openrac-arsenal-bridge.patch).
The exact IDs and signatures are in [hooks.json](hooks.json).

This folder contains source changes and documentation only. No executable,
retail ISO, extracted game art, original level manifest, RAM dump or save is
included. The bridge is GPL-scoped engine work; [COPYING](COPYING) and upstream
file-specific copyright/license notices remain applicable. The mod's separate
interop headers do not relicense the engine.

## Reproducing the historical development runtime

```sh
git clone https://github.com/OpenRAC/OpenRAC.git OpenRAC
cd OpenRAC
git checkout 77a2f05257c5dfc2ab9b1ec52a84fc78e8f64225
git apply /path/to/openrac-mods/docs/arsenal-bridge/openrac-arsenal-bridge.patch
python sdk/tools/gen_binding.py rac1
```

Follow that checkout's `docs/port` build instructions with your own NTSC-U disc.
Build `openrac-rac1-ntsc` into a separate directory and keep the official runtime
installation intact. No prebuilt bridge executable is supplied here.

The bridge exposes menu/preview, quick-select, vendor, ammo and save lifecycle
boundaries. It preserves original arguments/results and does not extend the
generic mod API ABI. Weapon ownership, class aliases, custom ammo, progression,
favourites and sidecar policy stay in the Revolverator plugin. PAL mappings are
provided for source consistency; this development combination has native Windows
NTSC-U evidence only.
