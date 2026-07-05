# Permanent UE 5.8 Host Project Design

## Goal

Create a persistent Unreal Engine 5.8 host project for developing and testing
the customized Cesium for Unreal plugin.

## Structure

The project will live at `C:\tmp\cesium-ue58-host`. It will contain a minimal
game module plus Game and Editor targets using UE 5.8 build settings. Its
`Plugins\CesiumForUnreal` directory will be a Windows directory junction to
`E:\UnrealProjects\cesium-unreal\.worktrees\ue58-upgrade`, so source changes on
the upgrade branch participate in the next host build without copying files.

The project descriptor will use EngineAssociation `5.8` and explicitly enable
CesiumForUnreal. Generated binaries, intermediates, and project files remain in
the host directory and are not committed to the plugin repository.

## Verification

Build `HostProjectEditor Win64 Development` with UE 5.8, then start the editor
commandlet far enough to confirm that CesiumRuntime and CesiumEditor load
without an engine-version compatibility prompt. The host is considered ready
when the build exits successfully and the project and plugin junction remain
present after verification.
