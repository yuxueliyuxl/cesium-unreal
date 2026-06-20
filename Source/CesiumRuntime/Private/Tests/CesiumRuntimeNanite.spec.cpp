// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#if WITH_EDITOR

#include "Cesium3DTileset.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumRuntimeNaniteDefaults,
    "Cesium.Unit.RuntimeNanite.Defaults",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ProductFilter);

bool FCesiumRuntimeNaniteDefaults::RunTest(const FString&) {
  const ACesium3DTileset* pTileset = GetDefault<ACesium3DTileset>();
  TestFalse(
      TEXT("Runtime Nanite is disabled by default"),
      pTileset->GetEnableRuntimeNanite());
  TestEqual(
      TEXT("Runtime Nanite defaults to 2000 triangles"),
      pTileset->GetRuntimeNaniteMinimumTriangleCount(),
      2000);
  return true;
}

#endif
