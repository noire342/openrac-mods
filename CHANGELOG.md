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

## Revolverator 0.1.0 — source preview

- Publish source for capture/spin/throw, native ammunition/HUD, Hoven vendor,
  contact-based attachment and the separately controlled original drill tip.
- Add a session guest arena, five-language text, a copy-only C API and ten
  host test groups, plus local asset/bundle builders.
- Record native Windows NTSC-U checks and 19-level load smoke evidence, with
  explicit Rilgar HD fallback and Blarg Station pickup-text limitations.
- Publish no Revolverator binaries or game-derived art. Weapon Experience 0.6.1
  source and release archives remain unchanged by this preview publication.

See [current scope](docs/REVOLVERATOR.md).
