// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Portions derived from VoxelCore.
// Copyright Voxel Plugin SAS.
// Licensed under the MIT License; see LICENSE.VoxelCore.txt.

#pragma once

#include "Rendering/NaniteResources.h"
#include "RuntimeNanite/CesiumRuntimeNaniteTypes.h"
#include "StaticMeshResources.h"

TUniquePtr<FStaticMeshRenderData>
BuildCesiumRuntimeNaniteRenderData(
    const FCesiumRuntimeNaniteMeshData& Mesh,
    int32 PositionPrecision = 4);
