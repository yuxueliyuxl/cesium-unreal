// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#if WITH_EDITOR

#include "Cesium3DTileset.h"
#include "RuntimeNanite/CesiumPrimitiveMeshData.h"
#include "Misc/AutomationTest.h"
#include "RuntimeNanite/CesiumRuntimeNaniteBuilder.h"
#include "RuntimeNanite/CesiumRuntimeNaniteEncoding.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumRuntimeNaniteEncoding,
    "Cesium.Unit.RuntimeNanite.Encoding",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ProductFilter);

bool FCesiumRuntimeNaniteEncoding::RunTest(const FString&) {
  const FCesiumRuntimeNaniteOctahedron Encoded =
      FCesiumRuntimeNaniteOctahedron::Encode(FVector3f::UpVector);

  TestTrue(TEXT("Encoded normal is valid"), Encoded.IsValid());
  TestTrue(
      TEXT("Decoded normal points up"),
      Encoded.Decode().Equals(FVector3f::UpVector, 0.02f));

  const FCesiumRuntimeNanitePageSections Sections{
      .Cluster = 64,
      .MaterialTable = 3,
      .DecodeInfo = 5,
      .Index = 12,
      .Position = 16,
      .Attribute = 20};
  const FCesiumRuntimeNanitePageSections Offsets = Sections.GetOffsets();
  TestEqual(
      TEXT("Cluster data follows the GPU page header"),
      Offsets.Cluster,
      uint32(NANITE_GPU_PAGE_HEADER_SIZE));
  TestEqual(
      TEXT("Material table is aligned before following sections"),
      Offsets.VertReuseBatchInfo,
      Align(Offsets.MaterialTable + Sections.MaterialTable, 16u));

  TestEqual(
      TEXT("Zig-zag preserves zero"),
      CesiumRuntimeNaniteEncodeZigZag(0),
      uint32(0));
  TestEqual(
      TEXT("Zig-zag encodes negative one"),
      CesiumRuntimeNaniteEncodeZigZag(-1),
      uint32(1));
  TestTrue(
      TEXT("UV encoding preserves ordering"),
      CesiumRuntimeNaniteEncodeUVFloat(-1.0f) <
          CesiumRuntimeNaniteEncodeUVFloat(0.0f) &&
          CesiumRuntimeNaniteEncodeUVFloat(0.0f) <
              CesiumRuntimeNaniteEncodeUVFloat(1.0f));
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumRuntimeNaniteBuilder,
    "Cesium.Unit.RuntimeNanite.Builder",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ProductFilter);

bool FCesiumRuntimeNaniteBuilder::RunTest(const FString&) {
  FCesiumPrimitiveMeshData Mesh;
  Mesh.Positions = {
      FVector3f(0.0f, 0.0f, 0.0f),
      FVector3f(100.0f, 0.0f, 0.0f),
      FVector3f(0.0f, 100.0f, 0.0f)};
  Mesh.Normals.Init(FVector3f::UpVector, 3);
  Mesh.Indices = {0, 1, 2};

  TUniquePtr<FStaticMeshRenderData> RenderData =
      BuildCesiumRuntimeNaniteRenderData(Mesh);
  TestNotNull(TEXT("Nanite render data is created"), RenderData.Get());
  TestTrue(
      TEXT("Nanite resources are valid"),
      RenderData && RenderData->NaniteResourcesPtr.IsValid());
  TestTrue(
      TEXT("Root page data is populated"),
      RenderData && RenderData->NaniteResourcesPtr.IsValid() &&
          !RenderData->NaniteResourcesPtr->RootData.IsEmpty());
  TestEqual(
      TEXT("Input triangle count is retained"),
      RenderData && RenderData->NaniteResourcesPtr.IsValid()
          ? RenderData->NaniteResourcesPtr->NumInputTriangles
          : 0u,
      uint32(1));

  FCesiumPrimitiveMeshData MultiClusterMesh = Mesh;
  MultiClusterMesh.Indices.Reset();
  for (int32 Triangle = 0; Triangle < 90; ++Triangle) {
    MultiClusterMesh.Indices.Append({0, 1, 2});
  }
  TUniquePtr<FStaticMeshRenderData> MultiClusterRenderData =
      BuildCesiumRuntimeNaniteRenderData(MultiClusterMesh);
  TestTrue(
      TEXT("Mesh crossing the cluster vertex limit creates multiple clusters"),
      MultiClusterRenderData &&
          MultiClusterRenderData->NaniteResourcesPtr.IsValid() &&
          MultiClusterRenderData->NaniteResourcesPtr->NumClusters > 1);
  TestEqual(
      TEXT("Multi-cluster input triangle count is retained"),
      MultiClusterRenderData &&
              MultiClusterRenderData->NaniteResourcesPtr.IsValid()
          ? MultiClusterRenderData->NaniteResourcesPtr->NumInputTriangles
          : 0u,
      uint32(90));

  FCesiumPrimitiveMeshData Invalid = Mesh;
  Invalid.Positions.Reset();
  TestNull(
      TEXT("Empty positions fail safely"),
      BuildCesiumRuntimeNaniteRenderData(Invalid).Get());

  Invalid = Mesh;
  Invalid.Normals.Pop();
  TestNull(
      TEXT("Mismatched normals fail safely"),
      BuildCesiumRuntimeNaniteRenderData(Invalid).Get());

  Invalid = Mesh;
  Invalid.Indices.Pop();
  TestNull(
      TEXT("Non-triangle indices fail safely"),
      BuildCesiumRuntimeNaniteRenderData(Invalid).Get());

  Invalid = Mesh;
  Invalid.Indices[2] = 3;
  TestNull(
      TEXT("Out-of-range indices fail safely"),
      BuildCesiumRuntimeNaniteRenderData(Invalid).Get());

  Invalid = Mesh;
  Invalid.TextureCoordinates.AddDefaulted();
  Invalid.TextureCoordinates[0].SetNum(2);
  TestNull(
      TEXT("Mismatched texture coordinates fail safely"),
      BuildCesiumRuntimeNaniteRenderData(Invalid).Get());

  Invalid = Mesh;
  Invalid.TextureCoordinates.SetNum(NANITE_MAX_UVS + 1);
  for (TArray<FVector2f>& TextureCoordinates : Invalid.TextureCoordinates) {
    TextureCoordinates.SetNum(3);
  }
  TestNull(
      TEXT("Too many texture coordinates fail safely"),
      BuildCesiumRuntimeNaniteRenderData(Invalid).Get());
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumPrimitiveMeshDataExtraction,
    "Cesium.Unit.RuntimeNanite.PrimitiveMeshData",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ProductFilter);

bool FCesiumPrimitiveMeshDataExtraction::RunTest(const FString&) {
  FStaticMeshVertexBuffers Buffers;
  const TArray<FVector3f> Positions{
      FVector3f(1.0f, -2.0f, 3.0f),
      FVector3f(4.0f, -5.0f, 6.0f),
      FVector3f(7.0f, -8.0f, 9.0f)};
  Buffers.PositionVertexBuffer.Init(Positions);
  Buffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
  Buffers.StaticMeshVertexBuffer.Init(3, 2);
  Buffers.ColorVertexBuffer.Init(3);

  for (uint32 Index = 0; Index < 3; ++Index) {
    Buffers.StaticMeshVertexBuffer.SetVertexTangents(
        Index,
        FVector3f::ForwardVector,
        Index == 1 ? -FVector3f::RightVector : FVector3f::RightVector,
        FVector3f::UpVector);
    Buffers.StaticMeshVertexBuffer.SetVertexUV(
        Index,
        0,
        FVector2f(float(Index), float(Index) + 0.25f));
    Buffers.StaticMeshVertexBuffer.SetVertexUV(
        Index,
        1,
        FVector2f(float(Index) + 0.5f, float(Index) + 0.75f));
    Buffers.ColorVertexBuffer.VertexColor(Index) =
        FColor(uint8(10 + Index), uint8(20 + Index), uint8(30 + Index), 255);
  }

  const TArray<uint32> Indices{0, 2, 1};
  const FBoxSphereBounds Bounds(
      FVector(4.0, -5.0, 6.0),
      FVector(3.0, 3.0, 3.0),
      5.0);
  const FCesiumPrimitiveMeshData Mesh =
      ExtractCesiumPrimitiveMeshData(Buffers, Indices, Bounds, true);

  TestEqual(TEXT("Positions are retained"), Mesh.Positions, Positions);
  TestEqual(TEXT("Indices are retained"), Mesh.Indices, Indices);
  TestEqual(TEXT("Two UV sets are retained"), Mesh.TextureCoordinates.Num(), 2);
  TestEqual(
      TEXT("Second UV set is retained"),
      Mesh.TextureCoordinates[1][2],
      FVector2f(2.5f, 2.75f));
  TestEqual(
      TEXT("Normals are retained"),
      Mesh.Normals[0],
      FVector3f::UpVector);
  TestEqual(
      TEXT("Tangents are retained"),
      Mesh.TangentsX[0],
      FVector3f::ForwardVector);
  TestEqual(TEXT("Tangent sign is retained"), Mesh.TangentSigns[1], -1.0f);
  TestTrue(TEXT("Colors are marked present"), Mesh.bHasColors);
  TestEqual(
      TEXT("Vertex colors are retained"),
      Mesh.Colors[2],
      FColor(12, 22, 32, 255));
  TestEqual(TEXT("Bounds are retained"), Mesh.Bounds, Bounds);
  return true;
}

#endif
