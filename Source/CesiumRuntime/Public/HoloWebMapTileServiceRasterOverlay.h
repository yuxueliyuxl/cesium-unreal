// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#pragma once

#include "CesiumRasterOverlay.h"
#include "CoreMinimal.h"
#include "HoloWebMapTileServiceRasterOverlay.generated.h"

UCLASS(ClassGroup = (CesiumExtend), meta = (BlueprintSpawnableComponent))
class CESIUMRUNTIME_API UHoloWebMapTileServiceRasterOverlay
    : public UCesiumRasterOverlay {
  GENERATED_BODY()

public:
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WMTSOption")
  FString BaseUrl;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WMTSOption")
  FString Key;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WMTSOption")
  bool bSpecifyZoomLevels = false;

  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "WMTSOption",
      meta = (EditCondition = "bSpecifyZoomLevels", ClampMin = 0))
  int32 MinimumLevel = 0;

  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "WMTSOption",
      meta = (EditCondition = "bSpecifyZoomLevels", ClampMin = 0))
  int32 MaximumLevel = 18;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WMTSOption")
  bool bDebug = false;

protected:
  virtual std::unique_ptr<CesiumRasterOverlays::RasterOverlay> CreateOverlay(
      const CesiumRasterOverlays::RasterOverlayOptions& options = {}) override;
};
