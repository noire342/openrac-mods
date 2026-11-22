# macOS validation

The OpenRAC 1.1.0 download includes the Ratchet & Clank game binaries for Apple silicon. The launcher app itself is universal. Use a `macos-arm64` package and mod interface 1.20 or newer for the bundled game. [OpenRAC installation guide](https://openrac.dev/install) · [OpenRAC 1.1.0 build matrix](https://github.com/OpenRAC/OpenRAC/blob/9b6573caf0dc708689be1783b5e4ac831118777b/.github/workflows/launcher.yml#L165-L176)

## Build and package

1. Build with `CMAKE_OSX_ARCHITECTURES=arm64` and `CMAKE_OSX_DEPLOYMENT_TARGET=12.0` to match the deployment target of the PAL and NTSC-U binaries in the official OpenRAC 1.1.0 archive. Run the CTest suite on a macOS runner.
2. Check the packaged library with `file`, `otool -L` and `codesign --verify`. Use `nm -gU` to verify the exported `openrac_mod_query`, `openrac_mod_load`, `openrac_mod_unload` and `openrac_revolverator_query` symbols.
3. Check that `openrac-mod.json` is at the archive root and that its `macos-arm64` entry resolves to `lib/macos-arm64/revolverator.dylib`. The release archive should carry the tools for preparing the weapon assets from the player's disc and the authored assets used by those tools.

## Stock game

1. Use the unmodified OpenRAC 1.1.0 launcher and game with an isolated install directory and test memory card. Add the ZIP in Mods, enable Revolverator, then start the game. [OpenRAC mod installation](https://openrac.dev/mods/docs/getting-started)
2. Check `<data>/plugins/status.json` for a `revolverator` entry in the `loaded` state. Record the disc version, mod interface, exact archive hash and game log. Repeat in separate PAL Europe v2.00 and NTSC-U sessions. [OpenRAC mod status](https://github.com/OpenRAC/OpenRAC/blob/9b6573caf0dc708689be1783b5e4ac831118777b/docs/port/MODS.md)
3. Start with cold asset caches and verify the first load and a restart. Inspect the standard and Golden held models, the equipped and selected pause-menu previews, Gadgetron's vendor row, and the purchase and acquisition scenes. Verify the native Walloper independently.
4. Buy or grant the Revolverator, use some ammunition, assign it to Quick Select and reload from the test memory card. Verify that the separate weapon state persists. Repeat with `--no-card` and confirm that the mod leaves the test card and companion data untouched.

The Mach-O inspection establishes the target architecture and binary interface. Record launcher admission and in-game results from the stock game on Apple silicon for runtime verification.
