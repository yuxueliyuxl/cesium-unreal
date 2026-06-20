// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#include "RuntimeNanite/CesiumPrimitiveMeshData.h"

FCesiumPrimitiveMeshData ExtractCesiumPrimitiveMeshData(
    const FStaticMeshVertexBuffers& VertexBuffers,
    TConstArrayView<uint32> Indices,
    const FBoxSphereBounds& Bounds,
    bool bHasColors) {
  FCesiumPrimitiveMeshData Result;
  const uint32 NumVertices =
      VertexBuffers.PositionVertexBuffer.GetNumVertices();
  const uint32 NumTextureCoordinates =
      VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();

  Result.Positions.SetNumUninitialized(NumVertices);
  Result.Normals.SetNumUninitialized(NumVertices);
  Result.TangentsX.SetNumUninitialized(NumVertices);
  Result.TangentSigns.SetNumUninitialized(NumVertices);
  Result.TextureCoordinates.SetNum(NumTextureCoordinates);
  for (TArray<FVector2f>& UVs : Result.TextureCoordinates) {
    UVs.SetNumUninitialized(NumVertices);
  }

  Result.bHasColors =
      bHasColors &&
      VertexBuffers.ColorVertexBuffer.GetNumVertices() == NumVertices;
  if (Result.bHasColors) {
    Result.Colors.SetNumUninitialized(NumVertices);
  }

  for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex) {
    Result.Positions[VertexIndex] =
        VertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex);

    const FVector3f TangentX =
        VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(VertexIndex);
    const FVector3f TangentY =
        VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(VertexIndex);
    const FVector3f Normal =
        VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(VertexIndex);
    Result.Normals[VertexIndex] = Normal;
    Result.TangentsX[VertexIndex] = TangentX;
    Result.TangentSigns[VertexIndex] =
        FVector3f::DotProduct(
            FVector3f::CrossProduct(Normal, TangentX),
            TangentY) >= 0.0f
            ? 1.0f
            : -1.0f;

    for (uint32 UVIndex = 0; UVIndex < NumTextureCoordinates; ++UVIndex) {
      Result.TextureCoordinates[UVIndex][VertexIndex] =
          VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(
              VertexIndex,
              UVIndex);
    }

    if (Result.bHasColors) {
      Result.Colors[VertexIndex] =
          VertexBuffers.ColorVertexBuffer.VertexColor(VertexIndex);
    }
  }

  Result.Indices.Append(Indices.GetData(), Indices.Num());
  Result.Bounds = Bounds;
  return Result;
}
