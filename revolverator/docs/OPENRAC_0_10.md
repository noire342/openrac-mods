# Revolverator 0.3.1 and OpenRAC 0.10

The current Windows preview targets the official OpenRAC 0.10 NTSC-U client.
Import the native mod ZIP in the launcher's Mods page. It includes the weapon
assets and private compatibility libraries; no replacement client is bundled.
After upgrading OpenRAC, use **Check and play** to read the levels again if
the mod reports an outdated extraction.

## Verified client and assets

The installed NTSC-U executable is byte-identical to the executable extracted
from the official 0.10 installer. Its SHA256 is
`f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b`.
The installer SHA256 is
`fde756d9c789f062a84c9951fb809303982cefda83e0c848245302061b8e9e29`.
The [official release source](https://github.com/OpenRAC/OpenRAC/commit/d015ea1c6925de90b4be91ddb349af83361d9d4b)
is commit `d015ea1c6925de90b4be91ddb349af83361d9d4b`.

An older installed `openrac-build.json` referring to 0.9/`3ae4f359` was a
leftover: the official 0.10 installer does not carry that file. It does not
identify the newer executable. The executable/installer comparison above is
the provenance check used for these tests.

`NativeAssetBundle` accepts and preserves manifest format 3 or 4. On a runtime
reporting API 1.10 or newer, integration requires format 4 before mounting any
replacement manifests. It stages copies in its own cache and preserves the
original level files. The earlier 0.9 executable hash `b7f3762a…df94081` remains
recognized; the final 0.3.1 native receipts below exercise 0.10.

## SDK compatibility

MSVC x64 layout comparisons confirm that SDK 1.10 retains every offset in the
SDK 1.3 table prefix used by this mod. The full API table grows from 200 to 672
bytes; `openrac_input` stays 560 bytes and `openrac_mod_info` stays 32 bytes.
The mod still declares ABI 1.1 and the same 27 public hook IDs/signatures.
None of its 30 private compatibility hook IDs has become a public SDK hook,
so the bundled compatibility provider remains required.

The 0.10 host fixes NTSC held-item and preview pose outputs at moby+0x68/+0x6C.
The guest Moby layout and joint evaluator used by the arm/rotor retain their
layout. Native scene rendering now also applies the custom item pose after
asynchronous loading: the scene rotor keeps animated root 0 and neutralizes
the original glove's finger joints. The original acquisition animation runs
with the Rotatore's current name and base/Gold model.

## Native evidence

The final-pulse DLL is
`c9294347cacad667bd9436feb6ac7c22196ddfff151c745dec8437d3554739f7`.
Seven targeted fixtures pass for purchase without XP, short/long Gold combat,
switching away during flight, a Gold level-up, save and fresh-process reload.
They exercise the centred, stationary, enlarged Gold display, native shop
lighting, purchase from 3.35 m and the Circle head pulse. Details, exact counters
and the separate rapid-reuse result are in [NATIVE_VALIDATION.md](NATIVE_VALIDATION.md).

This is Windows NTSC-U evidence on isolated Novalis fixtures. It does not
establish a complete campaign, every boss phase, PAL or native Linux/macOS.
