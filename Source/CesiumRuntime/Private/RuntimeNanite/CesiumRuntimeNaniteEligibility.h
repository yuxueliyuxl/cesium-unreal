// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#pragma once

#include "RuntimeNanite/CesiumRuntimeNaniteTypes.h"

FCesiumRuntimeNaniteEligibilityResult
EvaluateCesiumRuntimeNaniteEligibility(
    const FCesiumRuntimeNaniteEligibilityInput& Input);

FCesiumRuntimeNaniteRuntimeSettings GetCesiumRuntimeNaniteRuntimeSettings(
    bool bActorEnabled,
    int32 ActorMinimumTriangles);
