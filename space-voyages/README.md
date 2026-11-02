# Space Voyages 0.8.0

Free interplanetary flight for Ratchet & Clank 1 / OpenRAC, by noire342.
This is the established space-flight baseline, published separately from later
planet-atmosphere experiments. It preserves the native Gemlik flight controls.

After the original ship boarding and departure, fly between 18 destinations.
The mod adds radar, 176 asteroids in eight fields, ship collision damage,
engine flames, original Gemlik audio, and lasers on the orange and purple ships.
Available destinations retain campaign unlock rules. Triangle starts the
approach and original loading/landing sequence. Green and Original are unarmed.

## Requirements and installation

- Windows x64; PAL Europe v2.00 / SCES_509.16 is the tested reference.
- A separately built OpenRAC 0.10.0 **SDK 1.14-02 bridge**. Official 0.10.0 alone
  cannot run the mod; [bridge source and exact base](engine/README.md) are included.
- Locally generated scene resources from your own disc. No ISO, extracted art,
  PCM audio, game executable, saves or memory cards are supplied.

Build the isolated client first. Generate a full-galaxy resource pack using
[ASSETS.md](docs/ASSETS.md), [ASTEROIDS.md](docs/ASTEROIDS.md),
[SPACE_COMBAT.md](docs/SPACE_COMBAT.md) and [AUDIO.md](docs/AUDIO.md).
The ordinary ships do not require the optional Veldin Garage/Original provider.
The tested local asset pack was `pal-flightfx-07-final`; copyrighted resources
and its Original cockpit capture remain private, so that optional integration
is not an immediately reproducible public download.

Add [noire342/openrac-mods](https://github.com/noire342/openrac-mods) in the
launcher's Mods page, or import `space-voyages-0.8.0-windows-x86_64.zip`.
Set **Locally prepared scene assets** (`assets_directory`) to your pack,
enable the mod and board a ship normally. A missing bridge or invalid pack
produces a clear load failure. Keep your habitual client and saves separate.

## Controls

- Left stick / D-pad: original Gemlik steering; pull back to climb by default.
- Cross: boost.
- Square or L1: lasers on the orange and purple ships.
- Triangle: enter the nearby unlocked destination.
- Start: pause/resume.

`direct_vertical` optionally makes up climb. Asteroids, contacts, damage,
engine flames and weapons have separate settings. Input mappings come from
OpenRAC. Gameplay and mixer volumes use its normal settings.

## Build, API and validation

```powershell
$spaceSdk = (Resolve-Path ./support/sdk-1.14/cmake).Path
cmake -S . -B build -A x64 -DOpenRACMod_DIR="$spaceSdk"
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Output: `build/package/space_voyages`. The SDK is frozen source with its MIT
notice. The mod is GPL-3.0-or-later; explicitly licensed API headers keep their
own notices. See [API](docs/API.md), [custom destinations](docs/CUSTOM_DESTINATIONS.md),
[release evidence and limits](validation/RELEASE.md), and the retained upstream
OpenRAC/rac1-decomp credits. The original game content is not relicensed.

The release DLL is the unchanged user-tested `a891d74d...` baseline. Its
corresponding production sources match every hash in the original 40-suite
source/build receipt. CPU/CI tests and prior scoped native observations are
reported separately. Full campaigns, current NTSC integration, arbitrary mod
combinations and a cold Home -> space -> campaign -> Home Garage loop remain
unverified. Optional Garage APIs are included as consumers, not as Garage content.