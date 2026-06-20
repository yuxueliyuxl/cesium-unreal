// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#if WITH_EDITOR

#include "Cesium3DTileset.h"
#include "Misc/AutomationTest.h"
#include "RuntimeNanite/CesiumRuntimeNaniteEligibility.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumRuntimeNaniteEligibility,
    "Cesium.Unit.RuntimeNanite.Eligibility",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ProductFilter);

bool FCesiumRuntimeNaniteEligibility::RunTest(const FString&) {
  FCesiumRuntimeNaniteEligibilityInput Input;
  Input.bActorEnabled = true;
  Input.bGlobalEnabled = true;
  Input.bPlatformSupported = true;
  Input.bIsTriangleList = true;
  Input.bHasValidPositions = true;
  Input.bHasValidIndices = true;
  Input.bHasValidNormals = true;
  Input.TriangleCount = 2000;
  Input.MinimumTriangleCount = 2000;
  Input.NumTextureCoordinates = 1;

  const auto ExpectReason =
      [this, &Input](
          const TCHAR* What,
          ECesiumRuntimeNaniteFallbackReason Expected) {
        const FCesiumRuntimeNaniteEligibilityResult Result =
            EvaluateCesiumRuntimeNaniteEligibility(Input);
        TestFalse(What, Result.bEligible);
        TestEqual(TEXT("Fallback reason"), Result.FallbackReason, Expected);
      };

  Input.bActorEnabled = false;
  ExpectReason(
      TEXT("Actor disabled is ineligible"),
      ECesiumRuntimeNaniteFallbackReason::DisabledByActor);
  Input.bActorEnabled = true;

  Input.bGlobalEnabled = false;
  ExpectReason(
      TEXT("Global switch disabled is ineligible"),
      ECesiumRuntimeNaniteFallbackReason::DisabledGlobally);
  Input.bGlobalEnabled = true;

  Input.bPlatformSupported = false;
  ExpectReason(
      TEXT("Unsupported platform is ineligible"),
      ECesiumRuntimeNaniteFallbackReason::UnsupportedPlatform);
  Input.bPlatformSupported = true;

  Input.bIsTriangleList = false;
  ExpectReason(
      TEXT("Unsupported primitive mode is ineligible"),
      ECesiumRuntimeNaniteFallbackReason::UnsupportedPrimitiveMode);
  Input.bIsTriangleList = true;

  Input.bIsTranslucent = true;
  ExpectReason(
      TEXT("Translucent material is ineligible"),
      ECesiumRuntimeNaniteFallbackReason::TranslucentMaterial);
  Input.bIsTranslucent = false;

  Input.bHasValidPositions = false;
  ExpectReason(
      TEXT("Invalid positions are ineligible"),
      ECesiumRuntimeNaniteFallbackReason::InvalidPositions);
  Input.bHasValidPositions = true;

  Input.bHasValidIndices = false;
  ExpectReason(
      TEXT("Invalid indices are ineligible"),
      ECesiumRuntimeNaniteFallbackReason::InvalidIndices);
  Input.bHasValidIndices = true;

  Input.bHasValidNormals = false;
  ExpectReason(
      TEXT("Invalid normals are ineligible"),
      ECesiumRuntimeNaniteFallbackReason::InvalidNormals);
  Input.bHasValidNormals = true;

  Input.TriangleCount = 1999;
  ExpectReason(
      TEXT("Mesh below triangle threshold is ineligible"),
      ECesiumRuntimeNaniteFallbackReason::BelowTriangleThreshold);
  Input.TriangleCount = 2000;

  Input.NumTextureCoordinates = Input.MaximumTextureCoordinates + 1;
  ExpectReason(
      TEXT("Too many texture coordinate sets are ineligible"),
      ECesiumRuntimeNaniteFallbackReason::TooManyTextureCoordinates);
  Input.NumTextureCoordinates = 1;

  const FCesiumRuntimeNaniteEligibilityResult Result =
      EvaluateCesiumRuntimeNaniteEligibility(Input);
  TestTrue(TEXT("Valid input is eligible"), Result.bEligible);
  TestEqual(
      TEXT("Eligible input has no fallback"),
      Result.FallbackReason,
      ECesiumRuntimeNaniteFallbackReason::None);
  return true;
}

#endif
