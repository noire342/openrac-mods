# OpenRAC SDK 1.14-02 bridge source

Space Voyages needs the transient scene/audio extension (API 1.13 minimum).
The tested PAL baseline also uses the SDK 1.14-02 destination registry.
These services are absent from official OpenRAC 0.10.0. Installing the DLL in
the stock client alone does not provide free flight.

The cumulative text patch reconstructs OpenRAC commit
`6fdf807afa4f0d18bf9f6e246e0497047f975949` from upstream launcher-v0.10.0 commit
`d015ea1c6925de90b4be91ddb349af83361d9d4b`. Use a separate clean checkout:

```powershell
git checkout d015ea1c6925de90b4be91ddb349af83361d9d4b
git apply --check <absolute-path-to>/openrac-0.10.0-space-transit-sdk114.patch
git apply <absolute-path-to>/openrac-0.10.0-space-transit-sdk114.patch
```

Follow upstream OpenRAC build instructions with your own disc data. The
manifest records the patch, exact target tree and previously tested PAL binary
hash. No runtime executable or extracted game content is distributed here.
This release includes no SDK 1.15/1.16 or atmospheric-flight experiment.

Retain OpenRAC and rac1-decomp file-level GPL notices and upstream credits.