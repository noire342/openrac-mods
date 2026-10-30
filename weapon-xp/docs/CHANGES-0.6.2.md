# 0.6.2

- Fixed the flipped Morph-o-Ray emitting from the original barrel position. The
  model roll now precedes the native muzzle-joint query instead of following it.
- The original native update now builds the ray, muzzle sparks and light from the
  same pose as the visible barrel. The local firing axis stays unchanged.
- Restore the previous owned pose before every update, including toggling off;
  repeated updates cannot compound the roll.
- Added both-disc DLL regression coverage of the native joint-query order, muzzle
  position, unchanged forward axis and target ownership, and restoration when off.
- Native NTSC QA compares the sampled muzzle joint with the actual beam origin
  and first beam node, alongside native transformation and on/off checks.

Settings, API layouts and XP save format remain compatible with 0.6.1.
