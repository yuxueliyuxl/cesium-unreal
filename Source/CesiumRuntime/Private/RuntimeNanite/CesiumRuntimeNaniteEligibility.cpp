// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#include "RuntimeNanite/CesiumRuntimeNaniteEligibility.h"

#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "RenderUtils.h"

namespace {

TAutoConsoleVariable<int32> CVarCesiumRuntimeNaniteEnable(
    TEXT("cesium.RuntimeNanite.Enable"),
    1,
    TEXT("Globally enables Cesium runtime Nanite when the tileset also enables it."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarCesiumRuntimeNaniteMinTriangles(
    TEXT("cesium.RuntimeNanite.MinTriangles"),
    -1,
    TEXT("Overrides the tileset runtime Nanite triangle threshold. Negative values use the tileset setting."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarCesiumRuntimeNaniteLogFallbacks(
    TEXT("cesium.RuntimeNanite.LogFallbacks"),
    0,
    TEXT("Logs expected Cesium runtime Nanite eligibility fallbacks."),
    ECVF_Default);

FCesiumRuntimeNaniteEligibilityResult Ineligible(
    ECesiumRuntimeNaniteFallbackReason Reason) {
  return FCesiumRuntimeNaniteEligibilityResult{false, Reason};
}

} // namespace

FCesiumRuntimeNaniteEligibilityResult
EvaluateCesiumRuntimeNaniteEligibility(
    const FCesiumRuntimeNaniteEligibilityInput& Input) {
  if (!Input.bActorEnabled) {
    return Ineligible(ECesiumRuntimeNaniteFallbackReason::DisabledByActor);
  }
  if (!Input.bGlobalEnabled) {
    return Ineligible(ECesiumRuntimeNaniteFallbackReason::DisabledGlobally);
  }
  if (!Input.bPlatformSupported) {
    return Ineligible(ECesiumRuntimeNaniteFallbackReason::UnsupportedPlatform);
  }
  if (!Input.bIsTriangleList) {
    return Ineligible(
        ECesiumRuntimeNaniteFallbackReason::UnsupportedPrimitiveMode);
  }
  if (Input.bIsTranslucent) {
    return Ineligible(
        ECesiumRuntimeNaniteFallbackReason::TranslucentMaterial);
  }
  if (!Input.bHasValidPositions) {
    return Ineligible(ECesiumRuntimeNaniteFallbackReason::InvalidPositions);
  }
  if (!Input.bHasValidIndices) {
    return Ineligible(ECesiumRuntimeNaniteFallbackReason::InvalidIndices);
  }
  if (!Input.bHasValidNormals) {
    return Ineligible(ECesiumRuntimeNaniteFallbackReason::InvalidNormals);
  }
  if (Input.TriangleCount < FMath::Max(0, Input.MinimumTriangleCount)) {
    return Ineligible(
        ECesiumRuntimeNaniteFallbackReason::BelowTriangleThreshold);
  }
  if (Input.NumTextureCoordinates > Input.MaximumTextureCoordinates) {
    return Ineligible(
        ECesiumRuntimeNaniteFallbackReason::TooManyTextureCoordinates);
  }

  return FCesiumRuntimeNaniteEligibilityResult{
      true,
      ECesiumRuntimeNaniteFallbackReason::None};
}

FCesiumRuntimeNaniteRuntimeSettings GetCesiumRuntimeNaniteRuntimeSettings(
    bool bActorEnabled,
    int32 ActorMinimumTriangles) {
  const int32 MinimumTrianglesOverride =
      CVarCesiumRuntimeNaniteMinTriangles.GetValueOnAnyThread();

  return FCesiumRuntimeNaniteRuntimeSettings{
      bActorEnabled,
      CVarCesiumRuntimeNaniteEnable.GetValueOnAnyThread() != 0,
      UseNanite(GMaxRHIShaderPlatform),
      CVarCesiumRuntimeNaniteLogFallbacks.GetValueOnAnyThread() != 0,
      MinimumTrianglesOverride >= 0
          ? MinimumTrianglesOverride
          : FMath::Max(0, ActorMinimumTriangles)};
}
