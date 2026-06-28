// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#pragma once

#include "CoreMinimal.h"
#include "NaniteDefinitions.h"

struct FCesiumPrimitiveMeshData {
  TArray<FVector3f> Positions;
  TArray<FVector3f> Normals;
  TArray<FVector3f> TangentsX;
  TArray<float> TangentSigns;
  TArray<FColor> Colors;
  TArray<TArray<FVector2f>> TextureCoordinates;
  TArray<uint32> Indices;
  FBoxSphereBounds Bounds = FBoxSphereBounds(EForceInit::ForceInit);
  bool bHasColors = false;
};

enum class ECesiumRuntimeNaniteFallbackReason : uint8 {
  None,
  DisabledByActor,
  DisabledGlobally,
  UnsupportedPlatform,
  UnsupportedPrimitiveMode,
  TranslucentMaterial,
  InvalidPositions,
  InvalidIndices,
  InvalidNormals,
  BelowTriangleThreshold,
  TooManyTextureCoordinates,
  EncodingLimitExceeded,
  BuilderFailed,
  ResourceInitializationFailed
};

struct FCesiumRuntimeNaniteEligibilityInput {
  bool bActorEnabled = false;
  bool bGlobalEnabled = true;
  bool bPlatformSupported = false;
  bool bIsTriangleList = false;
  bool bIsTranslucent = false;
  bool bHasValidPositions = false;
  bool bHasValidIndices = false;
  bool bHasValidNormals = false;
  int32 TriangleCount = 0;
  int32 MinimumTriangleCount = 2000;
  int32 NumTextureCoordinates = 0;
  int32 MaximumTextureCoordinates = NANITE_MAX_UVS;
};

struct FCesiumRuntimeNaniteEligibilityResult {
  bool bEligible = false;
  ECesiumRuntimeNaniteFallbackReason FallbackReason =
      ECesiumRuntimeNaniteFallbackReason::DisabledByActor;
};

struct FCesiumRuntimeNaniteRuntimeSettings {
  bool bActorEnabled = false;
  bool bGlobalEnabled = true;
  bool bPlatformSupported = false;
  bool bLogFallbacks = false;
  int32 MinimumTriangleCount = 2000;
};
