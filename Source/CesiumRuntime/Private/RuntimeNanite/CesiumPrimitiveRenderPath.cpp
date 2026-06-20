// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#include "RuntimeNanite/CesiumPrimitiveRenderPath.h"

bool HasCesiumRuntimeNaniteData(
    const FStaticMeshRenderData* pRenderData) {
  return pRenderData && pRenderData->HasValidNaniteData();
}

FCesiumPrimitiveRenderDataSelection SelectCesiumPrimitiveRenderData(
    const FCesiumPrimitiveMeshData& Mesh,
    const FCesiumRuntimeNaniteEligibilityInput& EligibilityInput,
    TUniquePtr<FStaticMeshRenderData>&& LegacyRenderData,
    TFunctionRef<TUniquePtr<FStaticMeshRenderData>(
        const FCesiumPrimitiveMeshData&)>
        BuildNanite) {
  const FCesiumRuntimeNaniteEligibilityResult Eligibility =
      EvaluateCesiumRuntimeNaniteEligibility(EligibilityInput);
  if (!Eligibility.bEligible) {
    return FCesiumPrimitiveRenderDataSelection{
        MoveTemp(LegacyRenderData),
        ECesiumPrimitiveRenderPath::Legacy,
        Eligibility.FallbackReason};
  }

  TUniquePtr<FStaticMeshRenderData> NaniteRenderData =
      BuildNanite(Mesh);
  if (!NaniteRenderData) {
    return FCesiumPrimitiveRenderDataSelection{
        MoveTemp(LegacyRenderData),
        ECesiumPrimitiveRenderPath::Legacy,
        ECesiumRuntimeNaniteFallbackReason::BuilderFailed};
  }

  return FCesiumPrimitiveRenderDataSelection{
      MoveTemp(NaniteRenderData),
      ECesiumPrimitiveRenderPath::RuntimeNanite,
      ECesiumRuntimeNaniteFallbackReason::None};
}
