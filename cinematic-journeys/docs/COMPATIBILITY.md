# OpenRAC 0.10.0 compatibility

Reference: official release `launcher-v0.10.0`, commit
`d015ea1c6925de90b4be91ddb349af83361d9d4b`.
Windows NTSC-U executable SHA-256:
`f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b`.
The test executable comes from the official MSI, extracted without installing over
the player's client. No engine patch is required.

The standalone build pins SDK **1.10.1**, SHA-256
`5d7455a9c0e7bb6067ee514565cbce9abf12e132f9eb5053d8d3618c16084470`.
The DLL still requests **API 1.1** and uses only that prefix; the official client
reports **API 1.10**. Loading checks structure size and all services the mod uses.
The repository CI also checks the SDK 1.3 baseline. SDK compatibility and host tests
alone do not establish native gameplay.

## Reproduce

From this module's directory:

```sh
python tools/fetch_sdk.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
python tools/package_delivery.py --build build --output dist
```

The packager reads the built manifest and version. It takes no personal QA folder,
profile or game file as an input. `--validation receipt.json` optionally includes
an existing factual receipt; packaging never invents native results. Use a fresh
output folder for each run. The source ZIP can build independently of the monorepo.

In the full repository, `tools/run_standalone_native.ps1` takes an explicit official
EXE, the player's extracted data, a build with the QA fixtures enabled and a new
output directory. It uses isolated cards, settings and LOCALAPPDATA. The fixtures
must never be distributed in the installable mod. Its screenshots and logs are
evidence of the requested scenario, not an exhaustive campaign test.

## Cinematic Journeys scope

NTSC-U native tests cover accidental Start filtering, deliberate full-chord skip
and an ordinary planet 2 → 3 travel fixture. PAL policy/runtime adapters are
host-tested; PAL gameplay and every story scene have not been tested natively.
The fixture asks the original game for travel; it does not prove manual map input.

Pad reports may live in the port's mapped guest arena at 0xE0000000–0xE0FFFFFF,
including translated C stack locals, as well as the PS2 RAM. Input validation
accepts those mapped regions and rejects holes and overflowing ranges. Scene and
flight globals remain bounded to PS2 RAM. The injected clock keeps timing tests
independent of the machine's speed.

L1+L2+R1+R2+Start remains the deliberate skip. The five-second minimum is a lower
bound: the current original animation pass completes, and slow loading can take
longer. The final pass of special story arrivals remains the game's own.
`--skip-movies` and synchronized co-op `play_skip` calls intentionally bypass pad
filtering; they are not accidental button presses the mod can block. Run without
`--skip-movies` when checking the movie cases.
