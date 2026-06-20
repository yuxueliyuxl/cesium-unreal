// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#pragma once

#include "RuntimeNanite/CesiumRuntimeNaniteEligibility.h"
#include "StaticMeshResources.h"
#include "Templates/Function.h"

enum class ECesiumPrimitiveRenderPath : uint8 {
  Legacy,
  RuntimeNanite
};

struct FCesiumPrimitiveRenderDataSelection {
  TUniquePtr<FStaticMeshRenderData> RenderData;
  ECesiumPrimitiveRenderPath RenderPath =
      ECesiumPrimitiveRenderPath::Legacy;
  ECesiumRuntimeNaniteFallbackReason FallbackReason =
      ECesiumRuntimeNaniteFallbackReason::DisabledByActor;
};

bool HasCesiumRuntimeNaniteData(
    const FStaticMeshRenderData* pRenderData);

FCesiumPrimitiveRenderDataSelection SelectCesiumPrimitiveRenderData(
    const FCesiumPrimitiveMeshData& Mesh,
    const FCesiumRuntimeNaniteEligibilityInput& EligibilityInput,
    TUniquePtr<FStaticMeshRenderData>&& LegacyRenderData,
    TFunctionRef<TUniquePtr<FStaticMeshRenderData>(
        const FCesiumPrimitiveMeshData&)>
        BuildNanite);
