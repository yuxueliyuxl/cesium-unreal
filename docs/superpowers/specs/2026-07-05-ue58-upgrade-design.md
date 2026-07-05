# Unreal Engine 5.8 Upgrade Design

## Goal

Upgrade the customized Cesium for Unreal fork to compile and run with Unreal
Engine 5.8 while preserving its Runtime Nanite, Holo raster overlay, and
cross-compilation customizations.

## Scope

Port the source compatibility changes from upstream merge commit
`28c40e8d94cd537f04cb6e93412c9cc4d5ce3869`. This includes the UE 5.8 version
guard, renderer include changes, RHI texture API migrations, stricter logging
format fixes, and compile-warning fixes. GitHub Actions jobs and upstream
maintainer documentation are excluded because they do not affect the local
plugin build.

The `cesium-native` gitlink will not be replaced blindly. The existing forked
submodule and its local customization remain authoritative; only native
changes proven necessary by the UE 5.8 build will be integrated.

## Integration approach

Apply the selected upstream file changes in the isolated
`codex/ue58-upgrade` worktree. Where an upstream edit overlaps customized code,
preserve the customized behavior and make only the smallest UE 5.8-specific
change. Do not merge upstream `main` or rewrite the current local branch.

## Verification

Use the installed engine at `D:\Program Files\Epic Games\UE_5.8`. Build a host
Editor target containing this plugin, then run the relevant Cesium automation
tests. Compilation failures are handled systematically: reproduce, identify
whether the failure is an engine API change or a fork-specific interaction,
add a regression test where behavior is involved, and apply the smallest fix.

The upgrade succeeds when the UE 5.8 Editor target compiles, the selected
Cesium tests pass, and the main worktree's pre-existing uncommitted files are
unchanged.
