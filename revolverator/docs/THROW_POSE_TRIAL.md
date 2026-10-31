# Throw spine gesture trial

The enemy release adds a small forward bend across Ratchet's three lower spine
joints (3, 4, 5). The bend reaches 12 degrees about 100 ms into the existing
250 ms arm follow-through, then smoothly returns to the authored pose. It is
inactive while carrying an enemy. The pelvis/root and leg joints are untouched.
The arm solves after the spine so its shoulder and held weapon follow the body.

The experiment lives in `throw_lean()` / `throw_lean_radians` in `arm_math` and
`NativeArm::pose_torso()`. Setting `throw_lean_radians` to zero and rebuilding
removes this addition independently of the arm gesture and release controller.

The isolated Windows NTSC OpenRAC 0.10 check completed a real base-weapon throw,
earned 39 XP, then switched to the native Walloper and earned its separate 39 XP.
The exact official renderer pose evaluator, evaluated against the same live
frame with the additive spine rotations neutralized, measured an 8.18 cm peak
forward head displacement and zero pelvis displacement. All three spine
modifiers were removed on weapon switch; none bent the spine during carrying.
Six focused host test groups passed. See
[`validation/throw-lean-0.3.1.json`](validation/throw-lean-0.3.1.json).

This is a local gesture trial awaiting the user's visual preference. Previous
Golden shop, acquisition and save receipts describe the earlier C929 build;
they do not establish those paths for this new trial binary. No trial release
has been published from this change.
