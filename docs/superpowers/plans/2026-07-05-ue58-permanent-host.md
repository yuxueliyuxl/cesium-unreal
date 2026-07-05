# Permanent UE 5.8 Host Project Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a persistent UE 5.8 C++ host project linked to the UE 5.8 Cesium upgrade worktree.

**Architecture:** A minimal C++ Unreal project lives under `C:\tmp` and enables CesiumForUnreal. A directory junction exposes the upgrade worktree as the project's plugin, while host-generated files remain outside the plugin repository.

**Tech Stack:** Unreal Engine 5.8, Unreal Build Tool, C#, C++, Windows directory junctions

---

### Task 1: Scaffold the host

**Files:**
- Create: `C:\tmp\cesium-ue58-host\HostProject.uproject`
- Create: `C:\tmp\cesium-ue58-host\Source\HostProject.Target.cs`
- Create: `C:\tmp\cesium-ue58-host\Source\HostProjectEditor.Target.cs`
- Create: `C:\tmp\cesium-ue58-host\Source\HostProject\HostProject.Build.cs`
- Create: `C:\tmp\cesium-ue58-host\Source\HostProject\HostProject.cpp`

- [ ] Verify the target directory does not already contain user files.
- [ ] Create a minimal UE 5.8 project descriptor, targets, and primary game module.
- [ ] Create `Plugins\CesiumForUnreal` as a junction to the upgrade worktree.
- [ ] Confirm the junction resolves to the expected absolute path.

### Task 2: Compile and verify

**Files:**
- Test: `C:\tmp\cesium-ue58-host\HostProject.uproject`

- [ ] Run UE 5.8 `Build.bat HostProjectEditor Win64 Development`.
- [ ] Confirm Unreal Build Tool exits with code 0.
- [ ] Launch `UnrealEditor-Cmd` with `-NullRHI` and quit after initialization.
- [ ] Confirm logs show CesiumRuntime/CesiumEditor load without a compatibility prompt.
- [ ] Confirm the permanent project and junction still exist.
