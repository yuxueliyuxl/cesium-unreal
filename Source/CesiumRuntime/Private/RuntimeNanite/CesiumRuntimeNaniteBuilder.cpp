// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Portions derived from VoxelCore.
// Copyright Voxel Plugin SAS.
// Licensed under the MIT License; see LICENSE.VoxelCore.txt.

#include "RuntimeNanite/CesiumRuntimeNaniteBuilder.h"

#include "Nanite/NaniteFixupChunk.h"
#include "Rendering/NaniteResources.h"
#include "RuntimeNanite/CesiumRuntimeNaniteEncoding.h"

namespace {

class FByteBitWriter {
public:
  explicit FByteBitWriter(TArray<uint8>& InBuffer) : Buffer(InBuffer) {}

  void PutBits(uint32 Bits, uint32 NumBits) {
    if (NumBits == 0) {
      return;
    }
    check(NumBits <= 32);
    check(NumBits == 32 || uint64(Bits) < (uint64(1) << NumBits));
    PendingBits |= uint64(Bits) << NumPendingBits;
    NumPendingBits += NumBits;
    while (NumPendingBits >= 8) {
      Buffer.Add(uint8(PendingBits));
      PendingBits >>= 8;
      NumPendingBits -= 8;
    }
  }

  void Flush(uint32 Alignment = 1) {
    if (NumPendingBits > 0) {
      Buffer.Add(uint8(PendingBits));
    }
    while (Buffer.Num() % int32(Alignment) != 0) {
      Buffer.Add(0);
    }
    PendingBits = 0;
    NumPendingBits = 0;
  }

private:
  TArray<uint8>& Buffer;
  uint64 PendingBits = 0;
  int32 NumPendingBits = 0;
};

template <typename T> void AppendValue(TArray<uint8>& Data, const T& Value) {
  const int32 Offset = Data.AddUninitialized(sizeof(T));
  FMemory::Memcpy(Data.GetData() + Offset, &Value, sizeof(T));
}

template <typename T>
TArrayView<T> AppendValues(TArray<uint8>& Data, int32 Count) {
  const int32 Offset = Data.AddZeroed(sizeof(T) * Count);
  return TArrayView<T>(
      reinterpret_cast<T*>(Data.GetData() + Offset),
      Count);
}

void AlignData(TArray<uint8>& Data, uint32 Alignment) {
  while (Data.Num() % int32(Alignment) != 0) {
    Data.Add(0);
  }
}

FBox3f GetBounds(const TArray<FVector3f>& Positions) {
  FBox3f Bounds(EForceInit::ForceInit);
  for (const FVector3f& Position : Positions) {
    Bounds += Position;
  }
  return Bounds;
}

bool ValidateMesh(const FCesiumRuntimeNaniteMeshData& Mesh) {
  if (Mesh.Positions.IsEmpty() ||
      Mesh.Normals.Num() != Mesh.Positions.Num() ||
      Mesh.Indices.IsEmpty() || Mesh.Indices.Num() % 3 != 0 ||
      Mesh.TextureCoordinates.Num() > NANITE_MAX_UVS) {
    return false;
  }
  if (!Mesh.Colors.IsEmpty() &&
      Mesh.Colors.Num() != Mesh.Positions.Num()) {
    return false;
  }
  for (const TArray<FVector2f>& UVs : Mesh.TextureCoordinates) {
    if (UVs.Num() != Mesh.Positions.Num()) {
      return false;
    }
  }
  for (uint32 Index : Mesh.Indices) {
    if (Index >= uint32(Mesh.Positions.Num())) {
      return false;
    }
  }
  return true;
}

TArray<TUniquePtr<FCesiumRuntimeNaniteCluster>>
CreateClusters(const FCesiumRuntimeNaniteMeshData& Mesh) {
  TArray<TUniquePtr<FCesiumRuntimeNaniteCluster>> Clusters;
  const int32 TriangleCount = Mesh.Indices.Num() / 3;

  for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount;
       ++TriangleIndex) {
    if (Clusters.IsEmpty() ||
        Clusters.Last()->NumTriangles() >= NANITE_MAX_CLUSTER_TRIANGLES ||
        Clusters.Last()->NumVertices() + 3 >
            NANITE_MAX_CLUSTER_VERTICES) {
      TUniquePtr<FCesiumRuntimeNaniteCluster>& Cluster =
          Clusters.Add_GetRef(
              MakeUnique<FCesiumRuntimeNaniteCluster>());
      Cluster->TextureCoordinates.SetNum(
          Mesh.TextureCoordinates.Num());
    }

    FCesiumRuntimeNaniteCluster& Cluster = *Clusters.Last();
    const uint32 ClusterTriangleIndex = Cluster.NumTriangles();
    const uint32 DWordBucket = ClusterTriangleIndex >> 5;
    const uint32 DWordBit = ClusterTriangleIndex & 31;

    for (int32 Corner = 0; Corner < 3; ++Corner) {
      const uint32 SourceIndex =
          Mesh.Indices[TriangleIndex * 3 + Corner];
      const uint8 ClusterIndex = uint8(Cluster.Positions.Num());
      Cluster.Positions.Add(Mesh.Positions[SourceIndex]);
      Cluster.Normals.Add(
          FCesiumRuntimeNaniteOctahedron::Encode(
              Mesh.Normals[SourceIndex]));
      if (!Mesh.Colors.IsEmpty()) {
        Cluster.Colors.Add(Mesh.Colors[SourceIndex]);
      }
      for (int32 UVIndex = 0;
           UVIndex < Mesh.TextureCoordinates.Num();
           ++UVIndex) {
        Cluster.TextureCoordinates[UVIndex].Add(
            Mesh.TextureCoordinates[UVIndex][SourceIndex]);
      }
      Cluster.Indices.Add(ClusterIndex);
    }

    Cluster.StripBitmaskDWords[3 * DWordBucket] |=
        1u << DWordBit;
  }
  return Clusters;
}

bool GetEncodingInfo(
    const FCesiumRuntimeNaniteCluster& Cluster,
    int32 PositionPrecision,
    FCesiumRuntimeNaniteEncodingInfo& Info) {
  const FBox3f Bounds = GetBounds(Cluster.Positions);
  Info.Settings.PositionPrecision = PositionPrecision;
  Info.BitsPerIndex =
      FMath::FloorLog2(FMath::Max(Cluster.NumVertices() - 1, 1)) + 1;
  Info.BitsPerAttribute =
      2 * FCesiumRuntimeNaniteEncodingSettings::NormalBits;

  const float QuantizationScale =
      FMath::Exp2(float(PositionPrecision));
  const FIntVector Min(
      FMath::FloorToInt(Bounds.Min.X * QuantizationScale),
      FMath::FloorToInt(Bounds.Min.Y * QuantizationScale),
      FMath::FloorToInt(Bounds.Min.Z * QuantizationScale));
  const FIntVector Max(
      FMath::CeilToInt(Bounds.Max.X * QuantizationScale),
      FMath::CeilToInt(Bounds.Max.Y * QuantizationScale),
      FMath::CeilToInt(Bounds.Max.Z * QuantizationScale));
  Info.PositionMin = Min;
  Info.PositionBits = FIntVector(
      FMath::CeilLogTwo(Max.X - Min.X + 1),
      FMath::CeilLogTwo(Max.Y - Min.Y + 1),
      FMath::CeilLogTwo(Max.Z - Min.Z + 1));
  if (Info.PositionBits.GetMax() >
      NANITE_MAX_POSITION_QUANTIZATION_BITS) {
    return false;
  }

  if (!Cluster.Colors.IsEmpty()) {
    Info.ColorMin = Cluster.Colors[0];
    Info.ColorMax = Cluster.Colors[0];
    for (const FColor& Color : Cluster.Colors) {
      Info.ColorMin.R = FMath::Min(Info.ColorMin.R, Color.R);
      Info.ColorMin.G = FMath::Min(Info.ColorMin.G, Color.G);
      Info.ColorMin.B = FMath::Min(Info.ColorMin.B, Color.B);
      Info.ColorMin.A = FMath::Min(Info.ColorMin.A, Color.A);
      Info.ColorMax.R = FMath::Max(Info.ColorMax.R, Color.R);
      Info.ColorMax.G = FMath::Max(Info.ColorMax.G, Color.G);
      Info.ColorMax.B = FMath::Max(Info.ColorMax.B, Color.B);
      Info.ColorMax.A = FMath::Max(Info.ColorMax.A, Color.A);
    }
    if (Info.ColorMin != Info.ColorMax) {
      Info.ColorBits = FIntVector4(
          FMath::CeilLogTwo(
              int32(Info.ColorMax.R) - int32(Info.ColorMin.R) + 1),
          FMath::CeilLogTwo(
              int32(Info.ColorMax.G) - int32(Info.ColorMin.G) + 1),
          FMath::CeilLogTwo(
              int32(Info.ColorMax.B) - int32(Info.ColorMin.B) + 1),
          FMath::CeilLogTwo(
              int32(Info.ColorMax.A) - int32(Info.ColorMin.A) + 1));
      Info.BitsPerAttribute += Info.ColorBits.X +
                               Info.ColorBits.Y +
                               Info.ColorBits.Z +
                               Info.ColorBits.W;
    }
  }

  Info.NumTextureCoordinates = Cluster.TextureCoordinates.Num();
  for (int32 UVIndex = 0;
       UVIndex < Cluster.TextureCoordinates.Num();
       ++UVIndex) {
    FUintVector2 UVMin(MAX_uint32, MAX_uint32);
    FUintVector2 UVMax(0, 0);
    for (const FVector2f& UV :
         Cluster.TextureCoordinates[UVIndex]) {
      if (!FMath::IsFinite(UV.X) || !FMath::IsFinite(UV.Y)) {
        return false;
      }
      const uint32 U = CesiumRuntimeNaniteEncodeUVFloat(UV.X);
      const uint32 V = CesiumRuntimeNaniteEncodeUVFloat(UV.Y);
      UVMin.X = FMath::Min(UVMin.X, U);
      UVMin.Y = FMath::Min(UVMin.Y, V);
      UVMax.X = FMath::Max(UVMax.X, U);
      UVMax.Y = FMath::Max(UVMax.Y, V);
    }
    FCesiumRuntimeNaniteUVRange& Range = Info.UVRanges[UVIndex];
    Range.Min = UVMin;
    Range.NumBits = FUintVector2(
        FMath::CeilLogTwo(UVMax.X - UVMin.X + 1),
        FMath::CeilLogTwo(UVMax.Y - UVMin.Y + 1));
    Info.UVMins[UVIndex] = UVMin;
    Info.BitsPerAttribute += Range.NumBits.X + Range.NumBits.Y;
  }

  FCesiumRuntimeNanitePageSections& Sizes = Info.GPUSizes;
  Sizes.Cluster = sizeof(Nanite::FPackedCluster);
  Sizes.DecodeInfo =
      Cluster.TextureCoordinates.Num() *
      sizeof(FCesiumRuntimeNanitePackedUVRange);
  const int32 BitsPerTriangle = Info.BitsPerIndex + 10;
  Sizes.Index =
      FMath::DivideAndRoundUp(
          Cluster.NumTriangles() * BitsPerTriangle,
          32) *
      sizeof(uint32);
  const int32 PositionBitsPerVertex = Info.PositionBits.X +
                                      Info.PositionBits.Y +
                                      Info.PositionBits.Z;
  Sizes.Position =
      FMath::DivideAndRoundUp(
          Cluster.NumVertices() * PositionBitsPerVertex,
          32) *
      sizeof(uint32);
  Sizes.Attribute =
      FMath::DivideAndRoundUp(
          Cluster.NumVertices() * Info.BitsPerAttribute,
          32) *
      sizeof(uint32);
  return true;
}

Nanite::FPackedCluster PackCluster(
    const FCesiumRuntimeNaniteCluster& Cluster,
    const FCesiumRuntimeNaniteEncodingInfo& Info) {
  const FBox3f Bounds = GetBounds(Cluster.Positions);
  float MaxEdgeLengthSquared = 0.0f;
  for (int32 Triangle = 0; Triangle < Cluster.NumTriangles();
       ++Triangle) {
    const FVector3f& A =
        Cluster.Positions[Cluster.Indices[Triangle * 3]];
    const FVector3f& B =
        Cluster.Positions[Cluster.Indices[Triangle * 3 + 1]];
    const FVector3f& C =
        Cluster.Positions[Cluster.Indices[Triangle * 3 + 2]];
    MaxEdgeLengthSquared =
        FMath::Max(MaxEdgeLengthSquared, FVector3f::DistSquared(A, B));
    MaxEdgeLengthSquared =
        FMath::Max(MaxEdgeLengthSquared, FVector3f::DistSquared(B, C));
    MaxEdgeLengthSquared =
        FMath::Max(MaxEdgeLengthSquared, FVector3f::DistSquared(A, C));
  }

  Nanite::FPackedCluster Packed;
  FMemory::Memzero(Packed);
  Packed.SetNumVerts(Cluster.NumVertices());
  Packed.SetNumTris(Cluster.NumTriangles());
  if (Cluster.Colors.IsEmpty()) {
    Packed.SetColorMode(NANITE_VERTEX_COLOR_MODE_CONSTANT);
    Packed.ColorMin = FColor::White.ToPackedABGR();
  } else if (Info.ColorBits == FIntVector4(0)) {
    Packed.SetColorMode(NANITE_VERTEX_COLOR_MODE_CONSTANT);
    Packed.ColorMin = Info.ColorMin.ToPackedABGR();
  } else {
    Packed.SetColorMode(NANITE_VERTEX_COLOR_MODE_VARIABLE);
    Packed.SetColorBitsR(Info.ColorBits.X);
    Packed.SetColorBitsG(Info.ColorBits.Y);
    Packed.SetColorBitsB(Info.ColorBits.Z);
    Packed.SetColorBitsA(Info.ColorBits.W);
    Packed.ColorMin = Info.ColorMin.ToPackedABGR();
  }
  Packed.SetGroupIndex(0);
  Packed.SetBitsPerIndex(Info.BitsPerIndex);
  Packed.PosStart = Info.PositionMin;
  Packed.SetPosPrecision(Info.Settings.PositionPrecision);
  Packed.SetPosBitsX(Info.PositionBits.X);
  Packed.SetPosBitsY(Info.PositionBits.Y);
  Packed.SetPosBitsZ(Info.PositionBits.Z);
  const FVector3f Center = Bounds.GetCenter();
  const FVector3f Extent = Bounds.GetExtent();
  Packed.LODBounds = FSphere3f(Center, Extent.Length());
  Packed.BoxBoundsCenter = Center;
  Packed.LODErrorAndEdgeLength =
      uint32(FFloat16(0.1f).Encoded) |
      (uint32(FFloat16(FMath::Sqrt(MaxEdgeLengthSquared)).Encoded)
       << 16);
  Packed.BoxBoundsExtent = Extent;
  Packed.SetFlags(
      NANITE_CLUSTER_FLAG_STREAMING_LEAF |
      NANITE_CLUSTER_FLAG_ROOT_LEAF);
  Packed.SetBitsPerAttribute(Info.BitsPerAttribute);
  Packed.SetNormalPrecision(
      FCesiumRuntimeNaniteEncodingSettings::NormalBits);
  Packed.SetHasTangents(false);
  Packed.SetHasSkinning(false);
  Packed.SetNumUVs(Cluster.TextureCoordinates.Num());
  uint32 BitOffset = 0;
  for (int32 UVIndex = 0;
       UVIndex < Cluster.TextureCoordinates.Num();
       ++UVIndex) {
    Packed.UVBitOffsets |= BitOffset << (UVIndex * 8);
    BitOffset += Info.UVRanges[UVIndex].NumBits.X +
                 Info.UVRanges[UVIndex].NumBits.Y;
  }
  return Packed;
}

int32 ShortestWrap(int32 Value, int32 NumBits) {
  if (NumBits == 0) {
    return 0;
  }
  const int32 Shift = 32 - NumBits;
  return (Value << Shift) >> Shift;
}

void WriteZigZagDelta(
    TArray<uint8>& Low,
    TArray<uint8>& Mid,
    TArray<uint8>& High,
    int32 Delta,
    int32 NumBytes) {
  const uint32 Value = CesiumRuntimeNaniteEncodeZigZag(Delta);
  if (NumBytes >= 3) {
    High.Add((Value >> 16) & 0xffu);
  }
  if (NumBytes >= 2) {
    Mid.Add((Value >> 8) & 0xffu);
  }
  if (NumBytes >= 1) {
    Low.Add(Value & 0xffu);
  }
}

bool CreatePageData(
    const TArray<FCesiumRuntimeNaniteCluster*>& Clusters,
    int32 PositionPrecision,
    TArray<uint8>& PageData) {
  if (Clusters.IsEmpty()) {
    return false;
  }
  const int32 PageStart = PageData.Num();
  const auto PageOffset = [&]() {
    return uint32(PageData.Num() - PageStart);
  };
  const int32 NumUVs = Clusters[0]->TextureCoordinates.Num();

  TArray<FCesiumRuntimeNaniteEncodingInfo> Infos;
  Infos.SetNum(Clusters.Num());
  FCesiumRuntimeNanitePageSections PageSizes;
  for (int32 Index = 0; Index < Clusters.Num(); ++Index) {
    if (Clusters[Index]->TextureCoordinates.Num() != NumUVs ||
        !GetEncodingInfo(
            *Clusters[Index],
            PositionPrecision,
            Infos[Index])) {
      return false;
    }
    PageSizes += Infos[Index].GPUSizes;
  }
  if (PageSizes.GetTotal() > NANITE_ROOT_PAGE_GPU_SIZE) {
    return false;
  }

  TArray<Nanite::FPackedCluster> PackedClusters;
  PackedClusters.Reserve(Clusters.Num());
  FCesiumRuntimeNanitePageSections Offsets =
      PageSizes.GetOffsets();
  for (int32 Index = 0; Index < Clusters.Num(); ++Index) {
    Nanite::FPackedCluster& Packed =
        PackedClusters.Add_GetRef(
            PackCluster(*Clusters[Index], Infos[Index]));
    const int32 NumBatches = FMath::DivideAndRoundUp(
        Clusters[Index]->NumTriangles(),
        10);
    TArray<uint8> BatchBytes;
    FByteBitWriter BatchWriter(BatchBytes);
    BatchWriter.PutBits(NumBatches, 4);
    BatchWriter.PutBits(0, 4);
    BatchWriter.PutBits(0, 4);
    int32 TrianglesLeft = Clusters[Index]->NumTriangles();
    for (int32 Batch = 0; Batch < NumBatches; ++Batch) {
      const int32 Count = FMath::Min(TrianglesLeft, 10);
      BatchWriter.PutBits(Count - 1, 5);
      TrianglesLeft -= Count;
    }
    BatchWriter.Flush(sizeof(uint32));
    TArray<uint32> BatchInfo;
    BatchInfo.SetNumZeroed(BatchBytes.Num() / sizeof(uint32));
    FMemory::Memcpy(
        BatchInfo.GetData(),
        BatchBytes.GetData(),
        BatchBytes.Num());
    Packed.PackedMaterialInfo =
        uint32(Clusters[Index]->NumTriangles() - 1) << 18;
    Packed.SetIndexOffset(Offsets.Index);
    Packed.SetPositionOffset(Offsets.Position);
    Packed.SetAttributeOffset(Offsets.Attribute);
    Packed.SetDecodeInfoOffset(Offsets.DecodeInfo);
    Packed.SetVertResourceBatchInfo(
        BatchInfo,
        Offsets.VertReuseBatchInfo,
        1);
    Packed.SetExtendedDataOffset(Offsets.ExtendedData);
    Packed.SetExtendedDataNum(0);
    Offsets += Infos[Index].GPUSizes;
  }

  const int32 PageHeaderOffset =
      PageData.AddZeroed(sizeof(FCesiumRuntimeNanitePageDiskHeader));
  const int32 ClusterHeadersOffset = PageData.AddZeroed(
      sizeof(FCesiumRuntimeNaniteClusterDiskHeader) *
      Clusters.Num());
  FCesiumRuntimeNanitePageDiskHeader PageHeader;
  PageHeader.NumClusters = Clusters.Num();
  TArray<FCesiumRuntimeNaniteClusterDiskHeader> ClusterHeaders;
  ClusterHeaders.SetNumZeroed(Clusters.Num());

  const uint32 RawFloat4Start = PageOffset();
  FCesiumRuntimeNanitePageGPUHeader GPUHeader;
  GPUHeader.NumClusters = Clusters.Num();
  AppendValue(PageData, GPUHeader);
  static_assert(sizeof(Nanite::FPackedCluster) % 16 == 0);
  constexpr int32 VectorsPerCluster =
      sizeof(Nanite::FPackedCluster) / sizeof(FVector4f);
  for (int32 VectorIndex = 0; VectorIndex < VectorsPerCluster;
       ++VectorIndex) {
    for (const Nanite::FPackedCluster& Packed : PackedClusters) {
      const FVector4f* Vectors =
          reinterpret_cast<const FVector4f*>(&Packed);
      AppendValue(PageData, Vectors[VectorIndex]);
    }
  }

  AlignData(PageData, 16);
  for (int32 ClusterIndex = 0;
       ClusterIndex < Clusters.Num();
       ++ClusterIndex) {
    ClusterHeaders[ClusterIndex].DecodeInfoOffset = PageOffset();
    for (int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex) {
      const FCesiumRuntimeNaniteUVRange& Range =
          Infos[ClusterIndex].UVRanges[UVIndex];
      FCesiumRuntimeNanitePackedUVRange PackedRange;
      PackedRange.Data.X =
          (Range.Min.X << 5) | Range.NumBits.X;
      PackedRange.Data.Y =
          (Range.Min.Y << 5) | Range.NumBits.Y;
      AppendValue(PageData, PackedRange);
    }
  }
  AlignData(PageData, 16);
  PageHeader.NumRawFloat4s =
      (PageOffset() - RawFloat4Start) / sizeof(FVector4f);

  for (int32 ClusterIndex = 0;
       ClusterIndex < Clusters.Num();
       ++ClusterIndex) {
    ClusterHeaders[ClusterIndex].IndexDataOffset = PageOffset();
  }
  AlignData(PageData, sizeof(uint32));
  PageHeader.StripBitmaskOffset = PageOffset();
  constexpr int32 NumMaskDWords =
      NANITE_MAX_CLUSTER_TRIANGLES / 32;
  for (const FCesiumRuntimeNaniteCluster* Cluster : Clusters) {
    for (int32 Word = 0; Word < 3 * NumMaskDWords; ++Word) {
      AppendValue(PageData, Cluster->StripBitmaskDWords[Word]);
    }
  }
  for (int32 ClusterIndex = 0;
       ClusterIndex < Clusters.Num();
       ++ClusterIndex) {
    ClusterHeaders[ClusterIndex].PageClusterMapOffset = PageOffset();
  }
  PageHeader.VertexRefBitmaskOffset = PageOffset();
  PageData.AddZeroed(
      Clusters.Num() * (NANITE_MAX_CLUSTER_VERTICES / 32) *
      sizeof(uint32));
  PageHeader.NumVertexRefs = 0;
  for (int32 ClusterIndex = 0;
       ClusterIndex < Clusters.Num();
       ++ClusterIndex) {
    ClusterHeaders[ClusterIndex].VertexRefDataOffset = PageOffset();
    ClusterHeaders[ClusterIndex].NumVertexRefs = 0;
  }

  struct FStreamCounts {
    int32 Low = 0;
    int32 Mid = 0;
    int32 High = 0;
  };
  TArray<FStreamCounts> StreamCounts;
  StreamCounts.SetNum(Clusters.Num());
  TArray<uint8> Low;
  TArray<uint8> Mid;
  TArray<uint8> High;
  for (int32 ClusterIndex = 0;
       ClusterIndex < Clusters.Num();
       ++ClusterIndex) {
    const int32 PreviousLow = Low.Num();
    const int32 PreviousMid = Mid.Num();
    const int32 PreviousHigh = High.Num();
    const FCesiumRuntimeNaniteCluster& Cluster =
        *Clusters[ClusterIndex];
    const FCesiumRuntimeNaniteEncodingInfo& Info =
        Infos[ClusterIndex];
    const int32 PositionBytes =
        FMath::DivideAndRoundUp(Info.PositionBits.GetMax(), 8);
    const int32 NormalBytes = FMath::DivideAndRoundUp(
        FCesiumRuntimeNaniteEncodingSettings::NormalBits,
        8);
    const float Scale =
        FMath::Exp2(float(PositionPrecision));
    FIntVector PreviousPosition(
        (1 << Info.PositionBits.X) / 2,
        (1 << Info.PositionBits.Y) / 2,
        (1 << Info.PositionBits.Z) / 2);
    for (const FVector3f& Position : Cluster.Positions) {
      const FIntVector Quantized(
          FMath::RoundToInt(Position.X * Scale) -
              Info.PositionMin.X,
          FMath::RoundToInt(Position.Y * Scale) -
              Info.PositionMin.Y,
          FMath::RoundToInt(Position.Z * Scale) -
              Info.PositionMin.Z);
      FIntVector Delta = Quantized - PreviousPosition;
      Delta.X = ShortestWrap(Delta.X, Info.PositionBits.X);
      Delta.Y = ShortestWrap(Delta.Y, Info.PositionBits.Y);
      Delta.Z = ShortestWrap(Delta.Z, Info.PositionBits.Z);
      WriteZigZagDelta(Low, Mid, High, Delta.X, PositionBytes);
      WriteZigZagDelta(Low, Mid, High, Delta.Y, PositionBytes);
      WriteZigZagDelta(Low, Mid, High, Delta.Z, PositionBytes);
      PreviousPosition = Quantized;
    }
    FIntPoint PreviousNormal = FIntPoint::ZeroValue;
    for (const FCesiumRuntimeNaniteOctahedron& Normal :
         Cluster.Normals) {
      const FIntPoint Encoded(Normal.X, Normal.Y);
      FIntPoint Delta = Encoded - PreviousNormal;
      Delta.X = ShortestWrap(
          Delta.X,
          FCesiumRuntimeNaniteEncodingSettings::NormalBits);
      Delta.Y = ShortestWrap(
          Delta.Y,
          FCesiumRuntimeNaniteEncodingSettings::NormalBits);
      WriteZigZagDelta(Low, Mid, High, Delta.X, NormalBytes);
      WriteZigZagDelta(Low, Mid, High, Delta.Y, NormalBytes);
      PreviousNormal = Encoded;
    }
    if (!Cluster.Colors.IsEmpty() &&
        Info.ColorBits != FIntVector4(0)) {
      FIntVector4 PreviousColor(0);
      for (const FColor& Color : Cluster.Colors) {
        const FIntVector4 Encoded(
            Color.R - Info.ColorMin.R,
            Color.G - Info.ColorMin.G,
            Color.B - Info.ColorMin.B,
            Color.A - Info.ColorMin.A);
        FIntVector4 Delta = Encoded - PreviousColor;
        Delta.X = ShortestWrap(Delta.X, Info.ColorBits.X);
        Delta.Y = ShortestWrap(Delta.Y, Info.ColorBits.Y);
        Delta.Z = ShortestWrap(Delta.Z, Info.ColorBits.Z);
        Delta.W = ShortestWrap(Delta.W, Info.ColorBits.W);
        WriteZigZagDelta(Low, Mid, High, Delta.X, 1);
        WriteZigZagDelta(Low, Mid, High, Delta.Y, 1);
        WriteZigZagDelta(Low, Mid, High, Delta.Z, 1);
        WriteZigZagDelta(Low, Mid, High, Delta.W, 1);
        PreviousColor = Encoded;
      }
    }
    for (int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex) {
      const FCesiumRuntimeNaniteUVRange& Range =
          Info.UVRanges[UVIndex];
      const int32 UVBytes = FMath::DivideAndRoundUp(
          FMath::Max<int32>(Range.NumBits.X, Range.NumBits.Y),
          8);
      FIntPoint PreviousUV = FIntPoint::ZeroValue;
      for (const FVector2f& UV :
           Cluster.TextureCoordinates[UVIndex]) {
        const FIntPoint Encoded(
            int32(CesiumRuntimeNaniteEncodeUVFloat(UV.X) -
                  Range.Min.X),
            int32(CesiumRuntimeNaniteEncodeUVFloat(UV.Y) -
                  Range.Min.Y));
        FIntPoint Delta = Encoded - PreviousUV;
        Delta.X = ShortestWrap(Delta.X, Range.NumBits.X);
        Delta.Y = ShortestWrap(Delta.Y, Range.NumBits.Y);
        WriteZigZagDelta(Low, Mid, High, Delta.X, UVBytes);
        WriteZigZagDelta(Low, Mid, High, Delta.Y, UVBytes);
        PreviousUV = Encoded;
      }
    }
    StreamCounts[ClusterIndex] = {
        Low.Num() - PreviousLow,
        Mid.Num() - PreviousMid,
        High.Num() - PreviousHigh};
  }

  if (!Clusters.IsEmpty()) {
    ClusterHeaders[0].LowBytesOffset = PageOffset();
    PageData.Append(Low);
    ClusterHeaders[0].MidBytesOffset = PageOffset();
    PageData.Append(Mid);
    ClusterHeaders[0].HighBytesOffset = PageOffset();
    PageData.Append(High);
    for (int32 Index = 1; Index < Clusters.Num(); ++Index) {
      ClusterHeaders[Index].LowBytesOffset =
          ClusterHeaders[Index - 1].LowBytesOffset +
          StreamCounts[Index - 1].Low;
      ClusterHeaders[Index].MidBytesOffset =
          ClusterHeaders[Index - 1].MidBytesOffset +
          StreamCounts[Index - 1].Mid;
      ClusterHeaders[Index].HighBytesOffset =
          ClusterHeaders[Index - 1].HighBytesOffset +
          StreamCounts[Index - 1].High;
    }
  }
  AlignData(PageData, sizeof(uint32));

  FMemory::Memcpy(
      PageData.GetData() + PageHeaderOffset,
      &PageHeader,
      sizeof(PageHeader));
  FMemory::Memcpy(
      PageData.GetData() + ClusterHeadersOffset,
      ClusterHeaders.GetData(),
      sizeof(FCesiumRuntimeNaniteClusterDiskHeader) *
          ClusterHeaders.Num());
  return true;
}

Nanite::FPackedHierarchyNode MakeHierarchyNode(
    const FBox3f& Bounds) {
  Nanite::FPackedHierarchyNode Node;
  FMemory::Memzero(Node);
  for (int32 Index = 0; Index < NANITE_MAX_BVH_NODE_FANOUT;
       ++Index) {
    Node.LODBounds[Index] =
        FVector4f(Bounds.GetCenter(), Bounds.GetExtent().Length());
    Node.Misc0[Index].MinLODError_MaxParentLODError =
        uint32(FFloat16(1.0e10f).Encoded) |
        (uint32(FFloat16(-1.0f).Encoded) << 16);
    Node.Misc0[Index].BoxBoundsCenter = Bounds.GetCenter();
    Node.Misc1[Index].BoxBoundsExtent = Bounds.GetExtent();
    Node.Misc1[Index].ChildStartReference = MAX_uint32;
    Node.Misc2[Index].ResourcePageRangeKey =
        NANITE_PAGE_RANGE_KEY_EMPTY_RANGE;
    Node.Misc2[Index].GroupPartSize_AssemblyPartIndex = 0;
  }
  return Node;
}

bool BuildResources(
    const TArray<TUniquePtr<FCesiumRuntimeNaniteCluster>>& Clusters,
    const FBox3f& Bounds,
    int32 PositionPrecision,
    Nanite::FResources& Resources) {
  if (Clusters.IsEmpty()) {
    return false;
  }

  TArray<TArray<FCesiumRuntimeNaniteCluster*>> Pages;
  int32 ClusterIndex = 0;
  while (ClusterIndex < Clusters.Num()) {
    TArray<FCesiumRuntimeNaniteCluster*>& Page =
        Pages.AddDefaulted_GetRef();
    FCesiumRuntimeNanitePageSections PageSizes;
    while (ClusterIndex < Clusters.Num() &&
           Page.Num() < NANITE_ROOT_PAGE_MAX_CLUSTERS) {
      FCesiumRuntimeNaniteEncodingInfo Info;
      if (!GetEncodingInfo(
              *Clusters[ClusterIndex],
              PositionPrecision,
              Info)) {
        return false;
      }
      FCesiumRuntimeNanitePageSections Candidate = PageSizes;
      Candidate += Info.GPUSizes;
      if (!Page.IsEmpty() &&
          Candidate.GetTotal() > NANITE_ROOT_PAGE_GPU_SIZE) {
        break;
      }
      if (Candidate.GetTotal() > NANITE_ROOT_PAGE_GPU_SIZE) {
        return false;
      }
      Page.Add(Clusters[ClusterIndex].Get());
      PageSizes = Candidate;
      ++ClusterIndex;
    }
  }

  const int32 TreeDepth = FMath::FloorToInt32(FMath::LogX(
      4.0f,
      float(FMath::Max(Clusters.Num() - 1, 1))));
  if (TreeDepth >= NANITE_MAX_CLUSTER_HIERARCHY_DEPTH) {
    return false;
  }
  Resources.HierarchyNodes.Add(MakeHierarchyNode(Bounds));
  TArray<int32> LeafNodes{0};
  for (int32 Depth = 0; Depth < TreeDepth; ++Depth) {
    TArray<int32> NewLeaves;
    for (int32 ParentIndex : LeafNodes) {
      const int32 ChildStart = Resources.HierarchyNodes.Num();
      for (int32 Child = 0; Child < 4; ++Child) {
        Resources.HierarchyNodes.Add(MakeHierarchyNode(Bounds));
        NewLeaves.Add(ChildStart + Child);
      }
      Nanite::FPackedHierarchyNode& Parent =
          Resources.HierarchyNodes[ParentIndex];
      for (int32 Child = 0; Child < 4; ++Child) {
        Parent.Misc1[Child].ChildStartReference =
            ChildStart + Child;
        Parent.Misc2[Child].ResourcePageRangeKey = MAX_uint32;
        Parent.Misc2[Child].GroupPartSize_AssemblyPartIndex =
            NANITE_HIERARCHY_MAX_ASSEMBLY_TRANSFORMS;
      }
    }
    LeafNodes = MoveTemp(NewLeaves);
  }

  struct FLeafLocation {
    int32 NodeIndex;
    int32 PartIndex;
  };
  TArray<FLeafLocation> Locations;
  for (int32 Leaf : LeafNodes) {
    for (int32 Part = 0; Part < 4; ++Part) {
      Locations.Add({Leaf, Part});
    }
  }
  if (Clusters.Num() > Locations.Num()) {
    return false;
  }

  int32 GlobalClusterIndex = 0;
  for (int32 PageIndex = 0; PageIndex < Pages.Num();
       ++PageIndex) {
    TArray<FCesiumRuntimeNaniteCluster*>& Page = Pages[PageIndex];
    for (int32 LocalIndex = 0; LocalIndex < Page.Num();
         ++LocalIndex) {
      const FLeafLocation& Location =
          Locations[GlobalClusterIndex + LocalIndex];
      Nanite::FPackedHierarchyNode& Node =
          Resources.HierarchyNodes[Location.NodeIndex];
      const FBox3f ClusterBounds =
          GetBounds(Page[LocalIndex]->Positions);
      Node.LODBounds[Location.PartIndex] = FVector4f(
          ClusterBounds.GetCenter(),
          ClusterBounds.GetExtent().Length());
      Node.Misc0[Location.PartIndex].BoxBoundsCenter =
          ClusterBounds.GetCenter();
      Node.Misc1[Location.PartIndex].BoxBoundsExtent =
          ClusterBounds.GetExtent();
      Node.Misc2[Location.PartIndex].ResourcePageRangeKey =
          Nanite::FPageRangeKey(
              PageIndex,
              1,
              false,
              false)
              .Value;
      Node.Misc2[Location.PartIndex]
          .GroupPartSize_AssemblyPartIndex =
          (MAX_uint32 & NANITE_HIERARCHY_MAX_ASSEMBLY_TRANSFORMS) |
          (1u << NANITE_HIERARCHY_ASSEMBLY_TRANSFORM_INDEX_BITS);
    }

    Nanite::FPageStreamingState StreamingState{};
    StreamingState.BulkOffset = Resources.RootData.Num();
    const uint32 FixupSize = Nanite::FFixupChunk::GetSize(
        1,
        Page.Num(),
        0,
        Page.Num(),
        0,
        0);
    TArray<uint8> FixupData;
    FixupData.SetNumZeroed(FixupSize);
    Nanite::FFixupChunk& Fixup =
        *reinterpret_cast<Nanite::FFixupChunk*>(
            FixupData.GetData());
    Fixup.Header.Magic = NANITE_FIXUP_MAGIC;
    Fixup.Header.NumGroupFixups = 1;
    Fixup.Header.NumPartFixups = Page.Num();
    Fixup.Header.NumClusters = Page.Num();
    Fixup.Header.NumReconsiderPages = 0;
    Fixup.Header.NumParentFixups = 0;
    Fixup.Header.NumHierarchyLocations = Page.Num();
    Fixup.Header.NumClusterIndices = 0;
    Nanite::FFixupChunk::FGroupFixup& Group =
        Fixup.GetGroupFixup(0);
    Group.PageDependencies =
        Nanite::FPageRangeKey(PageIndex, 1, false, false);
    Group.Flags = 0;
    Group.FirstPartFixup = 0;
    Group.NumPartFixups = Page.Num();
    Group.FirstParentFixup = 0;
    Group.NumParentFixups = 0;
    for (int32 LocalIndex = 0; LocalIndex < Page.Num();
         ++LocalIndex) {
      Nanite::FFixupChunk::FPartFixup& Part =
          Fixup.GetPartFixup(LocalIndex);
      Part.PageIndex = PageIndex;
      Part.StartClusterIndex = LocalIndex;
      Part.LeafCounter = 0;
      Part.FirstHierarchyLocation = LocalIndex;
      Part.NumHierarchyLocations = 1;
      const FLeafLocation& Location =
          Locations[GlobalClusterIndex + LocalIndex];
      Fixup.GetHierarchyLocation(LocalIndex) =
          Nanite::FFixupChunk::FHierarchyLocation(
              Location.NodeIndex,
              Location.PartIndex);
    }
    Resources.RootData.Append(FixupData);
    const int32 PageStart = Resources.RootData.Num();
    if (!CreatePageData(Page, PositionPrecision, Resources.RootData)) {
      return false;
    }
    StreamingState.DependenciesStart = 0;
    StreamingState.DependenciesNum = 0;
    StreamingState.BulkSize =
        Resources.RootData.Num() - StreamingState.BulkOffset;
    StreamingState.PageSize =
        Resources.RootData.Num() - PageStart;
    StreamingState.MaxHierarchyDepth =
        NANITE_MAX_CLUSTER_HIERARCHY_DEPTH;
    StreamingState.Flags = 0;
    Resources.PageStreamingStates.Add(StreamingState);
    GlobalClusterIndex += Page.Num();
  }
  return true;
}

} // namespace

TUniquePtr<FStaticMeshRenderData>
BuildCesiumRuntimeNaniteRenderData(
    const FCesiumRuntimeNaniteMeshData& Mesh,
    int32 PositionPrecision) {
  if (!ValidateMesh(Mesh)) {
    return nullptr;
  }
  PositionPrecision = FMath::Clamp(
      PositionPrecision,
      NANITE_MIN_POSITION_PRECISION,
      NANITE_MAX_POSITION_PRECISION);

  TArray<TUniquePtr<FCesiumRuntimeNaniteCluster>> Clusters =
      CreateClusters(Mesh);
  const FBox3f Bounds = GetBounds(Mesh.Positions);
  Nanite::FResources Resources;
  if (!BuildResources(
          Clusters,
          Bounds,
          PositionPrecision,
          Resources)) {
    return nullptr;
  }
  Resources.PositionPrecision = PositionPrecision;
  Resources.NormalPrecision =
      FCesiumRuntimeNaniteEncodingSettings::NormalBits;
  Resources.TangentPrecision = 0;
  Resources.NumInputTriangles = Mesh.Indices.Num() / 3;
  Resources.NumInputVertices = Mesh.Positions.Num();
  Resources.NumClusters = Clusters.Num();
  Resources.NumRootPages = Resources.PageStreamingStates.Num();
  Resources.HierarchyRootOffsets.Add(0);
  Resources.MeshBounds = FBoxSphereBounds3f(Bounds);

  TUniquePtr<FStaticMeshRenderData> RenderData =
      MakeUnique<FStaticMeshRenderData>();
  RenderData->Bounds = FBoxSphereBounds(FBox(Bounds));
  RenderData->NumInlinedLODs = 1;
  RenderData->NaniteResourcesPtr =
      MakePimpl<Nanite::FResources>(MoveTemp(Resources));

  FStaticMeshLODResources* LOD = new FStaticMeshLODResources();
  LOD->bBuffersInlined = true;
  LOD->Sections.Emplace();
  const TArray<FVector3f> DummyPositions{
      FVector3f(MAX_flt, MAX_flt, MAX_flt)};
  LOD->VertexBuffers.StaticMeshVertexBuffer.Init(
      DummyPositions.Num(),
      1);
  LOD->VertexBuffers.PositionVertexBuffer.Init(DummyPositions);
  LOD->VertexBuffers.ColorVertexBuffer.Init(DummyPositions.Num());
  LOD->BuffersSize = 1;
  RenderData->LODResources.Add(LOD);
  RenderData->LODVertexFactories.Add(
      FStaticMeshVertexFactories(GMaxRHIFeatureLevel));
  return RenderData;
}
