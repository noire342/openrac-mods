# Changelog

## Unreleased source changes

- Make the native memory helper templates available to every translation unit,
  fixing GNU linker errors in the Linux source build. The published Windows
  0.6.1 artifact and its source ZIP remain unchanged.
- Build/test on Windows and Ubuntu 24.04 using current GitHub Actions runtimes.

## Weapon Experience 0.6.1

- Ten persistent levels for twelve weapons, with progression-only defaults.
- Native HUD and acquisition presentation; configurable per-weapon rules.
- Public C APIs v1, v2 and v3, with existing layouts preserved.
- Independent opt-in Morph-o-Ray model-pose toggle; native targeting and
  transformation behavior remain in control.

See [the full 0.6.1 changes](weapon-xp/docs/CHANGES-0.6.1.md).

## Revolverator — unreleased

Capture/spin/throw reconstruction, native UI integration, an AI-assisted
ammunition model and vendor/ammunition integration are under development.
See [current status](docs/REVOLVERATOR.md).
