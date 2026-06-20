// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#pragma once

#include "CesiumRasterOverlay.h"
#include "CoreMinimal.h"
#include "HoloUrlTemplateRasterOverlay.generated.h"

UENUM(BlueprintType)
enum class EProjection : uint8 {
  WGS84 UMETA(DisplayName = "WGS84"),
  GCJ02 UMETA(DisplayName = "GCJ02")
};

UCLASS(ClassGroup = (CesiumExtend), meta = (BlueprintSpawnableComponent))
class CESIUMRUNTIME_API UHoloUrlTemplateRasterOverlay
    : public UCesiumRasterOverlay {
  GENERATED_BODY()

public:
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium")
  FString BaseUrl = "https://0pn.cn/maps/vt?lyrs=s,m&gl=CN&x={x}&y={y}&z={z}";

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium")
  EProjection Projection = EProjection::WGS84;

  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium",
      meta = (ClampMin = 64, ClampMax = 2048))
  int32 TileWidth = 256;

  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium",
      meta = (ClampMin = 64, ClampMax = 2048))
  int32 TileHeight = 256;

  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium",
      meta = (ClampMin = 0))
  int32 MinimumLevel = 0;

  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium",
      meta = (ClampMin = 0))
  int32 MaximumLevel = 18;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium")
  bool bDebug = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium")
  FString Subdoains;

protected:
  virtual std::unique_ptr<CesiumRasterOverlays::RasterOverlay> CreateOverlay(
      const CesiumRasterOverlays::RasterOverlayOptions& options = {}) override;
};
