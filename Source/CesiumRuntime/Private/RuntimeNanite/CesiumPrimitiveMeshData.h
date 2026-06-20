// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#pragma once

#include "RuntimeNanite/CesiumRuntimeNaniteTypes.h"
#include "StaticMeshResources.h"

FCesiumPrimitiveMeshData ExtractCesiumPrimitiveMeshData(
    const FStaticMeshVertexBuffers& VertexBuffers,
    TConstArrayView<uint32> Indices,
    const FBoxSphereBounds& Bounds,
    bool bHasColors);
