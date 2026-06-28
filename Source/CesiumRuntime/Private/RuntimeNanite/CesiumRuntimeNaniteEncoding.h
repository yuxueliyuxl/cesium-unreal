// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Portions derived from VoxelCore.
// Copyright Voxel Plugin SAS.
// Licensed under the MIT License; see LICENSE.VoxelCore.txt.

#pragma once

#include "CoreMinimal.h"
#include "NaniteDefinitions.h"

struct alignas(2) FCesiumRuntimeNaniteOctahedron {
  uint8 X = 0;
  uint8 Y = 0;

  static FCesiumRuntimeNaniteOctahedron Encode(FVector3f UnitVector);
  FVector3f Decode() const;
  bool IsValid() const;
};
static_assert(sizeof(FCesiumRuntimeNaniteOctahedron) == 2);

uint32 CesiumRuntimeNaniteEncodeZigZag(int32 Value);
uint32 CesiumRuntimeNaniteEncodeUVFloat(
    float Value,
    uint32 NumMantissaBits = NANITE_UV_FLOAT_NUM_MANTISSA_BITS);

struct FCesiumRuntimeNanitePageGPUHeader {
  uint32 NumClusters = 0;
  uint32 Pad[3] = {0, 0, 0};
};

struct FCesiumRuntimeNanitePageDiskHeader {
  uint32 NumClusters = 0;
  uint32 NumRawFloat4s = 0;
  uint32 NumVertexRefs = 0;
  uint32 StripBitmaskOffset = 0;
  uint32 VertexRefBitmaskOffset = 0;
};

struct FCesiumRuntimeNaniteClusterDiskHeader {
  uint32 DecodeInfoOffset = 0;
  uint32 IndexDataOffset = 0;
  uint32 PageClusterMapOffset = 0;
  uint32 VertexRefDataOffset = 0;
  uint32 LowBytesOffset = 0;
  uint32 MidBytesOffset = 0;
  uint32 HighBytesOffset = 0;
  uint32 NumVertexRefs = 0;
  uint32 NumPrevRefVerticesBeforeDwords = 0;
  uint32 NumPrevNewVerticesBeforeDwords = 0;
};

struct FCesiumRuntimeNanitePageSections {
  uint32 Cluster = 0;
  uint32 ClusterBoneInfluence = 0;
  uint32 VoxelBoneInfluence = 0;
  uint32 MaterialTable = 0;
  uint32 VertReuseBatchInfo = 0;
  uint32 BoneInfluence = 0;
  uint32 BrickData = 0;
  uint32 ExtendedData = 0;
  uint32 DecodeInfo = 0;
  uint32 Index = 0;
  uint32 Position = 0;
  uint32 Attribute = 0;

  uint32 GetMaterialTableSize() const { return Align(MaterialTable, 16u); }
  uint32 GetVertReuseBatchInfoSize() const {
    return Align(VertReuseBatchInfo, 16u);
  }
  uint32 GetDecodeInfoSize() const { return Align(DecodeInfo, 16u); }
  uint32 GetClusterBoneInfluenceSize() const {
    return Align(ClusterBoneInfluence, 16u);
  }
  uint32 GetVoxelBoneInfluenceSize() const {
    return Align(VoxelBoneInfluence, 16u);
  }
  uint32 GetBoneInfluenceSize() const {
    return Align(BoneInfluence, 16u);
  }
  uint32 GetBrickDataSize() const { return Align(BrickData, 16u); }
  uint32 GetExtendedDataSize() const {
    return Align(ExtendedData, 16u);
  }

  static uint32 GetClusterOffset() { return NANITE_GPU_PAGE_HEADER_SIZE; }
  uint32 GetClusterBoneInfluenceOffset() const {
    return GetClusterOffset() + Cluster;
  }
  uint32 GetVoxelBoneInfluenceOffset() const {
    return GetClusterBoneInfluenceOffset() + GetClusterBoneInfluenceSize();
  }
  uint32 GetMaterialTableOffset() const {
    return GetVoxelBoneInfluenceOffset() + GetVoxelBoneInfluenceSize();
  }
  uint32 GetVertReuseBatchInfoOffset() const {
    return GetMaterialTableOffset() + GetMaterialTableSize();
  }
  uint32 GetBoneInfluenceOffset() const {
    return GetVertReuseBatchInfoOffset() + GetVertReuseBatchInfoSize();
  }
  uint32 GetBrickDataOffset() const {
    return GetBoneInfluenceOffset() + GetBoneInfluenceSize();
  }
  uint32 GetExtendedDataOffset() const {
    return GetBrickDataOffset() + GetBrickDataSize();
  }
  uint32 GetDecodeInfoOffset() const {
    return GetExtendedDataOffset() + GetExtendedDataSize();
  }
  uint32 GetIndexOffset() const {
    return GetDecodeInfoOffset() + GetDecodeInfoSize();
  }
  uint32 GetPositionOffset() const { return GetIndexOffset() + Index; }
  uint32 GetAttributeOffset() const { return GetPositionOffset() + Position; }
  uint32 GetTotal() const { return GetAttributeOffset() + Attribute; }

  FCesiumRuntimeNanitePageSections GetOffsets() const;
  FCesiumRuntimeNanitePageSections&
  operator+=(const FCesiumRuntimeNanitePageSections& Other);
};

struct FCesiumRuntimeNaniteUVRange {
  FUintVector2 Min = FUintVector2::ZeroValue;
  FUintVector2 NumBits = FUintVector2::ZeroValue;
};

struct FCesiumRuntimeNanitePackedUVRange {
  FUintVector2 Data = FUintVector2::ZeroValue;
};

struct FCesiumRuntimeNaniteEncodingSettings {
  int32 PositionPrecision = 4;
  static constexpr int32 NormalBits = 8;
};

struct FCesiumRuntimeNaniteEncodingInfo {
  FCesiumRuntimeNaniteEncodingSettings Settings;
  int32 BitsPerIndex = 0;
  int32 BitsPerAttribute = 0;
  FIntVector PositionMin{ForceInit};
  FIntVector PositionBits{ForceInit};
  FColor ColorMin{ForceInit};
  FColor ColorMax{ForceInit};
  FIntVector4 ColorBits = FIntVector4(0);
  TStaticArray<FCesiumRuntimeNaniteUVRange, NANITE_MAX_UVS> UVRanges;
  TStaticArray<FUintVector2, NANITE_MAX_UVS> UVMins;
  int32 NumTextureCoordinates = 0;
  FCesiumRuntimeNanitePageSections GPUSizes;
};

struct FCesiumRuntimeNaniteCluster {
  TArray<FVector3f> Positions;
  TArray<FCesiumRuntimeNaniteOctahedron> Normals;
  TArray<FColor> Colors;
  TArray<TArray<FVector2f>> TextureCoordinates;
  TArray<uint8> Indices;
  TStaticArray<uint32, 3 * (NANITE_MAX_CLUSTER_TRIANGLES / 32)>
      StripBitmaskDWords{};
  TArray<uint8> IndexData;
  TArray<uint8> ExtendedData;

  int32 NumVertices() const { return Positions.Num(); }
  int32 NumTriangles() const {
    return Indices.Num() > 0 ? Indices.Num() / 3 : Positions.Num() / 3;
  }
};

struct FCesiumRuntimeNaniteFixupChunkHeader {
  uint16 NumClusters = 0;
  uint16 NumHierarchyFixups = 0;
  uint16 NumClusterFixups = 0;
  uint16 Pad = 0;
};

struct FCesiumRuntimeNaniteEncodedPage {
  TArray<uint8> Data;
  FCesiumRuntimeNanitePageSections GPUSizes;
};
