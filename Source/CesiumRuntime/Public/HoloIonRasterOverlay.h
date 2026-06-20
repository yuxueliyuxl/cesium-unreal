// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#pragma once

#include "CesiumRasterOverlay.h"
#include "CoreMinimal.h"
#include "HoloIonRasterOverlay.generated.h"

class UCesiumIonServer;

UENUM(BlueprintType)
enum class HoloVeserMapsType : uint8 {
  gaode_img UMETA(DisplayName = "高德影像"),
  gaode_vec UMETA(DisplayName = "高德矢量"),
  tiandidu_img_c UMETA(DisplayName = "天地图影像(CGCS2000)"),
  tiandidu_cia_c UMETA(DisplayName = "天地图影像标注(CGCS2000)"),
  tiandidu_vec_c UMETA(DisplayName = "天地图矢量(CGCS2000)"),
  tiandidu_cva_c UMETA(DisplayName = "天地图矢量标注(CGCS2000)"),
  tiandidu_ter_c UMETA(DisplayName = "天地图地形(CGCS2000)"),
  tiandidu_ibo_c UMETA(DisplayName = "天地图国界(CGCS2000)"),
  tiandidu_img_w UMETA(DisplayName = "天地图影像(WGS84)"),
  tiandidu_cia_w UMETA(DisplayName = "天地图影像标注(WGS84)"),
  tiandidu_vec_w UMETA(DisplayName = "天地图矢量(WGS84)"),
  tiandidu_cva_w UMETA(DisplayName = "天地图矢量标注(WGS84)"),
  tiandidu_ter_w UMETA(DisplayName = "天地图地形(WGS84)"),
  tiandidu_ibo_w UMETA(DisplayName = "天地图国界(WGS84)"),
  tiandidu_lc_land UMETA(DisplayName = "天地图地表覆盖"),
  tiandidu_js_vec_blue UMETA(DisplayName = "江苏天地图蓝色"),
  tiandidu_js_vec_black UMETA(DisplayName = "江苏天地图黑色"),
  tiandidu_js_vec_grey UMETA(DisplayName = "江苏天地图灰色"),
  tiandidu_js_vec UMETA(DisplayName = "江苏高清影像"),
  geovisearth_img UMETA(DisplayName = "星图影像"),
  geovisearth_vec UMETA(DisplayName = "星图矢量"),
  geovisearth_ter UMETA(DisplayName = "星图地形"),
  geovisearth_cia UMETA(DisplayName = "星图影像标注"),
  mapbox_img UMETA(DisplayName = "M.B影像"),
  mapbox_vec UMETA(DisplayName = "M.B矢量"),
  mapbox_lanuse UMETA(DisplayName = "M.B用地"),
  mapbox_color UMETA(DisplayName = "M.B彩色"),
  google_img_marker UMETA(DisplayName = "谷歌影像(GCJ02含标注)"),
  tiandidu_lc_terrain_rgb UMETA(DisplayName = "天地图山影"),
  google_img UMETA(DisplayName = "谷歌影像(WGS84)"),
  google_vec_marker UMETA(DisplayName = "谷歌矢量(GCJ02含标注)"),
  google_ter UMETA(DisplayName = "谷歌地形(GCJ02含标注)"),
  google_road_marker UMETA(DisplayName = "谷歌路网(GCJ02含标注)"),
  Customize UMETA(DisplayName = "自定义XYZ")
};

UCLASS(
    DisplayName = "Holoveser ion Raster Overlay",
    ClassGroup = (CesiumExtend),
    meta = (BlueprintSpawnableComponent))
class CESIUMRUNTIME_API UHoloIonRasterOverlay : public UCesiumRasterOverlay {
  GENERATED_BODY()

public:
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holoveser")
  int64 HoloveserIonAssetID = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holoveser")
  HoloVeserMapsType HoloveserIonMapType = HoloVeserMapsType::gaode_img;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holoveser")
  FString HoloveserIonAccessToken;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holoveser")
  bool bDebug = false;

  UPROPERTY(
      meta =
          (DeprecatedProperty,
           DeprecationMessage = "Use HoloveserIonServer instead."))
  FString IonAssetEndpointUrl_DEPRECATED;

  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Holoveser",
      AdvancedDisplay)
  UCesiumIonServer* HoloveserIonServer = nullptr;

  UFUNCTION(CallInEditor, Category = "Holoveser")
  void HoloveserMapToken();

  virtual void PostLoad() override;

protected:
  virtual std::unique_ptr<CesiumRasterOverlays::RasterOverlay> CreateOverlay(
      const CesiumRasterOverlays::RasterOverlayOptions& options = {}) override;
};
