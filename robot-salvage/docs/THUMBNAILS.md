# Component miniature deployment

The inventory renders the component's actual textured mesh from the player's
own extracted disc. No game models, textures or pre-rendered game images are
included in the mod package. Generated indexed miniatures are cached locally
with content fingerprints; changing an owned source mesh invalidates its cache.

## Finding the owned models

The default constructor follows OpenRAC's content path rules:

1. An explicit `--levels <directory>` wins.
2. Otherwise `--data <directory>` supplies `<directory>/port/<version>`.
3. Without `--data`, `OPENRAC_DATA` supplies the same data directory.
4. Without any of these, no content directory is guessed. The official runtime
   itself rejects an invocation with neither `--data` nor `OPENRAC_DATA`.

`<version>` comes from `openrac_api.version` (`pal` or `ntsc`). Like the official
`levels_or_flat` helper, an old PAL export is read from the parent directory
when its `level_99/manifest.json` exists there and not under `pal`. Current PAL
exports take precedence. NTSC never falls back to the PAL directory. Paths are
read from the wide Windows command line and environment, preserving Unicode.

The explicit constructor used by isolated adapters/tests can still provide an
exact levels directory directly. Nothing searches other installations, rewrites
launcher settings or copies assets into the mod package.

## Official launcher audit

The installed 0.10 launcher's `openrac/launcher/actions.json`, PAL action `play`,
passes `--data {data} --window --levels {data}/port/pal`. The source checkout
action passes its corresponding extracted directory, and `play-edited` also
names `--levels`. Real launcher log entries confirm `--levels` on official NTSC
launches with mods and plugins enabled. Normal launcher use therefore already
provides the required directory; the fallback above covers valid direct CLI
invocations too.

Source evidence: OpenRAC `port/game/common/program/options.cpp`, functions
`parse`, `default_levels` and `levels_or_flat`; `port/common/env.cpp`,
`Var::Data`; launcher `actions.json` actions `play`/`play-edited`; and
`launcher/core/src/library.rs::play_waits_for_the_levels_it_draws`.

The focused thumbnail tests cover directory precedence, both regions, Unicode,
legacy layouts and missing roots. Host checks establish path resolution and
rendering contracts, not additional native gameplay coverage.
