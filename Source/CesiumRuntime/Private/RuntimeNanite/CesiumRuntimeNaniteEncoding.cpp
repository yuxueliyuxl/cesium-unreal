// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Portions derived from VoxelCore.
// Copyright Voxel Plugin SAS.
// Licensed under the MIT License; see LICENSE.VoxelCore.txt.

#include "RuntimeNanite/CesiumRuntimeNaniteEncoding.h"

namespace {

float UInt8ToFloat(uint8 Value) {
  return float(Value) / 255.0f;
}

uint8 FloatToUInt8(float Value) {
  return uint8(FMath::Clamp(FMath::RoundToInt(Value * 255.0f), 0, 255));
}

} // namespace

FCesiumRuntimeNaniteOctahedron
FCesiumRuntimeNaniteOctahedron::Encode(FVector3f UnitVector) {
  if (!UnitVector.Normalize()) {
    UnitVector = FVector3f::UpVector;
  }

  const float AbsoluteSum = FMath::Abs(UnitVector.X) +
                            FMath::Abs(UnitVector.Y) +
                            FMath::Abs(UnitVector.Z);
  UnitVector.X /= AbsoluteSum;
  UnitVector.Y /= AbsoluteSum;

  FVector2f Octahedron(UnitVector.X, UnitVector.Y);
  if (UnitVector.Z <= 0.0f) {
    Octahedron.X =
        (1.0f - FMath::Abs(UnitVector.Y)) *
        (UnitVector.X >= 0.0f ? 1.0f : -1.0f);
    Octahedron.Y =
        (1.0f - FMath::Abs(UnitVector.X)) *
        (UnitVector.Y >= 0.0f ? 1.0f : -1.0f);
  }
  Octahedron = Octahedron * 0.5f + FVector2f(0.5f, 0.5f);

  return FCesiumRuntimeNaniteOctahedron{
      FloatToUInt8(Octahedron.X),
      FloatToUInt8(Octahedron.Y)};
}

FVector3f FCesiumRuntimeNaniteOctahedron::Decode() const {
  FVector2f Octahedron(UInt8ToFloat(X), UInt8ToFloat(Y));
  Octahedron = Octahedron * 2.0f - FVector2f(1.0f, 1.0f);

  FVector3f Unit(
      Octahedron.X,
      Octahedron.Y,
      1.0f - FMath::Abs(Octahedron.X) - FMath::Abs(Octahedron.Y));
  const float T = FMath::Max(-Unit.Z, 0.0f);
  Unit.X += Unit.X >= 0.0f ? -T : T;
  Unit.Y += Unit.Y >= 0.0f ? -T : T;
  return Unit.GetSafeNormal(SMALL_NUMBER, FVector3f::UpVector);
}

bool FCesiumRuntimeNaniteOctahedron::IsValid() const {
  return !Decode().ContainsNaN();
}

uint32 CesiumRuntimeNaniteEncodeZigZag(int32 Value) {
  return (uint32(Value) << 1u) ^ uint32(Value >> 31);
}

uint32 CesiumRuntimeNaniteEncodeUVFloat(
    float Value,
    uint32 NumMantissaBits) {
  if (!FMath::IsFinite(Value)) {
    return 0;
  }

  const uint32 SignBitPosition =
      NANITE_UV_FLOAT_NUM_EXPONENT_BITS + NumMantissaBits;
  const uint32 FloatUInt = FPlatformMath::AsUInt(Value);
  const uint32 AbsoluteFloatUInt = FloatUInt & 0x7fffffffu;

  uint32 Result;
  if (AbsoluteFloatUInt < 0x3f800000u) {
    const float AbsoluteFloat = FPlatformMath::AsFloat(AbsoluteFloatUInt);
    Result =
        uint32(double(AbsoluteFloat * float(1u << NumMantissaBits)) + 0.5);
  } else {
    const uint32 Shift = 23u - NumMantissaBits;
    const uint32 Temporary =
        (AbsoluteFloatUInt - 0x3f000000u) + (1u << (Shift - 1u));
    Result =
        FMath::Min(Temporary >> Shift, (1u << SignBitPosition) - 1u);
  }

  const uint32 SignMask =
      (1u << SignBitPosition) - (FloatUInt >> 31u);
  return Result ^ SignMask;
}

FCesiumRuntimeNanitePageSections
FCesiumRuntimeNanitePageSections::GetOffsets() const {
  return FCesiumRuntimeNanitePageSections{
      GetClusterOffset(),
      GetClusterBoneInfluenceOffset(),
      GetVoxelBoneInfluenceOffset(),
      GetMaterialTableOffset(),
      GetVertReuseBatchInfoOffset(),
      GetBoneInfluenceOffset(),
      GetBrickDataOffset(),
      GetExtendedDataOffset(),
      GetDecodeInfoOffset(),
      GetIndexOffset(),
      GetPositionOffset(),
      GetAttributeOffset()};
}

FCesiumRuntimeNanitePageSections&
FCesiumRuntimeNanitePageSections::operator+=(
    const FCesiumRuntimeNanitePageSections& Other) {
  Cluster += Other.Cluster;
  ClusterBoneInfluence += Other.ClusterBoneInfluence;
  VoxelBoneInfluence += Other.VoxelBoneInfluence;
  MaterialTable += Other.MaterialTable;
  VertReuseBatchInfo += Other.VertReuseBatchInfo;
  BoneInfluence += Other.BoneInfluence;
  BrickData += Other.BrickData;
  ExtendedData += Other.ExtendedData;
  DecodeInfo += Other.DecodeInfo;
  Index += Other.Index;
  Position += Other.Position;
  Attribute += Other.Attribute;
  return *this;
}
